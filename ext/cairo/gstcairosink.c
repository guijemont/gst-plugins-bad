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

#ifdef CAIROSINK_GL_DEBUG
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>

static inline void
_gl_debug (const char *function, const char *message)
{
#ifdef GL_GREMEDY_string_marker
  gchar *full_message = g_strdup_printf ("%s: %s", function, message);

  glStringMarkerGREMEDY (0, full_message);
  GST_TRACE (message);

  g_free (full_message);
#endif
}

#define gl_debug(message) _gl_debug(__func__, message)
#define gl_debug_frame_terminator glFrameTerminatorGREMEDY
#else
#define gl_debug GST_TRACE
#define gl_debug_frame_terminator()
#endif


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
gst_cairo_sink_prepare (GstBaseSink * base_sink, GstBuffer * buf);

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

static gboolean gst_cairo_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query);


static GstMemory *gst_cairo_allocator_alloc (GstAllocator * allocator,
    gsize size, GstAllocationParams * params);
static GstMemory *gst_cairo_allocator_do_alloc (GstCairoAllocator *
    allocator, gint width, gint height, gint stride);
static void gst_cairo_allocator_free (GstAllocator * allocator,
    GstMemory * memory);
static GstCairoAllocator *gst_cairo_allocator_new (GstCairoSink * sink);

gpointer gst_cairo_allocator_mem_map (GstMemory * mem, gsize maxsize,
    GstMapFlags flags);
void gst_cairo_allocator_mem_unmap (GstMemory * mem);

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

struct _GstCairoAllocator
{
  GstAllocator allocator;

  GstCairoSink *sink;
};

typedef GstAllocatorClass GstCairoAllocatorClass;

enum
{
  PROP_0,
  PROP_CAIRO_SURFACE,
  PROP_CAIRO_BACKEND,
  PROP_MAIN_CONTEXT
};

#ifdef USE_CAIRO_GLX
#define GST_CAIRO_BACKEND_DEFAULT GST_CAIRO_BACKEND_GLX
#elif USE_CAIRO_EGL
#define GST_CAIRO_BACKEND_DEFAULT GST_CAIRO_BACKEND_EGL
#else
#define GST_CAIRO_BACKEND_DEFAULT GST_CAIRO_BACKEND_XLIB
#endif

#define GST_CAIRO_BACKEND_TYPE (gst_cairo_backend_get_type())
static GType
gst_cairo_backend_get_type (void)
{
  static GType backend_type = 0;

  static const GEnumValue backend_values[] = {
    {GST_CAIRO_BACKEND_GLX, "Use OpenGL and GLX", "glx"},
    {GST_CAIRO_BACKEND_XLIB, "Use Xlib", "xlib"},
    {GST_CAIRO_BACKEND_EGL, "Use OpenGLES and EGL", "egl"},
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
    GST_STATIC_CAPS ("video/x-raw, format=BGRA")
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
  base_sink_class->prepare = gst_cairo_sink_prepare;
  base_sink_class->propose_allocation = gst_cairo_sink_propose_allocation;
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
          GST_CAIRO_BACKEND_DEFAULT, G_PARAM_READWRITE));
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
  cairosink->owns_surface = TRUE;
  cairosink->sinkpad =
      gst_pad_new_from_static_template (&gst_cairo_sink_sink_template, "sink");
  cairosink->backend_type = GST_CAIRO_BACKEND_DEFAULT;
}

