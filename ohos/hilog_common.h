/*
* Copyright (c) 2022 Huawei Device Co., Ltd.

* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef HILOG_COMMON_H
#define HILOG_COMMON_H

#ifndef HIVIEWDFX_HILOG_C_H
#define HIVIEWDFX_HILOG_C_H
#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOG_DOMAIN
#define LOG_DOMAIN 0
#endif

#ifndef LOG_TAG
#define LOG_TAG NULL
#endif

typedef enum {
    LOG_TYPE_MIN = 0,
    LOG_APP = 0,
    LOG_INIT = 1,
    LOG_CORE = 3,
    LOG_TYPE_MAX
} LogType;

typedef enum {
    /** Debug level to be used by {@link HILOG_DEBUG} */
    LOG_DEBUG = 3,
    /** Informational level to be used by {@link HILOG_INFO} */
    LOG_INFORMATION = 4,
    /** Warning level to be used by {@link HILOG_WARN} */
    LOG_WARN = 5,
    /** Error level to be used by {@link HILOG_ERROR} */
    LOG_ERROR = 6,
    /** Fatal level to be used by {@link HILOG_FATAL} */
    LOG_FATAL = 7,
    LOG_LEVEL_MAX
} LogLevel;

int HiLogPrint(LogType type, LogLevel level, unsigned int domain, const char *tag, const char *fmt, ...)
    __attribute__((__format__(os_log, 5, 6)));

#ifndef USE_MUSL
#define HILOG_DEBUG(type, ...) ((void)HiLogPrint((type), LOG_DEBUG, LOG_DOMAIN, LOG_TAG, __VA_ARGS__))
#else
#define HILOG_DEBUG(type, ...)
#endif

#ifndef USE_MUSL
#define HILOG_INFO(type, ...) ((void)HiLogPrint((type), LOG_INFORMATION, LOG_DOMAIN, LOG_TAG, __VA_ARGS__))
#else
#define HILOG_INFO(type, ...)
#endif

#ifndef USE_MUSL
#define HILOG_WARN(type, ...) ((void)HiLogPrint((type), LOG_WARN, LOG_DOMAIN, LOG_TAG, __VA_ARGS__))
#else
#define HILOG_WARN(type, ...)
#endif

#ifndef USE_MUSL
#define HILOG_ERROR(type, ...) ((void)HiLogPrint((type), LOG_ERROR, LOG_DOMAIN, LOG_TAG, __VA_ARGS__))
#else
#define HILOG_ERROR(type, ...)
#endif

#ifndef USE_MUSL
#define HILOG_FATAL(type, ...) ((void)HiLogPrint((type), LOG_FATAL, LOG_DOMAIN, LOG_TAG, __VA_ARGS__))
#else
#define HILOG_FATAL(type, ...)
#endif

bool HiLogIsLoggable(unsigned int domain, const char *tag, LogLevel level);

#ifdef __cplusplus
}
#endif
/** @} */
#endif  // HIVIEWDFX_HILOG_C_H



#include <string.h>
#include <stdint.h>
#include "stdio.h"
#ifdef HDF_LOG_TAG
#undef HDF_LOG_TAG
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#undef LOG_TAG
#undef LOG_DOMAIN
#define LOG_TAG "GPU_MESA"
#define LOG_DOMAIN 0xD001400

#ifndef DISPLAY_UNUSED
#define DISPLAY_UNUSED(x) (void)x
#endif

#define __FILENAME__ (strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/') + 1) : __FILE__)

#ifndef DISPLAY_LOGD
#define DISPLAY_LOGD(format, ...)                                                                                      \
    do {                                                                                                               \
        HILOG_DEBUG(LOG_CORE, "[%{public}s@%{public}s:%{public}d] " format "\n", __FUNCTION__, __FILENAME__, __LINE__, \
            ##__VA_ARGS__);                                                                                            \
    } while (0)
#endif

#ifndef DISPLAY_LOGI
#define DISPLAY_LOGI(format, ...)                                                                                     \
    do {                                                                                                              \
        HILOG_INFO(LOG_CORE, "[%{public}s@%{public}s:%{public}d] " format "\n", __FUNCTION__, __FILENAME__, __LINE__, \
            ##__VA_ARGS__);                                                                                           \
    } while (0)
#endif

#ifndef DISPLAY_LOGW
#define DISPLAY_LOGW(format, ...)                                                                                     \
    do {                                                                                                              \
        HILOG_WARN(LOG_CORE, "[%{public}s@%{public}s:%{public}d] " format "\n", __FUNCTION__, __FILENAME__, __LINE__, \
            ##__VA_ARGS__);                                                                                           \
    } while (0)
#endif

#ifndef DISPLAY_LOGE
#define DISPLAY_LOGE(format, ...)                                 \
    do {                                                          \
        HILOG_ERROR(LOG_CORE,                                     \
            "\033[0;32;31m"                                       \
            "[%{public}s@%{public}s:%{public}d] " format "\033[m" \
            "\n",                                                 \
            __FUNCTION__, __FILENAME__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#endif

#ifndef CHECK_NULLPOINTER_RETURN_VALUE
#define CHECK_NULLPOINTER_RETURN_VALUE(pointer, ret)          \
    do {                                                      \
        if ((pointer) == NULL) {                              \
            DISPLAY_LOGE("pointer is null and return ret\n"); \
            return (ret);                                     \
        }                                                     \
    } while (0)
#endif

#ifndef CHECK_NULLPOINTER_RETURN
#define CHECK_NULLPOINTER_RETURN(pointer)                 \
    do {                                                  \
        if ((pointer) == NULL) {                          \
            DISPLAY_LOGE("pointer is null and return\n"); \
            return;                                       \
        }                                                 \
    } while (0)
#endif

#ifndef DISPLAY_CHK_RETURN
#define DISPLAY_CHK_RETURN(val, ret, ...) \
    do {                                  \
        if (val) {                        \
            __VA_ARGS__;                  \
            return (ret);                 \
        }                                 \
    } while (0)
#endif

#ifndef DISPLAY_CHK_RETURN_NOT_VALUE
#define DISPLAY_CHK_RETURN_NOT_VALUE(val, ret, ...) \
    do {                                            \
        if (val) {                                  \
            __VA_ARGS__;                            \
            return;                                 \
        }                                           \
    } while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* DISP_COMMON_H */