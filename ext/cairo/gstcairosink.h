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

#ifndef _GST_CAIRO_SINK_H_
#define _GST_CAIRO_SINK_H_

#include <gst/video/gstvideosink.h>
#include <gst/base/gstdataqueue.h>

#include <cairo.h>
#include <cairo-gobject.h>
#include "gstcairobackend.h"

G_BEGIN_DECLS
#define GST_TYPE_CAIRO_SINK   (gst_cairo_sink_get_type())
#define GST_CAIRO_SINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CAIRO_SINK,GstCairoSink))
#define GST_CAIRO_SINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CAIRO_SINK,GstCairoSinkClass))
#define GST_IS_CAIRO_SINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CAIRO_SINK))
#define GST_IS_CAIRO_SINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CAIRO_SINK))
typedef struct _GstCairoSink GstCairoSink;
typedef struct _GstCairoSinkClass GstCairoSinkClass;

typedef struct _GstCairoMemory GstCairoMemory;

typedef struct _GstCairoAllocator GstCairoAllocator;

struct _GstCairoSink
{
  GstVideoSink base_cairosink;

  GstPad *sinkpad;
  GstCaps *caps;

  GstCairoBackendType backend_type;
  GstCairoBackend *backend;
  GMainContext *render_main_context;
  GThread *thread;
  GMainLoop *loop;
  GSource *source;
  GstDataQueue *queue;

  GMutex render_mutex;
  GCond render_cond;
  cairo_surface_t *surface;
  cairo_device_t *device;
  GstFlowReturn last_ret;
  GstMiniObject *last_finished_operation;

  GstCairoAllocator *allocator;
  GstBufferPool *buffer_pool;
};

struct _GstCairoSinkClass
{
  GstVideoSinkClass base_cairosink_class;
};

struct _GstCairoMemory
{
  GstMemory parent;

  GstCairoBackend *backend;

  cairo_surface_t *surface;
  GstCairoBackendSurfaceInfo *surface_info;
};

GType gst_cairo_sink_get_type (void);

G_END_DECLS
#endif
