#include <jni.h>

#include <sodium.h>

#include "checksignature.h"
#include "check_emulator.h"
#include "keys_generator.h"
#include <string.h>
#include <sys/ptrace.h>

// 获取数组的大小
#define NELEM(x) ((int)(sizeof(x) / sizeof((x)[0])))
// 指定要注册的类，对应完整的java类名
#define JNIREG_CLASS "com/androidyuan/aesjni/AESEncrypt"

const char *UNSIGNATURE = "UNSIGNATURE";

jstring char2jstring(JNIEnv *envPtr, char *src)
{
    JNIEnv env = *envPtr;

    jsize len = strlen(src);
    jclass clsstring = env->FindClass(envPtr, "java/lang/String");
    jstring strencode = env->NewStringUTF(envPtr, "UTF-8");
    jmethodID mid = env->GetMethodID(envPtr, clsstring, "<init>",
                                     "([BLjava/lang/String;)V");
    jbyteArray barr = env->NewByteArray(envPtr, len);
    env->SetByteArrayRegion(envPtr, barr, 0, len, (jbyte *)src);

    return (jstring)env->NewObject(envPtr, clsstring, mid, barr, strencode);
}

jstring char2string_with_len(JNIEnv *envPtr, char *src, size_t str_len)
{
    JNIEnv env = *envPtr;

    jsize len = (jsize)str_len;
    jclass clsstring = env->FindClass(envPtr, "java/lang/String");
    jstring strencode = env->NewStringUTF(envPtr, "UTF-8");
    jmethodID mid = env->GetMethodID(envPtr, clsstring, "<init>",
                                     "([BLjava/lang/String;)V");
    jbyteArray barr = env->NewByteArray(envPtr, len);
    env->SetByteArrayRegion(envPtr, barr, 0, len, (jbyte *)src);

    return (jstring)env->NewObject(envPtr, clsstring, mid, barr, strencode);
}

unsigned char *getNonce()
{
    const char *nonce_hex = "611dec2f53524315";
    const unsigned char *NONCE = chacha20_hexnonce2bin(nonce_hex);
    return NONCE;
}

//__attribute__((section (".mytext")))//隐藏字符表 并没有什么卵用 只是针对初阶hacker的一个小方案而已
unsigned char *getKey()
{
    const char *key_hex = "9876c42f2f61bee24cc27ebd6155897c46950a83c9b0cc95a9650f9ae7421d07";
    const unsigned char *KEY = chacha20_hexkey2bin(key_hex);
    return KEY;
}

JNIEXPORT jstring JNICALL encode(JNIEnv *env, jobject instance, jobject context, jstring str_)
{
    sodium_init();
    //先进行apk被 二次打包的校验
    //    if (check_signature(env, instance, context) != 1 || check_is_emulator(env) != 1) {
    //        char *str = UNSIGNATURE;
    //        return char2jstring(env,str);
    //    }

    const char *plain_str = (*env)->GetStringUTFChars(env, str_, JNI_FALSE);
    (*env)->ReleaseStringUTFChars(env, str_, plain_str);

    unsigned char *ciphertext;
    ciphertext = (unsigned char *)sodium_malloc(strlen(plain_str) + crypto_aead_chacha20poly1305_ABYTES);
    unsigned long long ciphertext_len;

    crypto_aead_chacha20poly1305_encrypt(ciphertext, &ciphertext_len,
                                         plain_str, strlen(plain_str),
                                         NULL, 0, //additional data is NULL, you can change it.
                                         NULL, getNonce(), getKey());
    if (ciphertext_len == 0)
    {
        abort();
    }

    char *result_hex = (char *)sodium_malloc(2 * ciphertext_len + 1); //return hex is easy to transport in internet.
    sodium_bin2hex(result_hex, (size_t)(2 * ciphertext_len + 1), ciphertext, ciphertext_len);
    return (*env)->NewStringUTF(env, result_hex);
}

JNIEXPORT jstring JNICALL decode(JNIEnv *env, jobject instance, jobject context, jstring str_)
{

    //security checking.
    //    if (check_signature(env, instance, context) != 1|| check_is_emulator(env) != 1) {
    //        char *str = UNSIGNATURE;
    //        return char2jstring(env,str);
    //    }

    //str_ must is hex.
    const char *hex_str = (*env)->GetStringUTFChars(env, str_, JNI_FALSE);
    (*env)->ReleaseStringUTFChars(env, str_, hex_str);

    int encrypt_len = strlen(hex_str) / 2;
    unsigned char *encrypted_str = (unsigned char *)sodium_malloc(encrypt_len);
    sodium_hex2bin(encrypted_str, encrypt_len,
                   hex_str, strlen(hex_str),
                   NULL, NULL, NULL);

    unsigned char *decrypted;
    decrypted = (unsigned char *)sodium_malloc(encrypt_len);
    unsigned long long decrypted_len;
    crypto_aead_chacha20poly1305_decrypt(decrypted, &decrypted_len,
                                         NULL,
                                         encrypted_str, encrypt_len,
                                         NULL, 0,
                                         getNonce(), getKey());

    //It cant use NewStringUTF, if decrypted is Garbled code ,use NewStringUTF will throw exception.
    return char2string_with_len(env, decrypted, (size_t)decrypted_len); //decrypted doesnt has '\0',so I put decrypted_len.
}

/**
 * if rerurn 1 ,is check pass.
 */
JNIEXPORT jint JNICALL
check_jni(JNIEnv *env, jobject instance, jobject con)
{
    return check_signature(env, instance, con);
}

// Java和JNI函数的绑定表
static JNINativeMethod method_table[] = {
    {"checkSignature", "(Ljava/lang/Object;)I", (void *)check_jni},
    {"decode", "(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/String;", (void *)decode},
    {"encode", "(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/String;", (void *)encode},
};

// 注册native方法到java中
static int registerNativeMethods(JNIEnv *env, const char *className,
                                 JNINativeMethod *gMethods, int numMethods)
{
    jclass clazz;
    clazz = (*env)->FindClass(env, className);
    if (clazz == NULL)
    {
        return JNI_FALSE;
    }
    if ((*env)->RegisterNatives(env, clazz, gMethods, numMethods) < 0)
    {
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

int register_ndk_load(JNIEnv *env)
{
    // 调用注册方法
    return registerNativeMethods(env, JNIREG_CLASS,
                                 method_table, NELEM(method_table));
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{

    ptrace(PTRACE_TRACEME, 0, 0, 0); //反调试
    //这是一种比较简单的防止被调试的方案
    // 有更复杂更高明的方案，比如：不用这个ptrace而是每次执行加密解密签先去判断是否被trace,目前的版本不做更多的负载方案，您想做可以fork之后，自己去做

    JNIEnv *env = NULL;
    jint result = -1;

    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_4) != JNI_OK)
    {
        return result;
    }

    register_ndk_load(env);

    // 返回jni的版本
    return JNI_VERSION_1_4;
}
