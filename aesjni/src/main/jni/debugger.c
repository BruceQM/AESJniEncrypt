//
// Created by wei on 17-12-5.
//

#include "debugger.h"
#include <stdio.h>
#include "string.h"
#include "logger.h"


/**
 * -----------
 * https://developer.android.com/ndk/guides/ndk-build#options
 * NDK_DEBUG=1 : Force a debuggable build.
 * -----------
 */

int is_debug()
{

#ifdef NDK_DEBUG //build with android.mk
    LOGI("NDK_DEBUG : %d",NDK_DEBUG);
    if(NDK_DEBUG==1){
        return 1;
    }
#endif
LOGD("NDK_DEBUG not define.");

#ifdef CMAKE_BUILD_TYPE //building with Cmakelist
    LOGI("is CMAKE_BUILD_TYPE. \n");
    return 1;
#endif
LOGD("CMAKE_BUILD_TYPE not define.");

    return 0;
}
