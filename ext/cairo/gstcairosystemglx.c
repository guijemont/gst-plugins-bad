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

#include <string.h>
#include <stdlib.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/glx.h>
#include <cairo-gl.h>
#include "gstcairosystem.h"
#include "gstcairogldebug.h"

static cairo_surface_t *_glx_create_display_surface (gint width, gint height);

static gboolean _glx_query_can_map (cairo_surface_t * surface);

static void _glx_dispose (void);

typedef struct
{
  GstCairoSystem parent;
  Display *display;
  Window display_window;
  gint display_window_width;
  gint display_window_height;
} GstCairoSystemGLX;

GstCairoSystemGLX gst_cairo_system_glx = {
  {_glx_create_display_surface,
        _glx_query_can_map,
        _glx_dispose,
      GST_CAIRO_SYSTEM_GLX},
  NULL
};

#define GL_VERSION_ENCODE(major, minor) ( \
  ((major) * 256) + ((minor) * 1))

static int multisampleAttributes[] = {
  GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
  GLX_RENDER_TYPE, GLX_RGBA_BIT,
  GLX_DOUBLEBUFFER, True,       /* Request a double-buffered color buffer with */
  GLX_RED_SIZE, 1,              /* the maximum number of bits per component    */
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_STENCIL_SIZE, 1,
  GLX_SAMPLES, 4,
  GLX_SAMPLE_BUFFERS, 1,
  None
};

static int singleSampleAttributes[] = {
  GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
  GLX_RENDER_TYPE, GLX_RGBA_BIT,
  GLX_DOUBLEBUFFER, True,       /* Request a double-buffered color buffer with */
  GLX_RED_SIZE, 1,              /* the maximum number of bits per component    */
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_STENCIL_SIZE, 1,
  None
};

/* copy from cairo */
static int
get_gl_version (void)
{
  int major, minor;
  const char *version = (const char *) glGetString (GL_VERSION);
  const char *dot = version == NULL ? NULL : strchr (version, '.');
  const char *major_start = dot;

  if (dot == NULL || dot == version || *(dot + 1) == '\0') {
    major = 0;
    minor = 0;
  } else {
    while (major_start > version && *major_start != ' ')
      --major_start;
    major = strtol (major_start, NULL, 10);
    minor = strtol (dot + 1, NULL, 10);
  }

  return GL_VERSION_ENCODE (major, minor);
}

static void
create_display (GstCairoSystemGLX * system)
{
  if (system->display)
    return;

  system->display = XOpenDisplay (NULL);
  if (!system->display)
    GST_ERROR ("Could not open display.");
}

static void
create_display_window (GstCairoSystemGLX * system, gint width, gint height)
{
  Window root_window;
  XVisualInfo *visual_info;
  XSetWindowAttributes window_attributes;
  GLXFBConfig *fb_configs;

  int num_returned = 0;

  gl_debug ("GLX: creating display window and surface");

  if (!system->display)
    create_display (system);

  if (!system->display)
    return;

  /* if request width, height match, we keep the existing window */
  if (system->display_window_width == width
      && system->display_window_height == height
      && system->display_window != None)
    return;

  if (system->display_window) {
    XUnmapWindow (system->display, system->display_window);
    XDestroyWindow (system->display, system->display_window);
  }

  fb_configs =
      glXChooseFBConfig (system->display, DefaultScreen (system->display),
      multisampleAttributes, &num_returned);
  if (fb_configs == NULL) {
    fb_configs =
        glXChooseFBConfig (system->display, DefaultScreen (system->display),
        singleSampleAttributes, &num_returned);
  }

  if (fb_configs == NULL) {
    GST_ERROR ("Unable to create a GL context with appropriate attributes.");
    /* FIXME: leak? */
    return;
  }

  visual_info = glXGetVisualFromFBConfig (system->display, fb_configs[0]);
  root_window = RootWindow (system->display, visual_info->screen);

  window_attributes.border_pixel = 0;
  window_attributes.event_mask = StructureNotifyMask;
  window_attributes.colormap =
      XCreateColormap (system->display, root_window, visual_info->visual,
      AllocNone);

  /* Create the XWindow. */
  system->display_window = XCreateWindow (system->display, root_window, 0, 0,
      width, height, 0, visual_info->depth, InputOutput, visual_info->visual,
      CWBorderPixel | CWColormap | CWEventMask, &window_attributes);

  XFree (visual_info);

  system->display_window_width = width;
  system->display_window_height = height;
}

static Bool
waitForNotify (Display * dpy, XEvent * event, XPointer arg)
{
  return (event->type == MapNotify) && (event->xmap.window == (Window) arg);
}

