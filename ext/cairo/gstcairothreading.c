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

#include "gstcairothreading.h"

#include <gst/gst.h>

typedef struct
{
  GstCairoThreadFunction function;
  gpointer user_data;
  GMutex *mutex;
  GCond *cond;
  gpointer *last_data;
} GstCairoFunctionInfo;

static gboolean
_function_wrapper (GstCairoFunctionInfo * function_info)
{
  g_mutex_lock (function_info->mutex);
  {
    function_info->function (function_info->user_data);
  }
  *function_info->last_data = function_info->user_data;
  g_cond_signal (function_info->cond);
  g_mutex_unlock (function_info->mutex);

  return FALSE;
}

/* Runs function in context, which sits in another thread, and waits for
 * function to return. */
void
gst_cairo_main_context_invoke_sync (GMainContext * context,
    GMutex * mutex,
    GCond * cond,
    gpointer * last_data, GstCairoThreadFunction function, gpointer user_data)
{
  g_mutex_lock (mutex);
  {
    GstCairoFunctionInfo function_info =
        { function, user_data, mutex, cond, last_data };

    g_main_context_invoke (context, (GSourceFunc) _function_wrapper,
        &function_info);

    g_main_context_wakeup (context);

    do {
      GST_TRACE ("waiting for cond");
      g_cond_wait (cond, mutex);
    } while (*last_data != user_data);
    GST_TRACE ("got cond");
  }
  g_mutex_unlock (mutex);
}

GstCairoThreadInfo *
gst_cairo_thread_info_new (GMainContext * context)
{
  GstCairoThreadInfo *thread_info;

  thread_info = g_slice_new (GstCairoThreadInfo);

  thread_info->context = g_main_context_ref (context);
  g_mutex_init (&thread_info->mutex);
  g_cond_init (&thread_info->cond);
  thread_info->last_data = NULL;

  return thread_info;
}

void
gst_cairo_thread_info_destroy (GstCairoThreadInfo * thread_info)
{
  g_main_context_unref (thread_info->context);
  g_mutex_clear (&thread_info->mutex);
  g_cond_clear (&thread_info->cond);
  g_slice_free (GstCairoThreadInfo, thread_info);
}
