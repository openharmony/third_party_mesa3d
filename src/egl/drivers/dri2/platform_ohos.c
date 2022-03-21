/*
 * Mesa 3-D graphics library
 *
 * Copyright (c) 2021 Huawei Device Co., Ltd. *
 * 
 * Based on platform_android, which has
 * 
 * Copyright (C) 2010-2011 Chia-I Wu <olvaffe@gmail.com>
 * Copyright (C) 2010-2011 LunarG Inc.
 *
 * Based on platform_x11, which has
 *
 * Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <xf86drm.h>
#include <stdbool.h>
#include <stdio.h>
//#include <sync/sync.h>
#include <sys/types.h>
#include <drm-uapi/drm_fourcc.h>

#include "util/compiler.h"
#include "util/os_file.h"

#include "loader.h"
#include "egl_dri2.h"
#include "platform_ohos.h"
#include "libsync.h"
#ifdef HAVE_DRM_GRALLOC
#include <gralloc_drm_handle.h>
#include "gralloc_drm.h"
#endif /* HAVE_DRM_GRALLOC */

#define ALIGN(val, align)	(((val) + (align) - 1) & ~((align) - 1))

static int
get_format_bpp(int native)
{
   int bpp;

   switch (native) {
   case PIXEL_FMT_RGBA_8888:
   case PIXEL_FMT_RGBX_8888:
   case PIXEL_FMT_BGRA_8888:
      bpp = 4;
      break;
   case PIXEL_FMT_RGB_565:
      bpp = 2;
      break;
   default:
      bpp = 0;
      break;
   }

   return bpp;
}

/* returns # of fds, and by reference the actual fds */
static unsigned
get_native_buffer_fds(struct ANativeWindowBuffer *buf, int fds[3])
{
   BufferHandle* handle = GetBufferHandleFromNative(buf);
   if (handle == NULL) {
      return 0;
   }

   fds[0] = handle->fd;
   return 1;
}

/* createImageFromFds requires fourcc format */
static int get_fourcc(int native)
{
   switch (native) {
   case PIXEL_FMT_RGB_565:   return DRM_FORMAT_RGB565;
   case PIXEL_FMT_BGRA_8888: return DRM_FORMAT_ARGB8888;
   case PIXEL_FMT_RGBA_8888: return DRM_FORMAT_ABGR8888;
   case PIXEL_FMT_RGBX_8888: return DRM_FORMAT_XBGR8888;
   default:
      _eglLog(_EGL_WARNING, "unsupported native buffer format 0x%x", native);
   }
   return -1;
}

static int
native_window_buffer_get_buffer_info(struct dri2_egl_display *dri2_dpy,
                                     struct ANativeWindowBuffer *buf,
                                     struct buffer_info *out_buf_info)
{
   int num_planes = 0;
   int drm_fourcc = 0;
   int pitch = 0;
   int fds[3];
   /*
    * Non-YUV formats could *also* have multiple planes, such as ancillary
    * color compression state buffer, but the rest of the code isn't ready
    * yet to deal with modifiers:
    */
   num_planes = get_native_buffer_fds(buf, fds);
   if (num_planes == 0)
      return -EINVAL;

   assert(num_planes == 1);
   BufferHandle* bufferHandle;
   bufferHandle = GetBufferHandleFromNative(buf);
   drm_fourcc = get_fourcc(bufferHandle->format);
   if (drm_fourcc == -1) {
      _eglError(EGL_BAD_PARAMETER, "eglCreateEGLImageKHR");
      return -EINVAL;
   }
   pitch = bufferHandle->stride;
   if (pitch == 0) {
      _eglError(EGL_BAD_PARAMETER, "eglCreateEGLImageKHR");
      return -EINVAL;
   }

   *out_buf_info = (struct buffer_info){
      .width = bufferHandle->width,
      .height = bufferHandle->height,
      .drm_fourcc = drm_fourcc,
      .num_planes = num_planes,
      .fds = { fds[0], -1, -1, -1 },
      .modifier = DRM_FORMAT_MOD_INVALID,
      .offsets = { 0, 0, 0, 0 },
      .pitches = { pitch, 0, 0, 0 },
      .yuv_color_space = EGL_ITU_REC601_EXT,
      .sample_range = EGL_YUV_NARROW_RANGE_EXT,
      .horizontal_siting = EGL_YUV_CHROMA_SITING_0_EXT,
      .vertical_siting = EGL_YUV_CHROMA_SITING_0_EXT,
   };

   return 0;
}


static __DRIimage *
ohos_create_image_from_buffer_info(struct dri2_egl_display *dri2_dpy,
                                    struct buffer_info *buf_info,
                                    void *priv)
{
   unsigned error;

   if (dri2_dpy->image->base.version >= 15 &&
       dri2_dpy->image->createImageFromDmaBufs2 != NULL) {
      return dri2_dpy->image->createImageFromDmaBufs2(
         dri2_dpy->dri_screen, buf_info->width, buf_info->height,
         buf_info->drm_fourcc, buf_info->modifier, buf_info->fds,
         buf_info->num_planes, buf_info->pitches, buf_info->offsets,
         buf_info->yuv_color_space, buf_info->sample_range,
         buf_info->horizontal_siting, buf_info->vertical_siting, &error,
         priv);
   }

   return dri2_dpy->image->createImageFromDmaBufs(
      dri2_dpy->dri_screen, buf_info->width, buf_info->height,
      buf_info->drm_fourcc, buf_info->fds, buf_info->num_planes,
      buf_info->pitches, buf_info->offsets, buf_info->yuv_color_space,
      buf_info->sample_range, buf_info->horizontal_siting,
      buf_info->vertical_siting, &error, priv);
}

