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

#include <cairo-gl.h>

#include "gstcairosink.h"
#include "gstcairobackend.h"
#include "gstcairogldebug.h"

#if defined(USE_CAIRO_GL) || defined(USE_CAIRO_GLESV2)
#include "gstcairobackendgl.h"
#include "gstglmemory.h"
#else
#error "Need one of cairo-gl or cairo-glesv2, other backends not implemented"
#endif

#ifdef HAVE_CAIRO_EGL
#include "gstcairosystemegl.h"
#endif
#ifdef HAVE_CAIRO_GLX
#include "gstcairosystemglx.h"
#endif

#if !(defined(HAVE_CAIRO_EGL) || defined (HAVE_CAIRO_GLX))
#error "Need one of cairo-egl or cairo-glx"
#endif

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

static gboolean gst_cairo_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query);

enum
{
  PROP_0,
  PROP_CAIRO_SURFACE,
  PROP_CAIRO_BACKEND,
  PROP_CAIRO_SYSTEM,
  PROP_MAIN_CONTEXT
};

#define GST_CAIRO_BACKEND_DEFAULT GST_CAIRO_BACKEND_GL

#ifdef HAVE_CAIRO_GLX
#define GST_CAIRO_SYSTEM_DEFAULT GST_CAIRO_SYSTEM_GLX
#else
#define GST_CAIRO_SYSTEM_DEFAULT GST_CAIRO_SYSTEM_EGL
#endif

#define GST_CAIRO_BACKEND_TYPE (gst_cairo_backend_get_type())
static GType
gst_cairo_backend_get_type (void)
{
  static GType backend_type = 0;

  static const GEnumValue backend_values[] = {
    {GST_CAIRO_BACKEND_GL,
#ifdef USE_CAIRO_GL
          "Use OpenGL",
#elif USE_CAIRO_GLESV2
          "Use OpenGLESV2",
#endif
        "gl"},
    {0, NULL, NULL}
  };

  if (!backend_type)
    backend_type =
        g_enum_register_static ("GstCairoBackendType", backend_values);

  return backend_type;
}

#define GST_CAIRO_SYSTEM_TYPE (gst_cairo_system_get_type())
static GType
gst_cairo_system_get_type (void)
{
  static GType system_type = 0;

  static const GEnumValue system_values[] = {
    {GST_CAIRO_SYSTEM_EGL, "Use EGL", "egl"},
    {GST_CAIRO_SYSTEM_GLX, "Use GLX", "glx"},
    {0, NULL, NULL}
  };

  if (!system_type)
    system_type = g_enum_register_static ("GstCairoSystemType", system_values);

  return system_type;
}

/* pad templates */

static GstStaticPadTemplate gst_cairo_sink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
#ifdef USE_CAIRO_GL
    /* when we use textures, we want to upload stuff in RGBA */
    GST_STATIC_CAPS ("video/x-raw, format=RGBA")
#else
    /* when using a cairo image surface, we need to provide data in BGRA */
    GST_STATIC_CAPS ("video/x-raw, format=BGRA")
