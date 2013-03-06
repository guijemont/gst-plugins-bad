#ifndef _GST_CAIRO_BACKEND_EGL_H_
#define _GST_CAIRO_BACKEND_EGL_H_

#include "gstcairobackend.h"

GstCairoBackend *gst_cairo_backend_egl_new (void);

void gst_cairo_backend_egl_destroy (GstCairoBackend *backend);

#endif /* _GST_CAIRO_BACKEND_EGL_H_ */