static __DRIimage *
ohos_create_image_from_native_buffer(_EGLDisplay *disp,
                                      struct ANativeWindowBuffer *buf,
                                      void *priv)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct buffer_info buf_info;
   __DRIimage *img = NULL;

   /* If dri driver is gallium virgl, real modifier info queried back from
    * CrOS info (and potentially mapper metadata if integrated later) cannot
    * get resolved and the buffer import will fail. Thus the fallback behavior
    * is preserved down to native_window_buffer_get_buffer_info() so that the
    * buffer can be imported without modifier info as a last resort.
    */

   if (!native_window_buffer_get_buffer_info(dri2_dpy, buf, &buf_info)) {
      img = ohos_create_image_from_buffer_info(dri2_dpy, &buf_info, priv);
   }

   return img;
}

static EGLBoolean
ohos_window_dequeue_buffer(struct dri2_egl_surface *dri2_surf)
{
   int fence_fd;

   if (ANativeWindow_dequeueBuffer(dri2_surf->window, &dri2_surf->buffer,
                                   &fence_fd))
      return EGL_FALSE;

   /* If access to the buffer is controlled by a sync fence, then block on the
    * fence.
    *
    * It may be more performant to postpone blocking until there is an
    * immediate need to write to the buffer. But doing so would require adding
    * hooks to the DRI2 loader.
    *
    * From the ANativeWindow_dequeueBuffer documentation:
    *
    *    The libsync fence file descriptor returned in the int pointed to by
    *    the fenceFd argument will refer to the fence that must signal
    *    before the dequeued buffer may be written to.  A value of -1
    *    indicates that the caller may access the buffer immediately without
    *    waiting on a fence.  If a valid file descriptor is returned (i.e.
    *    any value except -1) then the caller is responsible for closing the
    *    file descriptor.
    */
    if (fence_fd >= 0) {
       /* From the SYNC_IOC_WAIT documentation in <linux/sync.h>:
        *
        *    Waits indefinitely if timeout < 0.
        */
        int timeout = -1;
        sync_wait(fence_fd, timeout);
        close(fence_fd);
   }
   /* Record all the buffers created by ANativeWindow and update back buffer
    * for updating buffer's age in swap_buffers.
    */
   EGLBoolean updated = EGL_FALSE;
   for (int i = 0; i < dri2_surf->color_buffers_count; i++) {
      if (!dri2_surf->color_buffers[i].buffer) {
         dri2_surf->color_buffers[i].buffer = dri2_surf->buffer;
      }
      if (dri2_surf->color_buffers[i].buffer == dri2_surf->buffer) {
         dri2_surf->back = &dri2_surf->color_buffers[i];
         updated = EGL_TRUE;
         break;
      }
   }

   if (!updated) {
      /* In case of all the buffers were recreated by ANativeWindow, reset
       * the color_buffers
       */
      for (int i = 0; i < dri2_surf->color_buffers_count; i++) {
         dri2_surf->color_buffers[i].buffer = NULL;
         dri2_surf->color_buffers[i].age = 0;
      }
      dri2_surf->color_buffers[0].buffer = dri2_surf->buffer;
      dri2_surf->back = &dri2_surf->color_buffers[0];
   }

   return EGL_TRUE;
}

static EGLBoolean
ohos_window_enqueue_buffer(_EGLDisplay *disp, struct dri2_egl_surface *dri2_surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);

   /* To avoid blocking other EGL calls, release the display mutex before
    * we enter ohos_window_enqueue_buffer() and re-acquire the mutex upon
    * return.
    */
   mtx_unlock(&disp->Mutex);

   /* Queue the buffer with stored out fence fd. The ANativeWindow or buffer
    * consumer may choose to wait for the fence to signal before accessing
    * it. If fence fd value is -1, buffer can be accessed by consumer
    * immediately. Consumer or application shouldn't rely on timestamp
    * associated with fence if the fence fd is -1.
    *
    * Ownership of fd is transferred to consumer after queueBuffer and the
    * consumer is responsible for closing it. Caller must not use the fd
    * after passing it to queueBuffer.
    */
   int fence_fd = dri2_surf->out_fence_fd;
   dri2_surf->out_fence_fd = -1;
   ANativeWindow_queueBuffer(dri2_surf->window, dri2_surf->buffer, fence_fd);

   dri2_surf->buffer = NULL;
   dri2_surf->back = NULL;

   mtx_lock(&disp->Mutex);

   if (dri2_surf->dri_image_back) {
      dri2_dpy->image->destroyImage(dri2_surf->dri_image_back);
      dri2_surf->dri_image_back = NULL;
   }

   return EGL_TRUE;
}

