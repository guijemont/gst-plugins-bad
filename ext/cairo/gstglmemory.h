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
#ifndef _GST_GL_MEMORY_H_
#define _GST_GL_MEMORY_H_
#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstcairothreading.h"

#define GST_GL_MEMORY_TYPE "GL"
#define GST_CAPS_FEATURE_MEMORY_GL "memory:GL"

typedef struct _GstGLMemory GstGLMemory;

struct _GstGLMemory
{
    GstMemory parent;

    guint texture;
    guint pbo;
    guint data_size;
    guint width;
    guint height;
};

typedef struct _GstGLAllocator GstGLAllocator;
typedef GstAllocatorClass GstGLAllocatorClass;

typedef struct _GstGLBufferPool GstGLBufferPool;
typedef GstVideoBufferPoolClass GstGLBufferPoolClass;

typedef gboolean (*GstGLAcquireContextFunc) (gpointer user_data);
typedef void (*GstGLReleaseContextFunc) (gpointer user_data);


struct _GstGLAllocator
{
    GstAllocator parent;

    GstCairoThreadInfo *thread_info;

    GstGLAcquireContextFunc acquire_context;
    GstGLReleaseContextFunc release_context;
    gpointer user_data;
};

struct _GstGLBufferPool
{
    GstVideoBufferPool parent;

    GstGLAllocator *allocator;
    guint width;
    guint height;
    /* for now, we assume RGBA */
};

GType gst_gl_allocator_get_type (void);
GType gst_gl_buffer_pool_get_type (void);

GstGLAllocator * gst_gl_allocator_new (GstCairoThreadInfo *thread_info,
        GstGLAcquireContextFunc acquire_context,
        GstGLReleaseContextFunc release_context, gpointer user_data);

GstBuffer * gst_gl_allocator_alloc_buffer (GstAllocator * allocator,
        guint width, guint height);

GstGLBufferPool * gst_gl_buffer_pool_new (GstGLAllocator *allocator,
        GstCaps *caps);

#endif /* _GST_GL_MEMORY_H_ */