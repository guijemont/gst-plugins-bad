#ifndef _GST_CAIRO_BACKEND_GLX_H_
#define _GST_CAIRO_BACKEND_GLX_H_

#include "gstcairobackend.h"

GstCairoBackend *gst_cairo_backend_glx_new (void);

void gst_cairo_backend_glx_destroy (GstCairoBackend *backend);

#endif /* _GST_CAIRO_BACKEND_GLX_H_ */