static void
ohos_window_cancel_buffer(struct dri2_egl_surface *dri2_surf)
{
   int ret;
   int fence_fd = dri2_surf->out_fence_fd;

   dri2_surf->out_fence_fd = -1;
   ret = ANativeWindow_cancelBuffer(dri2_surf->window, dri2_surf->buffer,
                                    fence_fd);
   dri2_surf->buffer = NULL;
   if (ret < 0) {
      _eglLog(_EGL_WARNING, "ANativeWindow_cancelBuffer failed");
      dri2_surf->base.Lost = EGL_TRUE;
   }
}

static _EGLSurface *
ohos_create_surface(_EGLDisplay *disp, EGLint type, _EGLConfig *conf,
                     void *native_window, const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri2_egl_surface *dri2_surf;
   struct ANativeWindow *window = native_window;
   const __DRIconfig *config;

   dri2_surf = calloc(1, sizeof *dri2_surf);
   if (!dri2_surf) {
      _eglError(EGL_BAD_ALLOC, "ohos_create_surface");
      return NULL;
   }

   if (!dri2_init_surface(&dri2_surf->base, disp, type, conf, attrib_list,
                          true, native_window))
      goto cleanup_surface;

   if (type == EGL_WINDOW_BIT) {
      int format;
      int buffer_count;

      format = ANativeWindow_getFormat(window);
      if (format < 0) {
         _eglError(EGL_BAD_NATIVE_WINDOW, "ohos_create_surface");
         goto cleanup_surface;
      }

  
      /* Required buffer caching slots. */
      buffer_count = 3; // default use 3 buffer

      dri2_surf->color_buffers = calloc(buffer_count,
                                        sizeof(*dri2_surf->color_buffers));
      if (!dri2_surf->color_buffers) {
         _eglError(EGL_BAD_ALLOC, "ohos_create_surface");
         goto cleanup_surface;
      }
      dri2_surf->color_buffers_count = buffer_count;

      if (format != dri2_conf->base.NativeVisualID) {
         _eglLog(_EGL_WARNING, "Native format mismatch: 0x%x != 0x%x",
               format, dri2_conf->base.NativeVisualID);
      }

      NativeWindowHandleOpt(window, GET_BUFFER_GEOMETRY, &dri2_surf->base.Height, &dri2_surf->base.Width);
   }

   config = dri2_get_dri_config(dri2_conf, type,
                                dri2_surf->base.GLColorspace);
   if (!config) {
      _eglError(EGL_BAD_MATCH, "Unsupported surfacetype/colorspace configuration");
      goto cleanup_surface;
   }

   if (!dri2_create_drawable(dri2_dpy, config, dri2_surf, dri2_surf))
      goto cleanup_surface;

   if (window) {
      ANativeWindow_acquire(window);
      dri2_surf->window = window;
   }

   return &dri2_surf->base;

cleanup_surface:
   if (dri2_surf->color_buffers_count)
      free(dri2_surf->color_buffers);
   free(dri2_surf);

   return NULL;
}

static _EGLSurface *
ohos_create_window_surface(_EGLDisplay *disp, _EGLConfig *conf,
                            void *native_window, const EGLint *attrib_list)
{
   return ohos_create_surface(disp, EGL_WINDOW_BIT, conf,
                               native_window, attrib_list);
}

static _EGLSurface *
ohos_create_pbuffer_surface(_EGLDisplay *disp, _EGLConfig *conf,
                             const EGLint *attrib_list)
{
   return ohos_create_surface(disp, EGL_PBUFFER_BIT, conf,
                               NULL, attrib_list);
}

static EGLBoolean
ohos_destroy_surface(_EGLDisplay *disp, _EGLSurface *surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);

   dri2_egl_surface_free_local_buffers(dri2_surf);

   if (dri2_surf->base.Type == EGL_WINDOW_BIT) {
      if (dri2_surf->buffer)
         ohos_window_cancel_buffer(dri2_surf);

      ANativeWindow_release(dri2_surf->window);
   }

   if (dri2_surf->dri_image_back) {
      _eglLog(_EGL_DEBUG, "%s : %d : destroy dri_image_back", __func__, __LINE__);
      dri2_dpy->image->destroyImage(dri2_surf->dri_image_back);
      dri2_surf->dri_image_back = NULL;
   }

   if (dri2_surf->dri_image_front) {
      _eglLog(_EGL_DEBUG, "%s : %d : destroy dri_image_front", __func__, __LINE__);
      dri2_dpy->image->destroyImage(dri2_surf->dri_image_front);
      dri2_surf->dri_image_front = NULL;
   }

   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);

   dri2_fini_surface(surf);
   free(dri2_surf->color_buffers);
   free(dri2_surf);

   return EGL_TRUE;
}

static int
update_buffers(struct dri2_egl_surface *dri2_surf)
{
   if (dri2_surf->base.Lost)
      return -1;

   if (dri2_surf->base.Type != EGL_WINDOW_BIT)
      return 0;

   /* try to dequeue the next back buffer */
   if (!dri2_surf->buffer && !ohos_window_dequeue_buffer(dri2_surf)) {
      _eglLog(_EGL_WARNING, "Could not dequeue buffer from native window");
      dri2_surf->base.Lost = EGL_TRUE;
      return -1;
   }
   BufferHandle* handle = GetBufferHandleFromNative(dri2_surf->buffer);
   /* free outdated buffers and update the surface size */
   if (dri2_surf->base.Width != handle->width ||
       dri2_surf->base.Height != handle->height) {
      dri2_egl_surface_free_local_buffers(dri2_surf);
      dri2_surf->base.Width = handle->width;
      dri2_surf->base.Height = handle->height;
   }

   return 0;
}

