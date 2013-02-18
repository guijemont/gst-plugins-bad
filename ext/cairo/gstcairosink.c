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

GST_DEBUG_CATEGORY (gst_cairo_sink_debug_category);
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

static gpointer gst_cairo_sink_thread_init (gpointer data);

static GstFlowReturn
gst_cairo_sink_sync_render_operation (GstCairoSink * cairosink,
    GstMiniObject * operation);

static gboolean
gst_cairo_sink_source_prepare (GSource * source, gint * timeout_);
static gboolean gst_cairo_sink_source_check (GSource * source);
static gboolean gst_cairo_sink_source_dispatch (GSource * source,
    GSourceFunc callback, gpointer user_data);

typedef struct
{
  GSource parent;
  GstCairoSink *sink;
} CairoSinkSource;

GSourceFuncs gst_cairo_sink_source_funcs = {
  gst_cairo_sink_source_prepare,
  gst_cairo_sink_source_check,
  gst_cairo_sink_source_dispatch,
};


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
    GST_STATIC_CAPS ("video/x-raw, format=BGRx")
    );


/* class initialization */

#define parent_class gst_cairo_sink_parent_class
G_DEFINE_TYPE (GstCairoSink, gst_cairo_sink, GST_TYPE_VIDEO_SINK);

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
      cairosink->render_main_context = context;
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
      if (cairosink->render_main_context)
        context = g_main_context_ref (cairosink->render_main_context);

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
queue_check_full_func (GstDataQueue * queue, guint visible, guint bytes,
    guint64 time, gpointer checkdata)
{
  return visible != 0;
}


static gboolean
gst_cairo_sink_start (GstBaseSink * base_sink)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (base_sink);

  if (cairosink->backend == NULL)
    cairosink->backend = gst_cairo_backend_new (cairosink->backend_type);

  if (!cairosink->backend)
    goto error;

  if (cairosink->backend->need_own_thread && !cairosink->render_main_context) {
    GError *error = NULL;

    cairosink->render_main_context = g_main_context_new ();
    cairosink->thread =
        g_thread_try_new ("backend thread", gst_cairo_sink_thread_init,
        cairosink, &error);
    if (!cairosink->thread) {
      GST_ERROR ("Could not create rendering thread: %s", error->message);
      g_error_free (error);
      goto error;
    }
  }

  if (!cairosink->render_main_context)
    goto error;

  cairosink->queue =
      gst_data_queue_new (queue_check_full_func, NULL, NULL, NULL);
  cairosink->source =
      g_source_new (&gst_cairo_sink_source_funcs, sizeof (CairoSinkSource));
  if (!cairosink->source)
    goto error;

  /* We don't take a ref here because source lives at most from _start() to
   * _stop(), and cairosink is guaranteed to exist then */
  ((CairoSinkSource *) cairosink->source)->sink = cairosink;
  g_source_attach (cairosink->source, cairosink->render_main_context);

  return TRUE;

error:
  if (cairosink->backend) {
    gst_cairo_backend_destroy (cairosink->backend);
    cairosink->backend = NULL;
  }

  if (cairosink->render_main_context) {
    g_main_context_unref (cairosink->render_main_context);
    cairosink->render_main_context = NULL;
  }

  if (cairosink->thread) {
    g_thread_unref (cairosink->thread);
    cairosink->thread = NULL;
  }

  return FALSE;
}

static gpointer
gst_cairo_sink_thread_init (gpointer data)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (data);

  g_main_context_push_thread_default (cairosink->render_main_context);
  cairosink->loop = g_main_loop_new (cairosink->render_main_context, FALSE);
  g_main_loop_run (cairosink->loop);
  return NULL;
}


static gboolean
gst_cairo_sink_stop (GstBaseSink * base_sink)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (base_sink);

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


  if (cairosink->source) {
    g_source_destroy (cairosink->source);
    g_source_unref (cairosink->source);
    cairosink->source = NULL;
  }

  if (cairosink->render_main_context) {
    g_main_context_unref (cairosink->render_main_context);
    cairosink->render_main_context = NULL;
  }

  if (cairosink->loop) {
    g_main_loop_quit (cairosink->loop);
    g_main_loop_unref (cairosink->loop);
    cairosink->loop = NULL;
  }

  if (cairosink->queue) {
    gst_data_queue_set_flushing (cairosink->queue, TRUE);
    g_object_unref (cairosink->queue);
    cairosink->queue = NULL;
  }


  return TRUE;
}