/* Mostly cut and paste from glx-utils.c in cairo-gl-smoke-tests */
static cairo_surface_t *
_glx_create_display_surface (gint width, gint height)
{
  XEvent event;
  GLXContext glx_context;
  GLXFBConfig *fb_configs;
  GstCairoSystemGLX *system = &gst_cairo_system_glx;

  cairo_device_t *device;
  cairo_surface_t *surface;
  int num_returned = 0;

  gl_debug ("GLX: creating display window and surface");

  if (!system->display)
    create_display (system);

  if (!system->display)
    return NULL;

  fb_configs =
      glXChooseFBConfig (system->display, DefaultScreen (system->display),
      multisampleAttributes, &num_returned);
  if (fb_configs == NULL) {
    fb_configs =
        glXChooseFBConfig (system->display, DefaultScreen (system->display),
        singleSampleAttributes, &num_returned);
  }

  if (fb_configs == NULL) {
    GST_ERROR ("Unable to create a GL context with appropriate attributes.");
    /* FIXME: leak? */
    return NULL;
  }

  /* create window if necessary */
  create_display_window (system, width, height);

  /* Create a GLX context for OpenGL rendering */
  glx_context =
      glXCreateNewContext (system->display, fb_configs[0], GLX_RGBA_TYPE, NULL,
      True);

  /* Map the window to the screen, and wait for it to appear */
  XMapWindow (system->display, system->display_window);
  XIfEvent (system->display, &event, waitForNotify,
      (XPointer) system->display_window);

  device = cairo_glx_device_create (system->display, glx_context);
  surface = cairo_gl_surface_create_for_window (device,
      system->display_window, width, height);

  gl_debug ("exit");
  return surface;
}

static gboolean
_glx_query_can_map (cairo_surface_t * surface)
{
  GLXContext glx_context;
  int gl_version;

  Window window, root_window;
  XVisualInfo *visual_info;
  XSetWindowAttributes window_attributes;
  GLXFBConfig *fb_configs;
  GstCairoSystemGLX *system = &gst_cairo_system_glx;

  int num_returned = 0;

  if (g_getenv ("GSTCAIROSINK_DISABLE_MAP") != NULL)
    return FALSE;

  if (surface) {
    Display *display;

    cairo_device_t *device = cairo_surface_get_device (surface);
    display = cairo_glx_device_get_display (device);
    glx_context = cairo_glx_device_get_context (device);

    if (!display || !glx_context)
      return FALSE;

    cairo_device_acquire (device);
    gl_version = get_gl_version ();
    cairo_device_release (device);
    if (gl_version >= GL_VERSION_ENCODE (1, 5))
      return TRUE;

    return FALSE;
  }

  if (!system->display)
    create_display (system);

  if (!system->display)
    return FALSE;

  fb_configs =
      glXChooseFBConfig (system->display, DefaultScreen (system->display),
      singleSampleAttributes, &num_returned);

  if (fb_configs == NULL) {
    GST_ERROR ("Unable to create a GL context with appropriate attributes.");
    /* FIXME: leak? */
    return FALSE;
  }

  visual_info = glXGetVisualFromFBConfig (system->display, fb_configs[0]);
  root_window = RootWindow (system->display, visual_info->screen);

  window_attributes.border_pixel = 0;
  window_attributes.event_mask = StructureNotifyMask;
  window_attributes.colormap =
      XCreateColormap (system->display, root_window, visual_info->visual,
      AllocNone);

  /* Create the XWindow. */
  window = XCreateWindow (system->display, root_window, 0, 0, 1, 1,
      0, visual_info->depth, InputOutput, visual_info->visual,
      CWBorderPixel | CWColormap | CWEventMask, &window_attributes);

  glx_context =
      glXCreateNewContext (system->display, fb_configs[0], GLX_RGBA_TYPE, NULL,
      True);
  XFree (visual_info);

  if (!glx_context) {
    GST_ERROR ("Unable to create a GL context.");
    /* FIXME: leak? */
    return FALSE;
  }

  /* switch to current context */
  glXMakeCurrent (system->display, window, glx_context);
  gl_version = get_gl_version ();

  /* cleanup */
  glXMakeCurrent (system->display, None, None);
  XDestroyWindow (system->display, window);
  glXDestroyContext (system->display, glx_context);
  XSync (system->display, True);

  if (gl_version >= GL_VERSION_ENCODE (1, 5))
    return TRUE;

  return FALSE;
}

void
_glx_dispose (void)
{
  GstCairoSystemGLX *system = &gst_cairo_system_glx;

  if (!system->display)
    return;

  if (system->display_window != None) {
    XUnmapWindow (system->display, system->display_window);
    XDestroyWindow (system->display, system->display_window);
  }

  XCloseDisplay (system->display);
}
