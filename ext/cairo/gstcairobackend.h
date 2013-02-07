#ifndef _GST_CAIRO_BACKEND_H_
#define _GST_CAIRO_BACKEND_H_

#include <cairo.h>

typedef struct _GstCairoBackend GstCairoBackend;

/**
  A GstCairoBackend handles all the things that are specific to a surface type.
  Some surface types might have two separate back ends (that could share some
  code). For instance, there might be several back ends for gl surfaces:
  EGL/GLES, GLX, etc...
 */
struct _GstCairoBackend
{
  cairo_surface_t *(*create_surface) (gint width, gint height);

  gboolean need_own_thread;
  GThread *thread;
  GMainContext *thread_context;
  GMainLoop *loop;
  GSource *source;
  GstDataQueue *queue;
};

typedef enum
{
  GST_CAIRO_BACKEND_GLX = 1,
  GST_CAIRO_BACKEND_XLIB = 2,
} GstCairoBackendType;
#define GST_CAIRO_BACKEND_LAST 2

GstCairoBackend *gst_cairo_backend_new (GstCairoBackendType backend_type,
    GMainContext * context);
void gst_cairo_backend_use_main_context (GstCairoBackend * backend,
    GMainContext * context);
void gst_cairo_backend_create_thread (GstCairoBackend * backend);

#endif /* _GST_CAIRO_BACKEND_H_ */