static int
get_front_bo(struct dri2_egl_surface *dri2_surf, unsigned int format)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);

   if (dri2_surf->dri_image_front)
      return 0;

   if (dri2_surf->base.Type == EGL_WINDOW_BIT) {
      /* According current EGL spec, front buffer rendering
       * for window surface is not supported now.
       * and mesa doesn't have the implementation of this case.
       * Add warning message, but not treat it as error.
       */
      _eglLog(_EGL_DEBUG, "DRI driver requested unsupported front buffer for window surface");
   } else if (dri2_surf->base.Type == EGL_PBUFFER_BIT) {
      dri2_surf->dri_image_front =
          dri2_dpy->image->createImage(dri2_dpy->dri_screen,
                                              dri2_surf->base.Width,
                                              dri2_surf->base.Height,
                                              format,
                                              0,
                                              NULL);
      if (!dri2_surf->dri_image_front) {
         _eglLog(_EGL_WARNING, "dri2_image_front allocation failed");
         return -1;
      }
   }

   return 0;
}

static int
get_back_bo(struct dri2_egl_surface *dri2_surf)
{
   _EGLDisplay *disp = dri2_surf->base.Resource.Display;

   if (dri2_surf->dri_image_back)
      return 0;

   if (dri2_surf->base.Type == EGL_WINDOW_BIT) {
      if (!dri2_surf->buffer) {
         _eglLog(_EGL_WARNING, "Could not get native buffer");
         return -1;
      }

      dri2_surf->dri_image_back =
         ohos_create_image_from_native_buffer(disp, dri2_surf->buffer, NULL);
      if (!dri2_surf->dri_image_back) {
         _eglLog(_EGL_WARNING, "failed to create DRI image from FD");
         return -1;
      }
   } else if (dri2_surf->base.Type == EGL_PBUFFER_BIT) {
      /* The EGL 1.5 spec states that pbuffers are single-buffered. Specifically,
       * the spec states that they have a back buffer but no front buffer, in
       * contrast to pixmaps, which have a front buffer but no back buffer.
       *
       * Single-buffered surfaces with no front buffer confuse Mesa; so we deviate
       * from the spec, following the precedent of Mesa's EGL X11 platform. The
       * X11 platform correctly assigns pbuffers to single-buffered configs, but
       * assigns the pbuffer a front buffer instead of a back buffer.
       *
       * Pbuffers in the X11 platform mostly work today, so let's just copy its
       * behavior instead of trying to fix (and hence potentially breaking) the
       * world.
       */
      _eglLog(_EGL_DEBUG, "DRI driver requested unsupported back buffer for pbuffer surface");
   }

   return 0;
}

/* Some drivers will pass multiple bits in buffer_mask.
 * For such case, will go through all the bits, and
 * will not return error when unsupported buffer is requested, only
 * return error when the allocation for supported buffer failed.
 */
static int
ohos_image_get_buffers(__DRIdrawable *driDrawable,
                  unsigned int format,
                  uint32_t *stamp,
                  void *loaderPrivate,
                  uint32_t buffer_mask,
                  struct __DRIimageList *images)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;

   images->image_mask = 0;
   images->front = NULL;
   images->back = NULL;

   if (update_buffers(dri2_surf) < 0)
      return 0;

   if (_eglSurfaceInSharedBufferMode(&dri2_surf->base)) {
      if (get_back_bo(dri2_surf) < 0)
         return 0;

      /* We have dri_image_back because this is a window surface and
       * get_back_bo() succeeded.
       */
      assert(dri2_surf->dri_image_back);
      images->back = dri2_surf->dri_image_back;
      images->image_mask |= __DRI_IMAGE_BUFFER_SHARED;

      /* There exists no accompanying back nor front buffer. */
      return 1;
   }

   if (buffer_mask & __DRI_IMAGE_BUFFER_FRONT) {
      if (get_front_bo(dri2_surf, format) < 0)
         return 0;

      if (dri2_surf->dri_image_front) {
         images->front = dri2_surf->dri_image_front;
         images->image_mask |= __DRI_IMAGE_BUFFER_FRONT;
      }
   }

   if (buffer_mask & __DRI_IMAGE_BUFFER_BACK) {
      if (get_back_bo(dri2_surf) < 0)
         return 0;

      if (dri2_surf->dri_image_back) {
         images->back = dri2_surf->dri_image_back;
         images->image_mask |= __DRI_IMAGE_BUFFER_BACK;
      }
   }

   return 1;
}

static EGLint
ohos_query_buffer_age(_EGLDisplay *disp, _EGLSurface *surface)
{
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surface);

   if (update_buffers(dri2_surf) < 0) {
      _eglError(EGL_BAD_ALLOC, "ohos_query_buffer_age");
      return -1;
   }

   return dri2_surf->back ? dri2_surf->back->age : 0;
}