void
gst_cairo_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (object);

  switch (property_id) {
    case PROP_CAIRO_SURFACE:
    {
      cairo_surface_t *new_surface = g_value_get_boxed (value);
      if (new_surface && cairosink->surface != new_surface) {
        /* existing surface */
        cairo_surface_destroy (cairosink->surface);
        cairo_device_destroy (cairosink->device);

        cairo_surface_reference (new_surface);
        cairosink->surface = new_surface;
        cairosink->owns_surface = FALSE;

        cairosink->device = cairo_surface_get_device (new_surface);
        cairo_device_reference (cairosink->device);
      }
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
gst_cairo_sink_prepare (GstBaseSink * base_sink, GstBuffer * buf)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (base_sink);
  GST_TRACE_OBJECT (cairosink, "Got buffer %" GST_PTR_FORMAT, buf);

  return gst_cairo_sink_sync_render_operation (cairosink,
      GST_MINI_OBJECT_CAST (buf));
}

static GstFlowReturn
gst_cairo_sink_show_frame (GstVideoSink * video_sink, GstBuffer * buf)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (video_sink);
  GstFlowReturn ret;
  GST_TRACE_OBJECT (video_sink, "Need to show buffer %" GST_PTR_FORMAT, buf);

  ret = gst_cairo_sink_sync_render_operation (cairosink, NULL);

  return ret;
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

  /* query_can_map */
  cairosink->backend->can_map =
      cairosink->backend->query_can_map (cairosink->surface);

  if (cairosink->backend->can_map)
    cairosink->allocator = gst_cairo_allocator_new (cairosink);

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

  GST_TRACE_OBJECT (cairosink, "Hi from new thread");
  g_main_context_push_thread_default (cairosink->render_main_context);
  cairosink->loop = g_main_loop_new (cairosink->render_main_context, FALSE);
  g_main_loop_run (cairosink->loop);
  return NULL;
}


static gboolean
gst_cairo_sink_stop (GstBaseSink * base_sink)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (base_sink);

  if (cairosink->allocator) {
    gst_object_unref (cairosink->allocator);
    cairosink->allocator = NULL;
  }

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

  GST_TRACE_OBJECT (cairosink, "set_caps(%" GST_PTR_FORMAT ")", caps);
  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "width", &width)
      || !gst_structure_get_int (structure, "height", &height))
    return FALSE;

  ret = gst_cairo_sink_sync_render_operation (cairosink,
      GST_MINI_OBJECT_CAST (caps));

  return cairosink->surface != NULL && ret == GST_FLOW_OK;
}

static gboolean
gst_cairo_sink_source_prepare (GSource * source, gint * timeout_)
{
  CairoSinkSource *sink_source = (CairoSinkSource *) source;
  GST_TRACE_OBJECT (sink_source->sink, "prepare");
  return gst_cairo_sink_source_check (source);
}

static gboolean
gst_cairo_sink_source_check (GSource * source)
{
  CairoSinkSource *sink_source = (CairoSinkSource *) source;
  gboolean ret;

  ret = gst_data_queue_is_full (sink_source->sink->queue);

  GST_TRACE_OBJECT (sink_source->sink, "returning %s", ret ? "TRUE" : "FALSE");

  return ret;
}