#endif
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
  g_object_class_install_property (gobject_class, PROP_CAIRO_SYSTEM,
      g_param_spec_enum ("cairo-system",
          "Cairo system to use",
          "Cairo system to use",
          GST_CAIRO_SYSTEM_TYPE, GST_CAIRO_SYSTEM_DEFAULT, G_PARAM_READWRITE));
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
  cairosink->system_type = GST_CAIRO_SYSTEM_DEFAULT;
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
    case PROP_CAIRO_SYSTEM:
      cairosink->system_type = g_value_get_enum (value);
      break;
    case PROP_MAIN_CONTEXT:
    {
      GMainContext *context = g_value_get_boxed (value);
      if (context) {
        g_main_context_ref (context);
      }
      if (cairosink->render_thread_info) {
        gst_cairo_thread_info_destroy (cairosink->render_thread_info);
        cairosink->render_thread_info = NULL;
      }

      cairosink->render_main_context = context;
      if (context)
        cairosink->render_thread_info = gst_cairo_thread_info_new (context);
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
    case PROP_CAIRO_SYSTEM:
      g_value_set_enum (value, cairosink->system_type);
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

static void
_copy_buffer (GstStructure * params)
{
  GstMemory *mem;
  cairo_surface_t *surface;
  GstCairoSink *cairosink;
  cairo_t *context;
  GstMapInfo map_info;
  gint width, height;
  GstFlowReturn return_value = GST_FLOW_OK;

  if (!gst_structure_get (params, "cairosink", G_TYPE_POINTER, &cairosink,
          "memory", G_TYPE_POINTER, &mem,
          "width", G_TYPE_INT, &width, "height", G_TYPE_INT, &height, NULL)) {
    return_value = GST_FLOW_ERROR;
    goto end;
  }

  if (mem->allocator == cairosink->allocator) {
    surface = cairosink->backend->create_surface (cairosink->backend,
        cairosink->device, mem);
    if (!surface) {
      GST_WARNING_OBJECT (cairosink, "Backend could not create new surface");
      return_value = GST_FLOW_ERROR;
      goto end;
    }
  } else if (gst_memory_map (mem, &map_info, GST_MAP_READ)) {
    /* FIXME: check that our data has this stride, convert otherwise */
    gint stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);

    if (stride * height > map_info.size) {
      GST_ERROR_OBJECT (cairosink, "Incompatible stride? width:%d height:%d "
          "expected stride: %d expected size: %d actual size: %ld",
          width, height, stride, stride * height, map_info.size);
      return_value = GST_FLOW_ERROR;
      goto end;
    }

    surface = cairo_image_surface_create_for_data (map_info.data,
        CAIRO_FORMAT_ARGB32, width, height, stride);


    GST_TRACE_OBJECT (cairosink, "Uploaded memory %" GST_PTR_FORMAT, mem);
  } else {
    GST_ERROR_OBJECT (cairosink,
        "Do not know how to upload data from %" GST_PTR_FORMAT, mem);
    return_value = GST_FLOW_ERROR;
    goto end;
  }

  context = cairo_create (cairosink->surface);
  cairo_set_source_surface (context, surface, 0, 0);
  cairo_set_operator (context, CAIRO_OPERATOR_SOURCE);
  gl_debug ("cairo_paint()");
  cairo_paint (context);
  gl_debug ("cairo_paint() done");
  cairo_destroy (context);

  GST_TRACE_OBJECT (cairosink,
      "Copied texture/data from %" GST_PTR_FORMAT " to display surface", mem);


end:

  if (mem->allocator == cairosink->allocator) {
    if (surface)
      cairosink->backend->destroy_surface (surface);
  } else {
    if (surface)
      cairo_surface_destroy (surface);
    if (map_info.data)
      gst_memory_unmap (mem, &map_info);
  }
  gst_structure_set (params, "return-value", GST_TYPE_FLOW_RETURN,
      return_value, NULL);
}

static GstFlowReturn
gst_cairo_sink_prepare (GstBaseSink * base_sink, GstBuffer * buf)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (base_sink);
  GstStructure *params;
  GstMemory *mem;
  GstFlowReturn ret;

  GST_TRACE_OBJECT (cairosink, "Got buffer %" GST_PTR_FORMAT, buf);

  if (!cairosink->caps)
    return GST_FLOW_NOT_NEGOTIATED;

  if (gst_buffer_n_memory (buf) != 1) {
    GST_ERROR_OBJECT (cairosink, "Handling of buffer with more than one "
        "GstMemory not implemented");
    return GST_FLOW_ERROR;
  }

  mem = gst_buffer_peek_memory (buf, 0);

  params = gst_structure_new ("copy-buffer",
      "cairosink", G_TYPE_POINTER, cairosink,
      "memory", G_TYPE_POINTER, mem,
      "width", G_TYPE_INT, cairosink->width,
      "height", G_TYPE_INT, cairosink->height, NULL);
  gst_cairo_thread_invoke_sync (cairosink->render_thread_info,
      (GstCairoThreadFunction) _copy_buffer, params);

  if (!gst_structure_get (params, "return-value", GST_TYPE_FLOW_RETURN, &ret,
          NULL)) {
    GST_WARNING_OBJECT (cairosink,
        "Misterious issue when copying buffer to target surface");
    ret = GST_FLOW_ERROR;
  }

  gst_structure_free (params);

  GST_TRACE_OBJECT (cairosink, "Returning %s", gst_flow_get_name (ret));

  return ret;
}

