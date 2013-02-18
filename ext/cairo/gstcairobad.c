/* GStreamer
 * Copyright (C) 2013 Samsung Electronics
 *   @author: Guillaume Emont <guijemont@igalia.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gstcairosink.h>

#include <string.h>
#include <math.h>

GST_DEBUG_CATEGORY (gst_cairo_sink_debug_category);

static gboolean
plugin_init (GstPlugin * plugin)
{
  gst_element_register (plugin, "cairosink", GST_RANK_NONE,
      GST_TYPE_CAIRO_SINK);

  GST_DEBUG_CATEGORY_INIT (gst_cairo_sink_debug_category, "cairosink", 0,
      "debug category for cairosink element");

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, cairobad,
    "Cairo-based elements - bad", plugin_init, VERSION,
    GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
