/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * 
 * Based on platform_android, which has
 * 
 * Copyright © 2021, Google Inc.
 * Copyright (C) 2021, GlobalLogic Ukraine
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef EGL_OHOS_INCLUDED
#define EGL_OHOS_INCLUDED

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <GL/internal/dri_interface.h>

#include "egl_dri2.h"
#include "display_type.h"
#include "window.h"

#define ANativeWindow NativeWindow
#define ANativeWindowBuffer NativeWindowBuffer
static inline void
ANativeWindow_acquire(struct ANativeWindow *window)
{
   NativeObjectReference(window);
}

static inline void
ANativeWindow_release(struct ANativeWindow *window)
{
   NativeObjectReference(window);
}

static inline int32_t
ANativeWindow_getFormat(struct ANativeWindow *window)
{
   int32_t format = PIXEL_FMT_RGBA_8888;
   int32_t res = NativeWindowHandleOpt(window, GET_FORMAT, &format);
   return res == 0 ? format : res;
}

static inline int32_t
ANativeWindow_dequeueBuffer(struct ANativeWindow *window,
                            struct ANativeWindowBuffer **buffer,
                            int *fenceFd)
{
   return NativeWindowRequestBuffer(window, buffer, fenceFd);
}

static inline int32_t
ANativeWindow_queueBuffer(struct ANativeWindow *window,
                          struct ANativeWindowBuffer *buffer,
                          int fenceFd)
{
   struct Region dirty;
   dirty.rectNumber = 0;
   dirty.rects = NULL;
   return NativeWindowFlushBuffer(window, buffer, fenceFd, dirty);
}

static inline int32_t
ANativeWindow_cancelBuffer(struct ANativeWindow *window,
                           struct ANativeWindowBuffer *buffer,
                           int fenceFd)
{
   return NativeWindowCancelBuffer(window, buffer);
}

static inline int32_t
ANativeWindow_setUsage(struct ANativeWindow *window, uint64_t usage)
{
   return NativeWindowHandleOpt(window, SET_USAGE, usage);
}

struct buffer_info {
   int width;
   int height;
   uint32_t drm_fourcc;
   int num_planes;
   int fds[4];
   uint64_t modifier;
   int offsets[4];
   int pitches[4];
   enum __DRIYUVColorSpace yuv_color_space;
   enum __DRISampleRange sample_range;
   enum __DRIChromaSiting horizontal_siting;
   enum __DRIChromaSiting vertical_siting;
};

#endif /* EGL_OHOS_INCLUDED */
