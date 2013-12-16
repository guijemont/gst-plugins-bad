/* GStreamer
 * Copyright (C) 2013 FIXME <fixme@example.com>
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

G_BEGIN_DECLS

#define GST_TYPE_CAIRO_SINK   (gst_cairo_sink_get_type())
#define GST_CAIRO_SINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CAIRO_SINK,GstCairoSink))
#define GST_CAIRO_SINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CAIRO_SINK,GstCairoSinkClass))
#define GST_IS_CAIRO_SINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CAIRO_SINK))
#define GST_IS_CAIRO_SINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CAIRO_SINK))

typedef struct _GstCairoSink GstCairoSink;
typedef struct _GstCairoSinkClass GstCairoSinkClass;

struct _GstCairoSink
{
  GstVideoSink base_cairosink;

  GstPad *sinkpad;
};

struct _GstCairoSinkClass
{
  GstVideoSinkClass base_cairosink_class;
};

GType gst_cairo_sink_get_type (void);

G_END_DECLS

#endif