static GstFlowReturn
upload_buffer (GstCairoSink * cairosink, GstBuffer * buf)
{
  GstStructure *structure;
  GstMemory *gmem;
  GstMapInfo map_info;
  gint width, height;

  if (!cairosink->caps)
    return GST_FLOW_NOT_NEGOTIATED;

  if (gst_buffer_n_memory (buf) != 1) {
    GST_ERROR_OBJECT (cairosink, "Handling of buffer with more than one "
        "GstMemory not implemented");
    return GST_FLOW_ERROR;
  }

  gmem = gst_buffer_peek_memory (buf, 0);

  if (gmem->allocator == GST_ALLOCATOR_CAST (cairosink->allocator)) {
    cairo_t *context;
    GstCairoMemory *mem = (GstCairoMemory *) gmem;

    /* mem is already in GPU */
    context = cairo_create (cairosink->surface);
    cairo_set_source_surface (context, mem->surface, 0, 0);
    cairo_set_operator (context, CAIRO_OPERATOR_SOURCE);
    gl_debug ("cairo_paint()");
    cairo_paint (context);
    gl_debug ("cairo_paint() done");
    cairo_destroy (context);

    GST_TRACE_OBJECT (cairosink,
        "Copied texture from %" GST_PTR_FORMAT " to display surface", gmem);

    return GST_FLOW_OK;
  }

  structure = gst_caps_get_structure (cairosink->caps, 0);
  gst_structure_get_int (structure, "width", &width);
  gst_structure_get_int (structure, "height", &height);

  if (gst_memory_map (gmem, &map_info, GST_MAP_READ)) {
    /* FIXME: should we recreate the context every time or save it and reuse
     * it? */
    cairo_surface_t *source;
    cairo_t *context;
    double x_scale = 1.0;
    double y_scale = 1.0;
    gint surface_width;
    gint surface_height;

    gint stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);

    if (stride * height > map_info.size) {
      GST_ERROR_OBJECT (cairosink, "Incompatible stride? width:%d height:%d "
          "expected stride: %d expected size: %d actual size: %d",
          width, height, stride, stride * height, map_info.size);
      gst_memory_unmap (gmem, &map_info);
      return GST_FLOW_ERROR;
    }

    source = cairo_image_surface_create_for_data (map_info.data,
        CAIRO_FORMAT_ARGB32, width, height, stride);

    context = cairo_create (cairosink->surface);

    /* compute actual scaling */
    if (!cairosink->owns_surface) {
      cairosink->backend->get_size (cairosink->surface, &surface_width,
          &surface_height);
      x_scale = ((double) surface_width) / width;
      y_scale = ((double) surface_height) / height;
    }

    cairo_scale (context, x_scale, y_scale);
    cairo_set_source_surface (context, source, 0, 0);
    cairo_set_operator (context, CAIRO_OPERATOR_SOURCE);
    cairo_paint (context);
    cairo_destroy (context);

    cairo_surface_destroy (source);
    gst_memory_unmap (gmem, &map_info);
    GST_TRACE_OBJECT (cairosink, "Uploaded buffer %" GST_PTR_FORMAT, buf);
  } else {
    GST_ERROR_OBJECT (cairosink, "Could not map memory for reading");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static gpointer
_structure_get_pointer (GstStructure * structure, const gchar * field)
{
  const GValue *value;

  value = gst_structure_get_value (structure, field);

  return g_value_get_pointer (value);
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

  GST_TRACE_OBJECT (cairosink, "dispatch");

  if (!gst_data_queue_pop (cairosink->queue, &item))
    return FALSE;

  object = item->object;

  g_mutex_lock (&cairosink->render_mutex);
  if (GST_IS_CAPS (object)) {
    GstCaps *caps = GST_CAPS_CAST (object);
    gboolean need_new_surface;


    GST_TRACE_OBJECT (cairosink, "got new caps");

    /* FIXME: we should know whether we create the surface or it is externally
     * provided and act accordingly */
    need_new_surface = cairosink->surface == NULL
        || (!gst_caps_is_equal (caps, cairosink->caps)
        && cairosink->owns_surface);

    if (need_new_surface) {
      GstStructure *structure;
      gint caps_width, caps_height;

      /* we only destroy surface if we owns surface */
      if (cairosink->surface)
        cairo_surface_destroy (cairosink->surface);

      GST_TRACE_OBJECT (cairosink, "creating surface");
      structure = gst_caps_get_structure (caps, 0);
      gst_structure_get_int (structure, "width", &caps_width);
      gst_structure_get_int (structure, "height", &caps_height);
      cairosink->surface =
          cairosink->backend->create_display_surface (caps_width, caps_height);
      cairosink->device = cairo_surface_get_device (cairosink->surface);
    } else {
      GST_TRACE_OBJECT (cairosink, "Already have a surface for these caps");
    }

    if (cairosink->surface) {
      cairosink->last_ret = GST_FLOW_OK;
      if (cairosink->caps)
        gst_caps_unref (cairosink->caps);
      cairosink->caps = gst_caps_ref (caps);
    } else {
      GST_ERROR_OBJECT (cairosink, "No surface!");
      cairosink->last_ret = GST_FLOW_ERROR;
    }

  } else if (GST_IS_QUERY (object)) {
    GstStructure *query_structure =
        (GstStructure *) gst_query_get_structure (GST_QUERY_CAST (object));

    if (gst_structure_has_name (query_structure, "cairosink-allocate-surface")) {
      GstMemory *mem;
      gint width, height, stride;
      if (!gst_structure_get_int (query_structure, "width", &width)
          || !gst_structure_get_int (query_structure, "height", &height)
          || !gst_structure_get_int (query_structure, "stride", &stride))
        g_assert_not_reached ();

      mem = gst_cairo_allocator_do_alloc (cairosink->allocator, width, height,
          stride);

      GST_TRACE_OBJECT (cairosink, "got new mem %p, setting on query %p", mem,
          object);
      {
        GValue value = { 0, };
        g_value_init (&value, G_TYPE_POINTER);
        g_value_set_pointer (&value, mem);
        gst_structure_set_value (query_structure, "memory", &value);
        g_value_unset (&value);
      }
    } else if (gst_structure_has_name (query_structure,
            "cairosink-map-surface")) {
      gpointer mapped_area;
      cairo_surface_t *surface;
      GstCairoBackendSurfaceInfo *surface_info;
      GstMapFlags flags;

      if (!gst_structure_get (query_structure,
              "surface", G_TYPE_POINTER, &surface,
              "surface-info", G_TYPE_POINTER, &surface_info,
              "flags", G_TYPE_INT, &flags, NULL)) {
        g_assert_not_reached ();
      }

      mapped_area =
          cairosink->backend->surface_map (surface, surface_info, flags);
      GST_TRACE_OBJECT (cairosink, "Surface %p mapped to %p", surface,
          mapped_area);
      {
        GValue value = { 0, };
        g_value_init (&value, G_TYPE_POINTER);
        g_value_set_pointer (&value, mapped_area);
        gst_structure_set_value (query_structure, "mapped-area", &value);
        g_value_unset (&value);
      }
    } else if (gst_structure_has_name (query_structure,
            "cairosink-unmap-surface")) {
      cairo_surface_t *surface;
      GstCairoBackendSurfaceInfo *surface_info;

      if (!gst_structure_get (query_structure,
              "surface", G_TYPE_POINTER, &surface,
              "surface-info", G_TYPE_POINTER, &surface_info, NULL)) {
        g_assert_not_reached ();
      }
      cairosink->backend->surface_unmap (surface, surface_info);
    } else if (gst_structure_has_name (query_structure,
            "cairosink-destroy-surface")) {
      cairo_surface_t *surface;
      GstCairoBackendSurfaceInfo *surface_info;

      if (!gst_structure_get (query_structure,
              "surface", G_TYPE_POINTER, &surface,
              "surface-info", G_TYPE_POINTER, &surface_info, NULL)) {
        g_assert_not_reached ();
      }
      cairosink->backend->destroy_surface (surface, surface_info);
    } else {
      g_assert_not_reached ();
    }

    cairosink->last_ret = GST_FLOW_OK;

  } else if (GST_IS_BUFFER (object)) {
    GstBuffer *buf = GST_BUFFER_CAST (object);

    gl_debug ("about to upload buffer");
    cairosink->last_ret = upload_buffer (cairosink, buf);
    gl_debug ("upload_buffer finished");

  } else if (!object) {
    cairosink->backend->show (cairosink->surface);
  }

  item->destroy (item);
  cairosink->last_finished_operation = object;
  g_cond_signal (&cairosink->render_cond);
  g_mutex_unlock (&cairosink->render_mutex);

  return TRUE;
}

static void
gst_cairo_sink_queue_item_destroy (gpointer data)
{
  GstDataQueueItem *item = (GstDataQueueItem *) data;
  if (item->object && !GST_IS_QUERY (item->object))
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

  GST_TRACE_OBJECT (cairosink,
      "about to send operation %" GST_PTR_FORMAT " to render thread",
      operation);
  item = g_slice_new0 (GstDataQueueItem);

  if (operation == NULL) {
    item->object = NULL;
  } else if (GST_IS_QUERY (operation)) {
    /* When it's a query, the caller keeps the ownership until we return. We
     * do not refcount it more so that its structure remains mutable */
    item->object = operation;
  } else {
    item->object = gst_mini_object_ref (operation);
  }

  item->duration = GST_CLOCK_TIME_NONE;
  item->destroy = gst_cairo_sink_queue_item_destroy;
  item->visible = TRUE;

  g_mutex_lock (&cairosink->render_mutex);
  {
    GST_TRACE_OBJECT (cairosink, "Got lock, pushing on queue");
    if (!gst_data_queue_push (cairosink->queue, item)) {
      GST_DEBUG_OBJECT (cairosink,
          "Could not push on queue, returning that we are flushing");
      gst_cairo_sink_queue_item_destroy (item);
      g_mutex_unlock (&cairosink->render_mutex);
      return GST_FLOW_FLUSHING;
    }
    GST_TRACE_OBJECT (cairosink, "pushed item on queue");

    g_main_context_wakeup (cairosink->render_main_context);

    do {
      GST_TRACE_OBJECT (cairosink, "waiting for cond");
      g_cond_wait (&cairosink->render_cond, &cairosink->render_mutex);
    } while (cairosink->last_finished_operation != operation);
    GST_TRACE_OBJECT (cairosink, "got cond");

    last_ret = cairosink->last_ret;
  }
  g_mutex_unlock (&cairosink->render_mutex);

  GST_TRACE_OBJECT (cairosink, "returning %s", gst_flow_get_name (last_ret));

  return last_ret;
}

static gsize
_compute_padding (GstCairoSink * sink)
{
  GstStructure *structure;
  gint height, width, stride;

  structure = gst_caps_get_structure (sink->caps, 0);

  if (!gst_structure_get_int (structure, "width", &width)
      || gst_structure_get_int (structure, "height", &height))
    return 0;

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);

  return (stride - width) * height;
}