static gboolean
gst_cairo_sink_set_caps (GstBaseSink * base_sink, GstCaps * caps)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (base_sink);
  GstStructure *structure;
  gint width, height;
  GstFlowReturn ret;

  structure = gst_caps_get_structure (caps, 0);

  if (gst_structure_get_int (structure, "width", &width)
      || gst_structure_get_int (structure, "height", &height))
    return FALSE;

  ret = gst_cairo_sink_sync_render_operation (cairosink,
      GST_MINI_OBJECT_CAST (caps));

  return cairosink->surface != NULL && ret == GST_FLOW_OK;
}

static gboolean
gst_cairo_sink_source_prepare (GSource * source, gint * timeout_)
{
  return gst_cairo_sink_source_check (source);
}

static gboolean
gst_cairo_sink_source_check (GSource * source)
{
  CairoSinkSource *sink_source = (CairoSinkSource *) source;

  return gst_data_queue_is_full (sink_source->sink->queue);
}

static gboolean
gst_cairo_sink_source_dispatch (GSource * source,
    GSourceFunc callback, gpointer user_data)
{
  GstDataQueueItem *item = NULL;
  CairoSinkSource *sink_source = (CairoSinkSource *) source;
  GstCairoSink *cairosink;
  GstMiniObject *object;

  cairosink = sink_source->sink;

  if (!gst_data_queue_pop (cairosink->queue, &item))
    return FALSE;

  object = item->object;

  if (GST_IS_CAPS (object)) {
    GstCaps *caps = GST_CAPS_CAST (object);
    GstStructure *structure;
    gint caps_width, caps_height;
    gboolean need_new_surface = cairosink->surface != NULL;;
    structure = gst_caps_get_structure (caps, 0);
    gst_structure_get_int (structure, "width", &caps_width);
    gst_structure_get_int (structure, "height", &caps_height);

    if (cairosink->surface) {
      gint surface_width, surface_height;
      cairosink->backend->get_size (cairosink->surface, &surface_width,
          &surface_height);
      need_new_surface = caps_width != surface_width
          || caps_height != surface_height;
    }

    if (need_new_surface) {
      if (cairosink->surface)
        cairo_surface_destroy (cairosink->surface);

      cairosink->surface =
          cairosink->backend->create_surface (caps_width, caps_height);
    }

    if (cairosink->surface) {
      cairosink->last_ret = GST_FLOW_OK;
    } else {
      GST_ERROR_OBJECT (cairosink, "No surface!");
      cairosink->last_ret = GST_FLOW_ERROR;
    }

  } else if (GST_IS_QUERY (object)) {
  } else if (GST_IS_BUFFER (object)) {
  } else if (!object) {
  }


  return TRUE;
}

static void
gst_cairo_sink_queue_item_destroy (gpointer data)
{
  GstDataQueueItem *item = (GstDataQueueItem *) data;
  if (item->object)
    gst_mini_object_unref (item->object);

  g_slice_free (GstDataQueueItem, item);
}

/* Sends an operation to the rendering thread and wait for it to be handled.
 * Operation can be one of:
  - NULL: do a rendering of the last buffer sent
  - a GstCaps: try to use as new caps and, if needed, create a new surface
  - a GstBuffer to upload for future rendering
  - a GstQuery with a special operation?
 */
static GstFlowReturn
gst_cairo_sink_sync_render_operation (GstCairoSink * cairosink,
    GstMiniObject * operation)
{
  /* FIXME: should be on the heap for compatibility with the async case */
  /* item is on the stack, should be OK since we wait for the other thread to
   * finish handling it */
  GstDataQueueItem *item;
  GstFlowReturn last_ret;

  item = g_slice_new0 (GstDataQueueItem);

  if (operation == NULL) {
    item->object = NULL;
  } else {
    item->object = gst_mini_object_ref (operation);
  }

  item->duration = GST_CLOCK_TIME_NONE;
  item->destroy = gst_cairo_sink_queue_item_destroy;

  g_mutex_lock (&cairosink->render_mutex);
  {
    if (!gst_data_queue_push (cairosink->queue, item)) {
      gst_cairo_sink_queue_item_destroy (item);
      g_mutex_unlock (&cairosink->render_mutex);
      return GST_FLOW_FLUSHING;
    }

    do {
      g_cond_wait (&cairosink->render_cond, &cairosink->render_mutex);
    } while (cairosink->last_finished_operation != operation);

    last_ret = cairosink->last_ret;
  }
  g_mutex_unlock (&cairosink->render_mutex);

  return last_ret;
}
