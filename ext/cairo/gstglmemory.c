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

#ifdef USE_CAIRO_GL
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#define CAIROSINK_USE_PBO
#elif USE_CAIRO_GLESV2
#include <GLES2/gl2.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstcairogldebug.h"
#include "gstglmemory.h"

static void gst_gl_allocator_free (GstAllocator * allocator,
    GstMemory * memory);

#ifdef CAIROSINK_USE_PBO
static gpointer gst_gl_allocator_mem_map (GstMemory * mem, gsize maxsize,
    GstMapFlags flags);
static void gst_gl_allocator_mem_unmap (GstMemory * mem);
#endif

static GstFlowReturn gst_gl_buffer_pool_alloc_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params);

G_DEFINE_TYPE (GstGLAllocator, gst_gl_allocator, GST_TYPE_ALLOCATOR);

static void
gst_gl_allocator_class_init (GstGLAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);

  allocator_class->alloc = NULL;
  allocator_class->free = gst_gl_allocator_free;
}

static void
gst_gl_allocator_init (GstGLAllocator * glallocator)
{
  GstAllocator *allocator = GST_ALLOCATOR_CAST (glallocator);

  allocator->mem_type = GST_GL_MEMORY_TYPE;
#ifdef CAIROSINK_USE_PBO
  allocator->mem_map = gst_gl_allocator_mem_map;
  allocator->mem_unmap = gst_gl_allocator_mem_unmap;
#endif
}

GstGLAllocator *
gst_gl_allocator_new (GstCairoThreadInfo * thread_info)
{
  GstGLAllocator *allocator = g_object_new (gst_gl_allocator_get_type (),
      NULL);

  allocator->thread_info = thread_info;

  return allocator;
}

static void
_do_alloc (GstStructure * structure)
{
  GstMemory *mem;
  GstGLMemory *glmem;
  GstAllocator *allocator;
  GstGLAllocator *glallocator;
  gint width, height;
  guint data_size;

  if (!gst_structure_get (structure, "width", G_TYPE_INT, &width,
          "height", G_TYPE_INT, &height,
          "allocator", G_TYPE_POINTER, &allocator, NULL))
    return;

  data_size = width * height * 4;
  glmem = g_slice_new (GstGLMemory);
  mem = (GstMemory *) glmem;
  gst_memory_init (mem, 0, allocator, NULL, data_size, 0, 0, data_size);

  glmem->data_size = data_size;
  glmem->width = width;
  glmem->height = height;

  /* FIXME: would we need to find a way to cairo_device_acqure/release here?
   */
  glallocator = (GstGLAllocator *) allocator;
  if (glallocator->acquire_context && glallocator->release_context) {
    if (glallocator->acquire_context (glallocator->gst_element)) {
      glGenTextures (1, &glmem->texture);
      glBindTexture (GL_TEXTURE_2D, glmem->texture);
      glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA,
          GL_UNSIGNED_BYTE, NULL);
      glBindTexture (GL_TEXTURE_2D, 0);

#ifdef CAIROSINK_USE_PBO
      /* create PBO */
      glGenBuffers (1, &glmem->pbo);
      glBindBuffer (GL_PIXEL_UNPACK_BUFFER, glmem->pbo);
      glBufferData (GL_PIXEL_UNPACK_BUFFER, glmem->data_size, NULL, GL_STREAM_DRAW);
      glBindBuffer (GL_PIXEL_UNPACK_BUFFER, 0);
#endif
      glallocator->release_context (glallocator->gst_element);
    }
  }

  gst_structure_set (structure, "memory", G_TYPE_POINTER, glmem, NULL);
}

