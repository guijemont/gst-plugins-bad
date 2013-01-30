#include "gstcairobackend.h"
#ifdef HAVE_GLX
#include "gstcairobackendglx.h"
#endif

GstCairoBackend *
gst_cairo_backend_new (GstCairoBackendType backend_type, int width, int height)
{
  GstCairoBackend *backend = NULL;
  switch (backend_type) {
#ifdef HAVE_GLX
    case GST_CAIRO_BACKEND_GLX:
      return gst_cairo_backend_glx_new (width, height);
      break;
#endif
    default:
      GST_ERROR ("Unhandled backend type: %d", backend_type);
      return NULL;
  }
}

void
gst_cairo_backend_destroy (GstCairoBackend * backend)
{
  switch (backend_type) {
#ifdef HAVE_GLX
    case GST_CAIRO_BACKEND_GLX:
      gst_cairo_backend_glx_destroy (backend);
      break;
#endif
    default:
      GST_ERROR ("Unhandled backend type: %d", backend_type);
  }
}