static EGLBoolean
ohos_swap_buffers(_EGLDisplay *disp, _EGLSurface *draw)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(draw);
   const bool has_mutable_rb = _eglSurfaceHasMutableRenderBuffer(draw);

   /* From the EGL_KHR_mutable_render_buffer spec (v12):
    *
    *    If surface is a single-buffered window, pixmap, or pbuffer surface
    *    for which there is no pending change to the EGL_RENDER_BUFFER
    *    attribute, eglSwapBuffers has no effect.
    */
   if (has_mutable_rb &&
       draw->RequestedRenderBuffer == EGL_SINGLE_BUFFER &&
       draw->ActiveRenderBuffer == EGL_SINGLE_BUFFER) {
      _eglLog(_EGL_DEBUG, "%s: remain in shared buffer mode", __func__);
      return EGL_TRUE;
   }

   for (int i = 0; i < dri2_surf->color_buffers_count; i++) {
      if (dri2_surf->color_buffers[i].age > 0)
         dri2_surf->color_buffers[i].age++;
   }

   /* "XXX: we don't use get_back_bo() since it causes regressions in
    * several dEQP tests.
    */
   if (dri2_surf->back)
      dri2_surf->back->age = 1;

   dri2_flush_drawable_for_swapbuffers(disp, draw);

   /* dri2_surf->buffer can be null even when no error has occured. For
    * example, if the user has called no GL rendering commands since the
    * previous eglSwapBuffers, then the driver may have not triggered
    * a callback to ANativeWindow_dequeueBuffer, in which case
    * dri2_surf->buffer remains null.
    */
   if (dri2_surf->buffer)
      ohos_window_enqueue_buffer(disp, dri2_surf);

   dri2_dpy->flush->invalidate(dri2_surf->dri_drawable);

   return EGL_TRUE;
}

static EGLBoolean
ohos_query_surface(_EGLDisplay *disp, _EGLSurface *surf,
                    EGLint attribute, EGLint *value)
{
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);
   EGLint dummy;
   switch (attribute) {
      case EGL_WIDTH:
         if (dri2_surf->base.Type == EGL_WINDOW_BIT && dri2_surf->window) {
            NativeWindowHandleOpt(dri2_surf->window,
                                GET_BUFFER_GEOMETRY, &dummy, value);
            return EGL_TRUE;
         }
         break;
      case EGL_HEIGHT:
         if (dri2_surf->base.Type == EGL_WINDOW_BIT && dri2_surf->window) {
            NativeWindowHandleOpt(dri2_surf->window,
                                GET_BUFFER_GEOMETRY, value, &dummy);
            return EGL_TRUE;
         }
         break;
      default:
         break;
   }
   return _eglQuerySurface(disp, surf, attribute, value);
}

static _EGLImage *
dri2_create_image_ohos_native_buffer(_EGLDisplay *disp,
                                       _EGLContext *ctx,
                                       struct ANativeWindowBuffer *buf)
{
   if (ctx != NULL) {
      /* From the EGL_OHOS_image_native_buffer spec:
       *
       *     * If <target> is EGL_NATIVE_BUFFER_OHOS and <ctx> is not
       *       EGL_NO_CONTEXT, the error EGL_BAD_CONTEXT is generated.
       */
      _eglError(EGL_BAD_CONTEXT, "eglCreateEGLImageKHR: for "
                "EGL_NATIVE_BUFFER_OHOS, the context must be "
                "EGL_NO_CONTEXT");
      return NULL;
   }
   __DRIimage *dri_image =
      ohos_create_image_from_native_buffer(disp, buf, buf);

   if (dri_image) {
      return dri2_create_image_from_dri(disp, dri_image);
   }

   return NULL;
}

static _EGLImage *
ohos_create_image_khr(_EGLDisplay *disp, _EGLContext *ctx, EGLenum target,
                       EGLClientBuffer buffer, const EGLint *attr_list)
{
   switch (target) {
   case EGL_NATIVE_BUFFER_OHOS:
      return dri2_create_image_ohos_native_buffer(disp, ctx,
            (struct ANativeWindowBuffer *) buffer);
   default:
      return dri2_create_image_khr(disp, ctx, target, buffer, attr_list);
   }
}

static void
ohos_flush_front_buffer(__DRIdrawable * driDrawable, void *loaderPrivate)
{
}

#ifdef HAVE_DRM_GRALLOC
static int
ohos_get_buffers_parse_attachments(struct dri2_egl_surface *dri2_surf,
                                    unsigned int *attachments, int count)
{
   int num_buffers = 0;

   /* fill dri2_surf->buffers */
   for (int i = 0; i < count * 2; i += 2) {
      __DRIbuffer *buf, *local;

      assert(num_buffers < ARRAY_SIZE(dri2_surf->buffers));
      buf = &dri2_surf->buffers[num_buffers];

      switch (attachments[i]) {
      case __DRI_BUFFER_BACK_LEFT:
         if (dri2_surf->base.Type == EGL_WINDOW_BIT) {
            buf->attachment = attachments[i];
            buf->name = get_native_buffer_name(dri2_surf->buffer);
            buf->cpp = get_format_bpp(dri2_surf->buffer->format);
            buf->pitch = dri2_surf->buffer->stride * buf->cpp;
            buf->flags = 0;

            if (buf->name)
               num_buffers++;

            break;
         }
         FALLTHROUGH; /* for pbuffers */
      case __DRI_BUFFER_DEPTH:
      case __DRI_BUFFER_STENCIL:
      case __DRI_BUFFER_ACCUM:
      case __DRI_BUFFER_DEPTH_STENCIL:
      case __DRI_BUFFER_HIZ:
         local = dri2_egl_surface_alloc_local_buffer(dri2_surf,
               attachments[i], attachments[i + 1]);

         if (local) {
            *buf = *local;
            num_buffers++;
         }
         break;
      case __DRI_BUFFER_FRONT_LEFT:
      case __DRI_BUFFER_FRONT_RIGHT:
      case __DRI_BUFFER_FAKE_FRONT_LEFT:
      case __DRI_BUFFER_FAKE_FRONT_RIGHT:
      case __DRI_BUFFER_BACK_RIGHT:
      default:
         /* no front or right buffers */
         break;
      }
   }

   return num_buffers;
}