static gboolean
gst_cairo_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstCaps *caps;
  GstCairoSink *cairosink = GST_CAIRO_SINK (bsink);
  GstStructure *structure;
  gboolean need_pool;
  gint width, height, size = 0;
  gboolean ret = FALSE;

  gst_query_parse_allocation (query, &caps, &need_pool);
  if (!caps) {
    GST_ERROR_OBJECT (cairosink,
        "Can only handle allocation queries with caps");
    return FALSE;
  }

  structure = gst_caps_get_structure (cairosink->caps, 0);
  if (gst_structure_get_int (structure, "width", &width)
      && gst_structure_get_int (structure, "height", &height))
    size = width * height * 4;

  if (!size) {
    GST_ERROR_OBJECT (cairosink, "No caps or bad caps");
    return FALSE;
  }


  if (need_pool && !cairosink->buffer_pool) {
    GstStructure *config;

    cairosink->buffer_pool = gst_buffer_pool_new ();
    config = gst_buffer_pool_get_config (cairosink->buffer_pool);
    if (cairosink->allocator)
      gst_buffer_pool_config_set_allocator (config,
          GST_ALLOCATOR (cairosink->allocator), NULL);
    gst_buffer_pool_config_set_params (config, cairosink->caps, size, 2, 0);
    gst_buffer_pool_set_config (cairosink->buffer_pool, config);
  }

  if (need_pool && cairosink->buffer_pool) {
    gst_query_add_allocation_pool (query, cairosink->buffer_pool, size, 2, 0);
    GST_DEBUG_OBJECT (cairosink,
        "adding pool %" GST_PTR_FORMAT " to query %" GST_PTR_FORMAT " (%p)",
        cairosink->buffer_pool, query, query);

    ret = TRUE;
  }

  if (cairosink->allocator) {
    GstAllocationParams params;
    params.flags = 0;
    params.align = 0;
    params.prefix = 0;
    params.padding = _compute_padding (cairosink);
    gst_query_add_allocation_param (query,
        GST_ALLOCATOR (cairosink->allocator), &params);
    GST_DEBUG_OBJECT (cairosink,
        "adding allocator %" GST_PTR_FORMAT " to query %" GST_PTR_FORMAT
        " (%p)", cairosink->allocator, query, query);
    ret = TRUE;
  }

  return ret;
}

