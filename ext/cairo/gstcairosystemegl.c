/*
 * Copyright (C) 2013 Samsung Electronics Corporation. All rights reserved.
 *   @author: Guillaume Emont <guijemont@igalia.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 * 2.  Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.

 * THIS SOFTWARE IS PROVIDED BY SAMSUNG ELECTRONICS CORPORATION AND ITS
 * CONTRIBUTORS "AS IS", AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SAMSUNG
 * ELECTRONICS CORPORATION OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS, OR BUSINESS INTERRUPTION), HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING
 * NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <EGL/egl.h>

#include <cairo-gl.h>
#include <gst/gst.h>
#include <X11/Xlib.h>

#include "gstcairosystem.h"

static cairo_surface_t *_egl_create_display_surface (gint width, gint height);

static gboolean _egl_query_can_map (cairo_surface_t * surface);

static void _egl_dispose (void);

typedef struct
{
  GstCairoSystem parent;
  Display *display;
  Window display_window;
  gint display_window_width;
  gint display_window_height;
} GstCairoSystemEGL;

GstCairoSystemEGL gst_cairo_system_egl = {
  {
        _egl_create_display_surface,
        _egl_query_can_map,
        _egl_dispose,
      GST_CAIRO_SYSTEM_EGL},
  NULL,                         /* display */
  None,                         /* display_window */
  0,                            /* display_window_width */
  0                             /* display_window_height */
};

static EGLint multisampleAttributes[] = {
  EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
  EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
  EGL_RED_SIZE, 1,              /* the minmal number of bits per component    */
  EGL_GREEN_SIZE, 1,
  EGL_BLUE_SIZE, 1,
  EGL_STENCIL_SIZE, 1,
  EGL_SAMPLES, 4,
  EGL_SAMPLE_BUFFERS, 1,
  EGL_NONE
};

static EGLint singleSampleAttributes[] = {
  EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
  EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
  EGL_RED_SIZE, 1,              /* the minmal number of bits per component    */
  EGL_GREEN_SIZE, 1,
  EGL_BLUE_SIZE, 1,
  EGL_STENCIL_SIZE, 1,
  EGL_NONE
};

static void
create_display (GstCairoSystemEGL * system)
{
  if (system->display)
    return;

  system->display = XOpenDisplay (NULL);
  if (!system->display)
    GST_ERROR ("Could not open display.");
}

static void
create_display_window (GstCairoSystemEGL * system, gint width, gint height)
{
  if (!system->display)
    return;

  if (width == system->display_window_width
      && height == system->display_window_height
      && system->display_window != None)
    /* nothing to create, we already have a display window matching width and
     * height */
    return;

  if (system->display_window) {
    XUnmapWindow (system->display, system->display_window);
    XDestroyWindow (system->display, system->display_window);
  }

  system->display_window = XCreateSimpleWindow (system->display,
      DefaultRootWindow (system->display), 0, 0, width, height, 0, 0, 0);

  system->display_window_width = width;
  system->display_window_height = height;
}

/* Mostly cut and paste from glx-utils.c in cairo-gl-smoke-tests */
static cairo_surface_t *
_egl_create_display_surface (gint width, gint height)
{
  GstCairoSystemEGL *system = &gst_cairo_system_egl;

  EGLDisplay egl_display;
  EGLContext egl_context;
  EGLSurface egl_surface;
  EGLConfig egl_config;
  EGLint major, minor;
  EGLint num;

  EGLint ctx_attrs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };

  cairo_device_t *cairo_device;
  cairo_surface_t *cairo_surface;

  if (system->display)
    /* We already created display and surface and don't keep track of the
     * surface. */
    return NULL;

  create_display (system);

  /* Create the XWindow. */
  create_display_window (system, width, height);

  egl_display = eglGetDisplay ((EGLNativeDisplayType) system->display);
  if (egl_display == EGL_NO_DISPLAY)
    goto CLEANUP_X;

  if (!eglInitialize (egl_display, &major, &minor))
    goto CLEANUP_DISPLAY;

  if (!eglBindAPI (EGL_OPENGL_ES_API))
    goto CLEANUP_DISPLAY;

  if (!eglChooseConfig (egl_display, multisampleAttributes, &egl_config,
          1, &num) || num == 0) {
    if (!eglChooseConfig (egl_display, singleSampleAttributes, &egl_config,
            1, &num))
      goto CLEANUP_DISPLAY;
  }

  if (num != 1)
    goto CLEANUP_DISPLAY;

  egl_context = eglCreateContext (egl_display, egl_config, NULL, ctx_attrs);
  if (egl_context == EGL_NO_CONTEXT)
    goto CLEANUP_DISPLAY;

  egl_surface = eglCreateWindowSurface (egl_display, egl_config,
      (NativeWindowType) system->display_window, NULL);
  if (egl_surface == EGL_NO_SURFACE)
    goto CLEANUP_CONTEXT;

  XMapWindow (system->display, system->display_window);
  XFlush (system->display);

  cairo_device = cairo_egl_device_create (egl_display, egl_context);
  cairo_surface = cairo_gl_surface_create_for_egl (cairo_device, egl_surface,
      width, height);

  return cairo_surface;

CLEANUP_CONTEXT:
  eglDestroyContext (egl_display, egl_context);
CLEANUP_DISPLAY:
  eglTerminate (egl_display);
CLEANUP_X:
  GST_WARNING ("Could not initialise EGL context correctly");
  XDestroyWindow (system->display, system->display_window);
  XCloseDisplay (system->display);
  return NULL;
}

static gboolean
_egl_query_can_map (cairo_surface_t * surface)
{
  /* map not implemented yet */
  return FALSE;
}

void
_egl_dispose (void)
{
  GstCairoSystemEGL *system = &gst_cairo_system_egl;

  if (!system->display)
    return;

  if (system->display_window != None) {
    XUnmapWindow (system->display, system->display_window);
    XDestroyWindow (system->display, system->display_window);
  }

  XCloseDisplay (system->display);
}