GstBuffer *
gst_gl_allocator_alloc_buffer (GstAllocator * allocator, guint width,
    guint height)
{
  GstGLAllocator *glallocator = (GstGLAllocator *) allocator;
  GstStructure *structure;
  GstMemory *mem;
  GstBuffer *buffer = NULL;

  structure = gst_structure_new ("gl-allocate",
      "allocator", G_TYPE_POINTER, allocator,
      "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);

  gst_cairo_thread_invoke_sync (glallocator->thread_info,
      (GstCairoThreadFunction) _do_alloc, structure);

  if (!gst_structure_get (structure, "memory", G_TYPE_POINTER, &mem, NULL))
    goto beach;

  buffer = gst_buffer_new ();
  gst_buffer_add_video_meta (buffer, 0, GST_VIDEO_FORMAT_BGRx, width, height);

  gst_buffer_append_memory (buffer, mem);

beach:
  if (!mem) {
    GST_WARNING_OBJECT (glallocator, "Could not allocate");
    /* FIXME: provide fallback */
  }

  gst_structure_free (structure);

  GST_TRACE ("Returning memory %" GST_PTR_FORMAT, buffer);
  return buffer;
}

static void
_do_free (GstStructure * structure)
{
  GstGLMemory *glmem;
  GstGLAllocator *glallocator;

  if (!gst_structure_get (structure, "memory", G_TYPE_POINTER, &glmem,
          "allocator", G_TYPE_POINTER, &glallocator, NULL))
    return;

  if (glallocator->acquire_context && glallocator->release_context) {
    if (glallocator->acquire_context (glallocator->gst_element)) {
      glDeleteBuffers (1, &glmem->pbo);
      glDeleteTextures (1, &glmem->texture);
      glallocator->release_context (glallocator->gst_element);
    }
  }
  g_slice_free (GstGLMemory, glmem);
}

static void
gst_gl_allocator_free (GstAllocator * allocator, GstMemory * memory)
{
  GstStructure *structure;
  GstGLAllocator *glallocator = (GstGLAllocator *) allocator;

  structure = gst_structure_new ("gl-free",
      "memory", G_TYPE_POINTER, memory,
      "allocator", G_TYPE_POINTER, glallocator, NULL);

  gst_cairo_thread_invoke_sync (glallocator->thread_info,
      (GstCairoThreadFunction) _do_free, structure);

  gst_structure_free (structure);
}

#ifdef CAIROSINK_USE_PBO
static void
_do_map (GstStructure * structure)
{
  GstGLMemory *glmem;
  GstGLAllocator *glallocator;
  gpointer data_area;

  if (!gst_structure_get (structure, "memory", G_TYPE_POINTER, &glmem,
          "allocator", G_TYPE_POINTER, &glallocator, NULL))
    return;

  if (glallocator->acquire_context && glallocator->release_context) {
    if (glallocator->acquire_context (glallocator->gst_element)) {
      gl_debug ("GL: map");
      glBindBuffer (GL_PIXEL_UNPACK_BUFFER, glmem->pbo);

      /* We put a NULL buffer so that GL discards the current buffer if 
       * it is still being used, instead of waiting for the end of an 
       * operation on it. Makes sure the call to glMapBuffer() won't 
       * cause a sync */
      glBufferData (GL_PIXEL_UNPACK_BUFFER, glmem->data_size, NULL,
          GL_STREAM_DRAW);

      data_area = glMapBuffer (GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);

      glBindBuffer (GL_PIXEL_UNPACK_BUFFER, 0);
    }
  }

  gst_structure_set (structure, "data-area", G_TYPE_POINTER, data_area, NULL);

  gl_debug ("exit");
}

static gpointer
gst_gl_allocator_mem_map (GstMemory * mem, gsize maxsize, GstMapFlags flags)
{
  GstStructure *structure;
  GstGLAllocator *glallocator = (GstGLAllocator *) mem->allocator;
  gpointer data_area = NULL;

  structure =
      gst_structure_new ("gl-map", "memory", G_TYPE_POINTER, mem,
      "allocator", G_TYPE_POINTER, glallocator, NULL);

  gst_cairo_thread_invoke_sync (glallocator->thread_info,
      (GstCairoThreadFunction) _do_map, structure);

  if (!gst_structure_get (structure, "data-area", G_TYPE_POINTER, &data_area,
          NULL)) {
    GST_WARNING_OBJECT (glallocator, "Could not map");
  }

  gst_structure_free (structure);

  return data_area;
}

static void
_do_unmap (GstStructure * structure)
{
  GstGLMemory *glmem;
  GstGLAllocator *glallocator;

  if (!gst_structure_get (structure, "memory", G_TYPE_POINTER, &glmem,
          "allocator", G_TYPE_POINTER, &glallocator, NULL))
    return;

  if (glallocator->acquire_context && glallocator->release_context) {
    gl_debug ("GL: unmap");
    glBindBuffer (GL_PIXEL_UNPACK_BUFFER, glmem->pbo);
    glUnmapBuffer (GL_PIXEL_UNPACK_BUFFER);

    glBindTexture (GL_TEXTURE_2D, glmem->texture);
    /* copies data from pbo to texture */
    glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, glmem->width,
        glmem->height, GL_BGRA, GL_UNSIGNED_BYTE, 0);

    glBindBuffer (GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture (GL_TEXTURE_2D, 0);
    glallocator->release_context (glallocator->gst_element);
    gl_debug ("exit");
  }
}

static void
gst_gl_allocator_mem_unmap (GstMemory * mem)
{
  GstStructure *structure;
  GstGLAllocator *glallocator = (GstGLAllocator *) mem->allocator;

  structure =
      gst_structure_new ("gl-unmap", "memory", G_TYPE_POINTER, mem,
      "allocator", G_TYPE_POINTER, glallocator, NULL);

  gst_cairo_thread_invoke_sync (glallocator->thread_info,
      (GstCairoThreadFunction) _do_unmap, structure);

  gst_structure_free (structure);
}
#endif /* CAIROSINK_USE_PBO */

/* buffer pool */

G_DEFINE_TYPE (GstGLBufferPool, gst_gl_buffer_pool, GST_TYPE_VIDEO_BUFFER_POOL);

static void
gst_gl_buffer_pool_class_init (GstGLBufferPoolClass * klass)
{
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  gstbufferpool_class->alloc_buffer = gst_gl_buffer_pool_alloc_buffer;
}

static void
gst_gl_buffer_pool_init (GstGLBufferPool * pool)
{
}

static GstFlowReturn
gst_gl_buffer_pool_alloc_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstGLBufferPool *glpool = (GstGLBufferPool *) pool;

  *buffer =
      gst_gl_allocator_alloc_buffer (GST_ALLOCATOR_CAST (glpool->allocator),
      glpool->width, glpool->height);

  if (!*buffer)
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

GstGLBufferPool *
gst_gl_buffer_pool_new (GstGLAllocator * allocator, GstCaps * caps)
{
  GstStructure *config, *caps_structure;
  gsize size;
  gint width, height;
  GstGLBufferPool *pool;

  caps_structure = gst_caps_get_structure (caps, 0);
  if (!gst_structure_get_int (caps_structure, "width", &width)
      || !gst_structure_get_int (caps_structure, "height", &height))
    return NULL;

  size = width * height * 4;

  pool = g_object_new (gst_gl_buffer_pool_get_type (), NULL);

  pool->allocator = allocator;
  pool->width = width;
  pool->height = height;

  config = gst_buffer_pool_get_config (GST_BUFFER_POOL_CAST (pool));
  gst_buffer_pool_config_set_allocator (config, GST_ALLOCATOR (allocator),
      NULL);
  gst_buffer_pool_config_set_params (config, caps, size, 2, 0);
  gst_buffer_pool_set_config (GST_BUFFER_POOL_CAST (pool), config);

  return pool;
}