/* --- allocator stuff --- */
GType gst_cairo_allocator_get_type (void);
G_DEFINE_TYPE (GstCairoAllocator, gst_cairo_allocator, GST_TYPE_ALLOCATOR);

static void
gst_cairo_allocator_class_init (GstCairoAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);

  allocator_class->alloc = gst_cairo_allocator_alloc;
  allocator_class->free = gst_cairo_allocator_free;
}

static void
gst_cairo_allocator_init (GstCairoAllocator * cairo_allocator)
{
  GstAllocator *allocator = GST_ALLOCATOR_CAST (cairo_allocator);

  allocator->mem_type = "CairoSurface";
  allocator->mem_map = gst_cairo_allocator_mem_map;
  allocator->mem_unmap = gst_cairo_allocator_mem_unmap;
}

static GstCairoAllocator *
gst_cairo_allocator_new (GstCairoSink * sink)
{
  GstCairoAllocator *allocator = g_object_new (gst_cairo_allocator_get_type (),
      NULL);
  /* FIXME: we probably should take a ref here when allocator is not a member
   * of the sink any more */
  allocator->sink = sink;

  return allocator;
}

static GstMemory *
gst_cairo_allocator_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  GstCairoAllocator *cairo_allocator = (GstCairoAllocator *) allocator;
  GstStructure *query_structure, *caps_structure;
  GstQuery *query;
  int width, height, stride;
  GstFlowReturn ret;
  GstMemory *mem;

  caps_structure = gst_caps_get_structure (cairo_allocator->sink->caps, 0);

  if (!gst_structure_get_int (caps_structure, "width", &width)
      || !gst_structure_get_int (caps_structure, "height", &height)) {
    GST_WARNING_OBJECT (cairo_allocator->sink, "No proper caps set");
    return NULL;
  }

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);

  query_structure = gst_structure_new ("cairosink-allocate-surface",
      "width", G_TYPE_INT, width,
      "height", G_TYPE_INT, height, "stride", G_TYPE_INT, stride, NULL);

  query = gst_query_new_custom (GST_QUERY_CUSTOM, query_structure);

  ret = gst_cairo_sink_sync_render_operation (cairo_allocator->sink,
      GST_MINI_OBJECT_CAST (query));

  GST_TRACE_OBJECT (cairo_allocator->sink,
      "render operation returned %s, %p %" GST_PTR_FORMAT,
      gst_flow_get_name (ret), query, query);

  if (ret != GST_FLOW_OK
      || !gst_structure_has_field (query_structure, "memory"))
    goto beach;

  mem = GST_MEMORY_CAST (_structure_get_pointer (query_structure, "memory"));

