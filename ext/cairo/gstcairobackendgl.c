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

/* FIXME: work with gles */

#ifdef USE_CAIRO_GL
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#define CAIROSINK_USE_PBO
#elif USE_CAIRO_GLESV2
#include <GLES2/gl2.h>
#endif

#include <gst/gst.h>

#include <cairo-gl.h>

#include "gstcairogldebug.h"
#include "gstcairobackend.h"
#include "gstglmemory.h"

GST_DEBUG_CATEGORY_EXTERN (gst_cairo_sink_debug_category);
#define GST_CAT_DEFAULT gst_cairo_sink_debug_category

static cairo_surface_t *_gl_create_surface (GstCairoBackend * backend,
    cairo_device_t * device, GstMemory * mem);

static void _gl_destroy_surface (cairo_surface_t * surface);

static void _gl_show (cairo_surface_t * surface);

static gboolean _gl_get_size (cairo_surface_t * surface, gint * width,
    gint * height);

GstCairoBackend gst_cairo_backend_gl = {
  _gl_create_surface,
  _gl_destroy_surface,
  _gl_show,
  _gl_get_size,
  GST_CAIRO_BACKEND_GL
};

static void
_gl_show (cairo_surface_t * surface)
{
  gl_debug ("swapping buffers");

  /* XXX: forcing pending drawing.
   * Calling cairo_surface_flush () on cairo_gl_surface does 
   * not resolve multisampling on the current surface, application
   * must call glBlitFrameBuffer if it is supported and the surface
   * is texture based, or it must call glDisable (GL_MULTISAMPLE) if
   * application would like to use this surface with direct GL calls.
   * if application only uses cairo API, then cairo takes care of
   * resolving multisampling. */
  cairo_surface_flush (surface);

  cairo_gl_surface_swapbuffers (surface);
}

static cairo_surface_t *
_gl_create_surface (GstCairoBackend * backend, cairo_device_t * device,
    GstMemory * mem)
{
  cairo_surface_t *surface;
  GstGLMemory *glmem = (GstGLMemory *) mem;

  gl_debug ("GL: create surface");

  /* not sure whether we really need to cairo_device_acquire()+_release()
   * here, but cairo_gl_surface_create() seems to do it when it creates its
   * texture  */

  surface = cairo_gl_surface_create_for_texture (device, CAIRO_CONTENT_COLOR,
      glmem->texture, glmem->width, glmem->height);
  gl_debug ("exit");
  return surface;
}

static void
_gl_destroy_surface (cairo_surface_t * surface)
{
  gl_debug ("GL: destroy surface");
  cairo_surface_destroy (surface);
  gl_debug ("exit");
}

static gboolean
_gl_get_size (cairo_surface_t * surface, gint * width, gint * height)
{
  int _width, _height;
  _width = cairo_gl_surface_get_width (surface);
  _height = cairo_gl_surface_get_height (surface);

  if (!_width || !_height)
    return FALSE;

  *width = _width;
  *height = _height;
  return TRUE;
}