static __DRIbuffer *
ohos_get_buffers_with_format(__DRIdrawable * driDrawable,
			     int *width, int *height,
			     unsigned int *attachments, int count,
			     int *out_count, void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;

   if (update_buffers(dri2_surf) < 0)
      return NULL;

   *out_count = ohos_get_buffers_parse_attachments(dri2_surf, attachments, count);

   if (width)
      *width = dri2_surf->base.Width;
   if (height)
      *height = dri2_surf->base.Height;

   return dri2_surf->buffers;
}
#endif /* HAVE_DRM_GRALLOC */

static unsigned
ohos_get_capability(void *loaderPrivate, enum dri_loader_cap cap)
{
   /* Note: loaderPrivate is _EGLDisplay* */
   switch (cap) {
   case DRI_LOADER_CAP_RGBA_ORDERING:
      return 1;
   default:
      return 0;
   }
}

static void
ohos_destroy_loader_image_state(void *loaderPrivate)
{
}

static EGLBoolean
ohos_add_configs_for_visuals(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   static const struct {
      int format;
      int rgba_shifts[4];
      unsigned int rgba_sizes[4];
   } visuals[] = {
      { PIXEL_FMT_RGBA_8888, { 0, 8, 16, 24 }, { 8, 8, 8, 8 } },
      { PIXEL_FMT_RGBX_8888, { 0, 8, 16, -1 }, { 8, 8, 8, 0 } },
      { PIXEL_FMT_RGB_565,   { 11, 5, 0, -1 }, { 5, 6, 5, 0 } },
      /* This must be after PIXEL_FMT_RGBA_8888, we only keep BGRA
       * visual if it turns out RGBA visual is not available.
       */
      { PIXEL_FMT_BGRA_8888, { 16, 8, 0, 24 }, { 8, 8, 8, 8 } },
   };

   unsigned int format_count[ARRAY_SIZE(visuals)] = { 0 };
   int config_count = 0;

   bool has_rgba = false;
   for (int i = 0; i < ARRAY_SIZE(visuals); i++) {
      if (visuals[i].format == PIXEL_FMT_BGRA_8888 && has_rgba)
         continue;
      for (int j = 0; dri2_dpy->driver_configs[j]; j++) {
         const EGLint surface_type = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;

         const EGLint config_attrs[] = {
           EGL_NATIVE_VISUAL_ID,   visuals[i].format,
           EGL_NATIVE_VISUAL_TYPE, visuals[i].format,
           EGL_NONE
         };

         struct dri2_egl_config *dri2_conf =
            dri2_add_config(disp, dri2_dpy->driver_configs[j],
                            config_count + 1, surface_type, config_attrs,
                            visuals[i].rgba_shifts, visuals[i].rgba_sizes);
         if (dri2_conf) {
            if (dri2_conf->base.ConfigID == config_count + 1)
               config_count++;
            format_count[i]++;
         }
      }
      if (visuals[i].format == PIXEL_FMT_RGBA_8888 && format_count[i])
         has_rgba = true;
   }

   for (int i = 0; i < ARRAY_SIZE(format_count); i++) {
      if (!format_count[i]) {
         _eglLog(_EGL_DEBUG, "No DRI config supports native format 0x%x",
                 visuals[i].format);
      }
   }

   return (config_count != 0);
}

static const struct dri2_egl_display_vtbl ohos_display_vtbl = {
   .authenticate = NULL,
   .create_window_surface = ohos_create_window_surface,
   .create_pbuffer_surface = ohos_create_pbuffer_surface,
   .destroy_surface = ohos_destroy_surface,
   .create_image = ohos_create_image_khr,
   .swap_buffers = ohos_swap_buffers,
   .swap_interval = NULL,
   .query_buffer_age = ohos_query_buffer_age,
   .query_surface = ohos_query_surface,
   .get_dri_drawable = dri2_surface_get_dri_drawable,
};

static const __DRIimageLoaderExtension ohos_image_loader_extension = {
   .base = { __DRI_IMAGE_LOADER, 4 },

   .getBuffers               = ohos_image_get_buffers,
   .flushFrontBuffer         = ohos_flush_front_buffer,
   .getCapability            = ohos_get_capability,
   .flushSwapBuffers         = NULL,
   .destroyLoaderImageState  = ohos_destroy_loader_image_state,
};