static void
_show_frame (GstStructure * params)
{
  cairo_surface_t *surface;
  GstCairoBackend *backend;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!gst_structure_get (params, "surface", G_TYPE_POINTER, &surface,
          "backend", G_TYPE_POINTER, &backend, NULL)) {
    ret = GST_FLOW_ERROR;
    goto end;
  }

  backend->show (surface);

end:
  gst_structure_set (params, "return-value", GST_TYPE_FLOW_RETURN, ret, NULL);
}


static GstFlowReturn
gst_cairo_sink_show_frame (GstVideoSink * video_sink, GstBuffer * buf)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (video_sink);
  GstStructure *params;
  GstFlowReturn ret;

  GST_TRACE_OBJECT (video_sink, "Need to show buffer %" GST_PTR_FORMAT, buf);

  params = gst_structure_new ("show-frame",
      "surface", G_TYPE_POINTER, cairosink->surface,
      "backend", G_TYPE_POINTER, cairosink->backend, NULL);
  gst_cairo_thread_invoke_sync (cairosink->render_thread_info,
      (GstCairoThreadFunction) _show_frame, params);

  if (!gst_structure_get (params, "return-value", GST_TYPE_FLOW_RETURN, &ret,
          NULL)) {
    GST_WARNING_OBJECT (cairosink,
        "Misterious issue when showing target surface");
    ret = GST_FLOW_ERROR;
  }

  GST_TRACE_OBJECT (cairosink, "Returning %s", gst_flow_get_name (ret));

  return ret;
}


static GstCairoSystem *
_get_system (GstCairoSystemType type)
{
  switch (type) {
#ifdef HAVE_CAIRO_EGL
    case GST_CAIRO_SYSTEM_EGL:
      return &gst_cairo_system_egl;
      break;
#endif
#ifdef HAVE_CAIRO_GLX
    case GST_CAIRO_SYSTEM_GLX:
      return &gst_cairo_system_glx;
      break;
#endif
    default:
      return NULL;
  }
}

static GstCairoBackend *
_get_backend (GstCairoBackendType type)
{
#if defined(USE_CAIRO_GL) || defined(USE_CAIRO_GLESV2)
  if (type != GST_CAIRO_BACKEND_GL)
    GST_ERROR ("Backend type not implemented: %d", type);
  return &gst_cairo_backend_gl;
#endif
  return NULL;
}

static gboolean
gst_cairo_sink_start (GstBaseSink * base_sink)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (base_sink);

  if (cairosink->system == NULL) {
    cairosink->system = _get_system (cairosink->system_type);
  }
  if (cairosink->backend == NULL) {
    cairosink->backend = _get_backend (cairosink->backend_type);
  }

  if (!cairosink->backend || !cairosink->system)
    goto error;

  if (!cairosink->render_main_context) {
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
    cairosink->render_thread_info =
        gst_cairo_thread_info_new (cairosink->render_main_context);
  }

  if (!cairosink->render_main_context)
    goto error;

  return TRUE;

error:
  if (cairosink->backend) {
    cairosink->backend = NULL;
  }

  if (cairosink->render_main_context) {
    g_main_context_unref (cairosink->render_main_context);
    cairosink->render_main_context = NULL;
  }

  if (cairosink->render_thread_info)
    gst_cairo_thread_info_destroy (cairosink->render_thread_info);

  if (cairosink->thread) {
    g_thread_unref (cairosink->thread);
    cairosink->thread = NULL;
  }

  GST_TRACE_OBJECT (cairosink, "Something, somewhere, went wrong");

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
    cairosink->backend = NULL;
  }

  if (cairosink->render_main_context) {
    g_main_context_unref (cairosink->render_main_context);
    cairosink->render_main_context = NULL;
  }

  if (cairosink->render_thread_info) {
    gst_cairo_thread_info_destroy (cairosink->render_thread_info);
    cairosink->render_thread_info = NULL;
  }

  if (cairosink->loop) {
    g_main_loop_quit (cairosink->loop);
    g_main_loop_unref (cairosink->loop);
    cairosink->loop = NULL;
  }

  return TRUE;
}