beach:
  if (!mem) {
    GST_WARNING_OBJECT (cairo_allocator->sink, "Could not allocate");
    /* FIXME: provide fallback */
  }

  gst_query_unref (query);

  GST_TRACE_OBJECT (cairo_allocator->sink, "Returning memory %" GST_PTR_FORMAT,
      mem);
  return mem;
}

static GstMemory *
gst_cairo_allocator_do_alloc (GstCairoAllocator * allocator, gint width,
    gint height, gint stride)
{
  GstCairoMemory *mem;
  GstCairoBackend *backend;

  backend = allocator->sink->backend;

  mem = g_slice_new (GstCairoMemory);

  gst_memory_init (GST_MEMORY_CAST (mem), 0, GST_ALLOCATOR_CAST (allocator),
      NULL, width * height * 4, 0, 0, width * height * 4);

  mem->surface = backend->create_surface (backend, allocator->sink->device,
      width, height, &mem->surface_info);
  mem->backend = backend;

  GST_TRACE_OBJECT (allocator->sink, "Created memory %" GST_PTR_FORMAT, mem);
  return GST_MEMORY_CAST (mem);
}

static void
gst_cairo_allocator_free (GstAllocator * allocator, GstMemory * gmem)
{
  GstCairoMemory *mem = (GstCairoMemory *) gmem;
  GstCairoAllocator *cairo_allocator = (GstCairoAllocator *) gmem->allocator;
  GstStructure *query_structure;
  GstQuery *query;
  GstFlowReturn ret;

  query_structure = gst_structure_new ("cairosink-destroy-surface",
      "surface", G_TYPE_POINTER, mem->surface,
      "surface-info", G_TYPE_POINTER, mem->surface_info, NULL);
  query = gst_query_new_custom (GST_QUERY_CUSTOM, query_structure);

  ret = gst_cairo_sink_sync_render_operation (cairo_allocator->sink,
      GST_MINI_OBJECT_CAST (query));

  if (ret != GST_FLOW_OK)
    GST_ERROR_OBJECT (cairo_allocator->sink,
        "Could not free mem %" GST_PTR_FORMAT, gmem);

  g_slice_free (GstCairoMemory, mem);
}

