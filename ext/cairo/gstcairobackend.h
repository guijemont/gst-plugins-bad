#include <cairo.h>

typedef struct _GstCairoBackend GstCairoBackend;

/**
  A GstCairoBackend handles all the things that are specific to a surface type.
  Some surface types might have two separate back ends (that could share some
  code). For instance, there might be several back ends for gl surfaces:
  EGL/GLES, GLX, etc...
 */
struct _GstCairoBackend {
  cairo_surface_t *     (*create_surface)       (void);
};

/* This needs to be different from cairo_device_type_t because we want to make
 * a difference between variants of OpenGL/OpenGLES */
enum GstCairoBackendType {
  GST_CAIRO_BACKEND_GLX         = 1,
  GST_CAIRO_BACKEND_XLIB        = 2,
};
#define GST_CAIRO_BACKEND_LAST 2

GstCairoBackend * gst_cairo_backend_new (GstCairoBackendType backend_type);

gst_cairo_backend_destroy (GstCairoBackend *backend);