static void
_do_create_display_surface (GstStructure * params)
{
  guint width, height;
  cairo_surface_t *surface = NULL;
  GstCairoSink *cairosink;

  if (!gst_structure_get (params,
          "cairosink", G_TYPE_POINTER, &cairosink,
          "width", G_TYPE_UINT, &width, "height", G_TYPE_UINT, &height, NULL))
    return;

  surface = cairosink->system->create_display_surface (width, height);

  if (!surface) {
    GST_WARNING_OBJECT (cairosink, "Could not create display surface");
    return;
  }

  gst_structure_set (params, "surface", G_TYPE_POINTER, surface, NULL);
}

static cairo_surface_t *
gst_cairo_sink_create_display_surface (GstCairoSink * cairosink,
    guint width, guint height)
{
  GstStructure *params;
  cairo_surface_t *surface = NULL;

  params = gst_structure_new ("create-display-surface",
      "cairosink", G_TYPE_POINTER, cairosink,
      "width", G_TYPE_UINT, width, "height", G_TYPE_UINT, height, NULL);

  gst_cairo_thread_invoke_sync (cairosink->render_thread_info,
      (GstCairoThreadFunction) _do_create_display_surface, params);

  gst_structure_get (params, "surface", G_TYPE_POINTER, &surface, NULL);

  gst_structure_free (params);

  return surface;
}


static gboolean
gst_cairo_sink_set_caps (GstBaseSink * base_sink, GstCaps * caps)
{
  GstCairoSink *cairosink = GST_CAIRO_SINK (base_sink);
  GstStructure *structure;
  gint width, height;
  GstGLBufferPool *glpool;

  GST_TRACE_OBJECT (cairosink, "set_caps(%" GST_PTR_FORMAT ")", caps);
  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "width", &width)
      || !gst_structure_get_int (structure, "height", &height))
    return FALSE;

  /* query_can_map() should work in any thread as it is supposed to create its
   * own context. We might have to change that in the future */
  if (cairosink->system->query_can_map (cairosink->surface)) {
    if (!cairosink->allocator)
      cairosink->allocator = (GstAllocator *)
          gst_gl_allocator_new (cairosink->render_thread_info);

    glpool = (GstGLBufferPool *) cairosink->buffer_pool;
    if (glpool && (glpool->width != width || glpool->height != height)) {
      gst_object_unref (cairosink->buffer_pool);
      cairosink->buffer_pool = NULL;
    }

    if (!cairosink->buffer_pool) {
      cairosink->buffer_pool = (GstBufferPool *)
          gst_gl_buffer_pool_new (((GstGLAllocator *) cairosink->allocator),
          caps);
    }
  }

  /* FIXME: create cairosink->surface here? */
  if (cairosink->surface == NULL)
    cairosink->surface = gst_cairo_sink_create_display_surface (cairosink,
        width, height);

  if (!cairosink->surface)
    return FALSE;

  if (!cairosink->device)
    cairosink->device = cairo_surface_get_device (cairosink->surface);

  cairosink->caps = gst_caps_ref (caps);
  cairosink->width = width;
  cairosink->height = height;

  return TRUE;
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

  structure = gst_caps_get_structure (caps, 0);
  if (gst_structure_get_int (structure, "width", &width)
      && gst_structure_get_int (structure, "height", &height))
    size = width * height * 4;

  if (!size) {
    GST_ERROR_OBJECT (cairosink, "No caps or bad caps");
    return FALSE;
  }


  if (need_pool && !cairosink->buffer_pool) {
    GstStructure *config;

    /* need to go with a generic buffer pool */
    GST_INFO_OBJECT (cairosink, "Providing a generic buffer pool");

    cairosink->buffer_pool = gst_buffer_pool_new ();
    config = gst_buffer_pool_get_config (cairosink->buffer_pool);
    gst_buffer_pool_config_set_params (config, cairosink->caps, size, 2, 0);
    gst_buffer_pool_set_config (cairosink->buffer_pool, config);
  }

  if (cairosink->buffer_pool) {
    gst_query_add_allocation_pool (query, cairosink->buffer_pool, size, 2, 0);
    GST_DEBUG_OBJECT (cairosink,
        "adding pool %" GST_PTR_FORMAT " to query %" GST_PTR_FORMAT " (%p)",
        cairosink->buffer_pool, query, query);

    ret = TRUE;
  }

  return ret;
}
