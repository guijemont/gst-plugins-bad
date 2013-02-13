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
/**
 * SECTION:element-gstcairosink
 *
 * The cairosink element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v videotestsrc ! cairosink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include "gstcairosink.h"
#include "gstcairobackend.h"

#include <cairo-gl.h>

#define GST_CAT_DEFAULT gst_cairo_sink_debug_category

/* prototypes */

static void gst_cairo_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_cairo_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_cairo_sink_dispose (GObject * object);
static void gst_cairo_sink_finalize (GObject * object);

static gboolean gst_cairo_sink_start (GstBaseSink * base_sink);
static gboolean gst_cairo_sink_stop (GstBaseSink * base_sink);
static gboolean gst_cairo_sink_set_caps (GstBaseSink * sink, GstCaps * caps);

static GstFlowReturn
gst_cairo_sink_show_frame (GstVideoSink * video_sink, GstBuffer * buf);



enum
{
  PROP_0,
  PROP_CAIRO_SURFACE,
  PROP_CAIRO_BACKEND,
  PROP_MAIN_CONTEXT
};

/* FIXME: default to xlib if GLX not available */
#define GST_CAIRO_BACKEND_DEFAULT GST_CAIRO_BACKEND_GLX

#define GST_CAIRO_BACKEND_TYPE (gst_cairo_backend_get_type())
static GType
gst_cairo_backend_get_type (void)
{
  static GType backend_type = 0;

  static const GEnumValue backend_values[] = {
    {GST_CAIRO_BACKEND_GLX, "Use OpenGL and GLX", "glx"},
    {GST_CAIRO_BACKEND_XLIB, "Use Xlib", "xlib"},
    {0, NULL, NULL}
  };

  if (!backend_type)
    backend_type =
        g_enum_register_static ("GstCairoBackendType", backend_values);

  return backend_type;
}

/* pad templates */

static GstStaticPadTemplate gst_cairo_sink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/unknown")
    );


/* class initialization */

#define parent_class gst_cairo_sink_parent_class
G_DEFINE_TYPE (GstCairoSink, gst_cairo_sink, GST_TYPE_CAIRO_SINK);

static void
gst_cairo_sink_class_init (GstCairoSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);
  GstVideoSinkClass *video_sink_class = GST_VIDEO_SINK_CLASS (klass);

  gobject_class->set_property = gst_cairo_sink_set_property;
  gobject_class->get_property = gst_cairo_sink_get_property;
  gobject_class->dispose = gst_cairo_sink_dispose;
  gobject_class->finalize = gst_cairo_sink_finalize;
  base_sink_class->start = gst_cairo_sink_start;
  base_sink_class->stop = gst_cairo_sink_stop;
  base_sink_class->set_caps = gst_cairo_sink_set_caps;
  video_sink_class->show_frame = GST_DEBUG_FUNCPTR (gst_cairo_sink_show_frame);

  g_object_class_install_property (gobject_class, PROP_CAIRO_SURFACE,
      g_param_spec_boxed ("cairo-surface",
          "Cairo surface where the frame is put",
          "Cairo surface where the frame is put",
          CAIRO_GOBJECT_TYPE_SURFACE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_CAIRO_BACKEND,
      g_param_spec_enum ("cairo-backend",
          "Cairo backend to use",
          "Cairo backend to use",
          GST_CAIRO_BACKEND_TYPE,
          GST_CAIRO_BACKEND_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (gobject_class, PROP_MAIN_CONTEXT,
      g_param_spec_boxed ("main-context",
          "GMainContext to use for graphic calls",
          "GMainContext to use for graphic calls (e.g. gl calls)",
          G_TYPE_MAIN_CONTEXT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_cairo_sink_sink_template));

  gst_element_class_set_static_metadata (element_class, "FIXME Long name",
      "Generic", "FIXME Description", "FIXME <fixme@example.com>");
}

static void
gst_cairo_sink_init (GstCairoSink * cairosink)
{

  cairosink->sinkpad =
      gst_pad_new_from_static_template (&gst_cairo_sink_sink_template, "sink");
}

void
gst_cairo_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (object);

  switch (property_id) {
    case PROP_CAIRO_SURFACE:
    {
      cairo_surface_t *surface = g_value_get_boxed (value);
      if (surface)
        cairo_surface_reference (surface);
      if (cairosink->surface)
        cairo_surface_destroy (cairosink->surface);
      cairosink->surface = surface;
      break;
    }
    case PROP_CAIRO_BACKEND:
      cairosink->backend_type = g_value_get_enum (value);
      break;
    case PROP_MAIN_CONTEXT:
    {
      GMainContext *context = g_value_get_boxed (value);
      if (context)
        g_main_context_ref (context);
      cairosink->main_context = context;
      gst_cairo_backend_use_main_context (cairosink->backend, context);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_cairo_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (object);

  switch (property_id) {
    case PROP_CAIRO_SURFACE:
    {
      cairo_surface_t *surface = NULL;
      if (cairosink->surface)
        surface = cairo_surface_reference (cairosink->surface);

      g_value_set_boxed (value, surface);
      break;
    }
    case PROP_CAIRO_BACKEND:
      g_value_set_enum (value, cairosink->backend_type);
      break;
    case PROP_MAIN_CONTEXT:
    {
      GMainContext *context = NULL;
      if (cairosink->main_context)
        context = g_main_context_ref (cairosink->backend->thread_context);

      g_value_set_boxed (value, context);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_cairo_sink_dispose (GObject * object)
{
  /* GstCairoSink *cairosink = GST_CAIRO_SINK (object); */

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

void
gst_cairo_sink_finalize (GObject * object)
{
  /* GstCairoSink *cairosink = GST_CAIRO_SINK (object); */

  /* clean up object here */

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GstFlowReturn
gst_cairo_sink_show_frame (GstVideoSink * video_sink, GstBuffer * buf)
{

  return GST_FLOW_OK;
}

static gboolean
gst_cairo_sink_start (GstBaseSink * base_sink)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (base_sink);
  /* create backend, surface (and device?) here */

  if (cairosink->backend == NULL)
    cairosink->backend = gst_cairo_backend_new (cairosink->backend_type);

  return cairosink->backend != NULL;
}

static gboolean
gst_cairo_sink_stop (GstBaseSink * base_sink)
{
  if (cairosink->surface) {
    cairo_surface_destroy (cairosink->surface);
    cairosink->surface = NULL;
  }
  if (cairosink->device) {
    cairo_device_destroy (cairosink->device);
    cairosink->device = NULL;
  }
  if (cairosink->backend) {
    gst_cairo_backend_destroy (cairosink->backend);
    cairosink->backend = NULL;
  }

  return TRUE;
}

static gboolean
gst_cairo_sink_set_caps (GstBaseSink * sink, GstCaps * caps)
{
  GstStructure *structure;
  gint width, height;

  structure = gst_caps_get_structure (caps, 0);

  if (gst_structure_get_int (structure, "width", &width)
      || gst_structure_get_int (structure, "height", &height))
    return FALSE;

  if (cairosink->surface == NULL) {
    cairosink->surface = cairosink->backend->create_surface (width, height);
    if (cairosink->device)
      cairo_device_destroy (cairosink->device);

    cairosink->device =
        cairo_device_reference (cairo_surface_get_device (cairosink->surface));
  }

  return cairosink->surface != NULL;
}
