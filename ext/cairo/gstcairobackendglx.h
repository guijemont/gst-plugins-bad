#ifndef _GST_CAIRO_BACKEND_GLX_H_
#define _GST_CAIRO_BACKEND_GLX_H_

#include "gstcairobackend.h"

#define GL_VERSION_ENCODE(major, minor) ( \
  ((major) * 256) + ((minor) * 1))

GstCairoBackend *gst_cairo_backend_glx_new (void);

void gst_cairo_backend_glx_destroy (GstCairoBackend *backend);

#endif /* _GST_CAIRO_BACKEND_GLX_H_ */