gpointer
gst_cairo_allocator_mem_map (GstMemory * gmem, gsize maxsize, GstMapFlags flags)
{
  GstCairoMemory *mem = (GstCairoMemory *) gmem;
  GstStructure *query_structure;
  GstCairoAllocator *cairo_allocator = (GstCairoAllocator *) gmem->allocator;
  GstQuery *query;
  gpointer mapped_area;
  GstFlowReturn ret;

  /* After this, the surface will be modified outside of cairo, so we need to
   * flush it first */
  gl_debug ("about to flush");
  cairo_surface_flush (mem->surface);
  gl_debug ("done");

  query_structure = gst_structure_new ("cairosink-map-surface",
      "surface", G_TYPE_POINTER, mem->surface,
      "surface-info", G_TYPE_POINTER, mem->surface_info,
      "flags", G_TYPE_INT, (int) flags, NULL);
  query = gst_query_new_custom (GST_QUERY_CUSTOM, query_structure);

  ret = gst_cairo_sink_sync_render_operation (cairo_allocator->sink,
      GST_MINI_OBJECT_CAST (query));


  if (ret != GST_FLOW_OK
      || !gst_structure_has_field (query_structure, "mapped-area"))
    goto beach;

  mapped_area = _structure_get_pointer (query_structure, "mapped-area");

  GST_TRACE_OBJECT (cairo_allocator->sink,
      "render returned mapped area: %p", mapped_area);

beach:
  if (!mapped_area)
    GST_ERROR_OBJECT (cairo_allocator->sink, "Could not map");

  gst_query_unref (query);

  return mapped_area;
}

void
gst_cairo_allocator_mem_unmap (GstMemory * gmem)
{
  GstCairoMemory *mem = (GstCairoMemory *) gmem;
  GstCairoAllocator *cairo_allocator = (GstCairoAllocator *) gmem->allocator;
  GstStructure *query_structure;
  GstQuery *query;
  GstFlowReturn ret;

  query_structure = gst_structure_new ("cairosink-unmap-surface",
      "surface", G_TYPE_POINTER, mem->surface,
      "surface-info", G_TYPE_POINTER, mem->surface_info, NULL);
  query = gst_query_new_custom (GST_QUERY_CUSTOM, query_structure);

  ret = gst_cairo_sink_sync_render_operation (cairo_allocator->sink,
      GST_MINI_OBJECT_CAST (query));

  /* surface has been modified outside of cairo */
  gl_debug ("About to mark dirty");
  cairo_surface_mark_dirty (mem->surface);
  gl_debug ("done");

  if (ret != GST_FLOW_OK)
    GST_ERROR_OBJECT (cairo_allocator->sink,
        "Could not unmap mem %" GST_PTR_FORMAT, gmem);
}