static void
ohos_display_shared_buffer(__DRIdrawable *driDrawable, int fence_fd,
                            void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   struct ANativeWindowBuffer *old_buffer UNUSED = dri2_surf->buffer;

   if (!_eglSurfaceInSharedBufferMode(&dri2_surf->base)) {
      _eglLog(_EGL_WARNING, "%s: internal error: buffer is not shared",
              __func__);
      return;
   }

   if (fence_fd >= 0) {
      /* The driver's fence is more recent than the surface's out fence, if it
       * exists at all. So use the driver's fence.
       */
      if (dri2_surf->out_fence_fd >= 0) {
         close(dri2_surf->out_fence_fd);
         dri2_surf->out_fence_fd = -1;
      }
   } else if (dri2_surf->out_fence_fd >= 0) {
      fence_fd = dri2_surf->out_fence_fd;
      dri2_surf->out_fence_fd = -1;
   }

   if (ANativeWindow_queueBuffer(dri2_surf->window, dri2_surf->buffer,
                                 fence_fd)) {
      _eglLog(_EGL_WARNING, "%s: ANativeWindow_queueBuffer failed", __func__);
      close(fence_fd);
      return;
   }

   fence_fd = -1;

   if (ANativeWindow_dequeueBuffer(dri2_surf->window, &dri2_surf->buffer,
                                   &fence_fd)) {
      /* Tear down the surface because it no longer has a back buffer. */
      struct dri2_egl_display *dri2_dpy =
         dri2_egl_display(dri2_surf->base.Resource.Display);

      _eglLog(_EGL_WARNING, "%s: ANativeWindow_dequeueBuffer failed", __func__);

      dri2_surf->base.Lost = true;
      dri2_surf->buffer = NULL;
      dri2_surf->back = NULL;

      if (dri2_surf->dri_image_back) {
         dri2_dpy->image->destroyImage(dri2_surf->dri_image_back);
         dri2_surf->dri_image_back = NULL;
      }

      dri2_dpy->flush->invalidate(dri2_surf->dri_drawable);
      return;
   }

   if (fence_fd < 0)
      return;

   /* Access to the buffer is controlled by a sync fence. Block on it.
    *
    * Ideally, we would submit the fence to the driver, and the driver would
    * postpone command execution until it signalled. But DRI lacks API for
    * that (as of 2018-04-11).
    *
    *  SYNC_IOC_WAIT waits forever if timeout < 0
    */
   sync_wait(fence_fd, -1);
   close(fence_fd);
}

static const __DRImutableRenderBufferLoaderExtension ohos_mutable_render_buffer_extension = {
   .base = { __DRI_MUTABLE_RENDER_BUFFER_LOADER, 1 },
   .displaySharedBuffer = ohos_display_shared_buffer,
};

static const __DRIextension *ohos_image_loader_extensions[] = {
   &ohos_image_loader_extension.base,
   &image_lookup_extension.base,
   &use_invalidate.base,
   &ohos_mutable_render_buffer_extension.base,
   NULL,
};

static EGLBoolean
ohos_load_driver(_EGLDisplay *disp, bool swrast)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);

   dri2_dpy->driver_name = loader_get_driver_for_fd(dri2_dpy->fd);
   if (dri2_dpy->driver_name == NULL)
      return false;

#ifdef HAVE_DRM_GRALLOC
   /* Handle control nodes using __DRI_DRI2_LOADER extension and GEM names
    * for backwards compatibility with drm_gralloc. (Do not use on new
    * systems.) */
   dri2_dpy->loader_extensions = ohos_dri2_loader_extensions;
   if (!dri2_load_driver(disp)) {
      goto error;
   }
#else
   if (swrast) {
      /* Use kms swrast only with vgem / virtio_gpu.
       * virtio-gpu fallbacks to software rendering when 3D features
       * are unavailable since 6c5ab.
       */
      if (strcmp(dri2_dpy->driver_name, "vgem") == 0 ||
          strcmp(dri2_dpy->driver_name, "virtio_gpu") == 0) {
         free(dri2_dpy->driver_name);
         dri2_dpy->driver_name = strdup("kms_swrast");
      } else {
         goto error;
      }
   }

   dri2_dpy->loader_extensions = ohos_image_loader_extensions;
   if (!dri2_load_driver_dri3(disp)) {
      goto error;
   }
#endif

   return true;

error:
   free(dri2_dpy->driver_name);
   dri2_dpy->driver_name = NULL;
   return false;
}

static void
ohos_unload_driver(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);

   dlclose(dri2_dpy->driver);
   dri2_dpy->driver = NULL;
   free(dri2_dpy->driver_name);
   dri2_dpy->driver_name = NULL;
}

static int
ohos_filter_device(_EGLDisplay *disp, int fd, const char *vendor)
{
   drmVersionPtr ver = drmGetVersion(fd);
   if (!ver)
      return -1;

   if (strcmp(vendor, ver->name) != 0) {
      drmFreeVersion(ver);
      return -1;
   }

   drmFreeVersion(ver);
   return 0;
}

static EGLBoolean
ohos_probe_device(_EGLDisplay *disp, bool swrast)
{
  /* Check that the device is supported, by attempting to:
   * - load the dri module
   * - and, create a screen
   */
   if (!ohos_load_driver(disp, swrast))
      return EGL_FALSE;

   if (!dri2_create_screen(disp)) {
      _eglLog(_EGL_WARNING, "DRI2: failed to create screen");
      ohos_unload_driver(disp);
      return EGL_FALSE;
   }
   return EGL_TRUE;
}

