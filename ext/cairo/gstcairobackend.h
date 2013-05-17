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
#ifndef _GST_CAIRO_BACKEND_H_
#define _GST_CAIRO_BACKEND_H_

#include <cairo.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/gstmemory.h>

typedef struct _GstCairoBackend GstCairoBackend;

/**
  A GstCairoBackend handles all the things that are specific to a surface (e.g.
  GL or Xlib) type but not to the system (e.g. EGL or GLX).
 */

typedef enum
{
  GST_CAIRO_BACKEND_GL = 1,
} GstCairoBackendType;
#define GST_CAIRO_BACKEND_LAST 1

struct _GstCairoBackend
{
  cairo_surface_t *(*create_surface) (GstCairoBackend * backend,
      cairo_device_t * device, GstMemory * mem);
  void (*destroy_surface) (cairo_surface_t * surface);
  void (*show) (cairo_surface_t * surface);
  gboolean (*get_size) (cairo_surface_t *surface, gint *width, gint *height);

  GstCairoBackendType backend_type;
};

#endif /* _GST_CAIRO_BACKEND_H_ */