#ifdef HAVE_DRM_GRALLOC
static EGLBoolean
ohos_open_device(_EGLDisplay *disp, bool swrast)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   int fd = -1, err = -EINVAL;

   if (swrast)
      return EGL_FALSE;

   if (dri2_dpy->gralloc->perform)
      err = dri2_dpy->gralloc->perform(dri2_dpy->gralloc,
                                       GRALLOC_MODULE_PERFORM_GET_DRM_FD,
                                       &fd);
   if (err || fd < 0) {
      _eglLog(_EGL_WARNING, "fail to get drm fd");
      return EGL_FALSE;
   }

   dri2_dpy->fd = os_dupfd_cloexec(fd);
   if (dri2_dpy->fd < 0)
      return EGL_FALSE;

   if (drmGetNodeTypeFromFd(dri2_dpy->fd) == DRM_NODE_RENDER)
      return EGL_FALSE;

   return ohos_probe_device(disp, swrast);
}
#else
static EGLBoolean
ohos_open_device(_EGLDisplay *disp, bool swrast)
{
#define MAX_DRM_DEVICES 64
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   drmDevicePtr device, devices[MAX_DRM_DEVICES] = { NULL };
   int num_devices;

   char *vendor_name = NULL;
   // char vendor_buf[PROPERTY_VALUE_MAX];

#ifdef EGL_FORCE_RENDERNODE
   const unsigned node_type = DRM_NODE_RENDER;
#else
   const unsigned node_type = swrast ? DRM_NODE_PRIMARY : DRM_NODE_RENDER;
#endif

   // if (property_get("drm.gpu.vendor_name", vendor_buf, NULL) > 0)
   //   vendor_name = vendor_buf;

   num_devices = drmGetDevices2(0, devices, ARRAY_SIZE(devices));
   if (num_devices < 0)
      return EGL_FALSE;

   for (int i = 0; i < num_devices; i++) {
      device = devices[i];

      if (!(device->available_nodes & (1 << node_type)))
         continue;

      dri2_dpy->fd = loader_open_device(device->nodes[node_type]);
      if (dri2_dpy->fd < 0) {
         _eglLog(_EGL_WARNING, "%s() Failed to open DRM device %s",
                 __func__, device->nodes[node_type]);
         continue;
      }

      /* If a vendor is explicitly provided, we use only that.
       * Otherwise we fall-back the first device that is supported.
       */
      if (vendor_name) {
         if (ohos_filter_device(disp, dri2_dpy->fd, vendor_name)) {
            /* Device does not match - try next device */
            close(dri2_dpy->fd);
            dri2_dpy->fd = -1;
            continue;
         }
         /* If the requested device matches - use it. Regardless if
          * init fails, do not fall-back to any other device.
          */
         if (!ohos_probe_device(disp, false)) {
            close(dri2_dpy->fd);
            dri2_dpy->fd = -1;
         }

         break;
      }
      if (ohos_probe_device(disp, swrast))
         break;

      /* No explicit request - attempt the next device */
      close(dri2_dpy->fd);
      dri2_dpy->fd = -1;
   }
   drmFreeDevices(devices, num_devices);

   if (dri2_dpy->fd < 0) {
      _eglLog(_EGL_WARNING, "Failed to open %s DRM device",
            vendor_name ? "desired": "any");
      return EGL_FALSE;
   }

   return EGL_TRUE;
#undef MAX_DRM_DEVICES
}

#endif

EGLBoolean
dri2_initialize_ohos(_EGLDisplay *disp)
{
   _EGLDevice *dev;
   bool device_opened = false;
   struct dri2_egl_display *dri2_dpy;
   const char *err;

   dri2_dpy = calloc(1, sizeof(*dri2_dpy));
   if (!dri2_dpy)
      return _eglError(EGL_BAD_ALLOC, "eglInitialize");

   dri2_dpy->fd = -1;

   disp->DriverData = (void *) dri2_dpy;
   device_opened = ohos_open_device(disp, disp->Options.ForceSoftware);

   if (!device_opened) {
      err = "DRI2: failed to open device";
      goto cleanup;
   }

   dev = _eglAddDevice(dri2_dpy->fd, false);
   if (!dev) {
      err = "DRI2: failed to find EGLDevice";
      goto cleanup;
   }

   disp->Device = dev;

   if (!dri2_setup_extensions(disp)) {
      err = "DRI2: failed to setup extensions";
      goto cleanup;
   }

   dri2_setup_screen(disp);

   dri2_setup_swap_interval(disp, 1);

   disp->Extensions.KHR_image = EGL_TRUE;

   /* Create configs *after* enabling extensions because presence of DRI
    * driver extensions can affect the capabilities of EGLConfigs.
    */
   if (!ohos_add_configs_for_visuals(disp)) {
      err = "DRI2: failed to add configs";
      goto cleanup;
   }

   /* Fill vtbl last to prevent accidentally calling virtual function during
    * initialization.
    */
   dri2_dpy->vtbl = &ohos_display_vtbl;

   return EGL_TRUE;

cleanup:
   dri2_display_destroy(disp);
   return _eglError(EGL_NOT_INITIALIZED, err);
}
