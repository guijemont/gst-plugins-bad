#include "gstcairobackend.h"
#ifdef HAVE_GLX
#include "gstcairobackendglx.h"
#endif

static gpointer gst_cairo_backend_thread_init (gpointer data);

GstCairoBackend *
gst_cairo_backend_new (GstCairoBackendType backend_type)
{
  GstCairoBackend *backend = NULL;

  switch (backend_type) {
#ifdef HAVE_GLX
    case GST_CAIRO_BACKEND_GLX:
      backend = gst_cairo_backend_glx_new ();
      break;
#endif
    default:
      GST_ERROR ("Unhandled backend type: %d", backend_type);
  }

  if (backend && backend->need_own_thread) {
    GError *error = NULL;
    backend->thread =
        g_thread_try_new ("backend thread", gst_cairo_backend_thread_init,
        backend, &error);
    if (!backend->thread) {
      GST_ERROR ("Could not create backend thread: %s", error->message);
      gst_cairo_backend_destroy (backend);
      backend = NULL;
      g_error_free (error);
    }
  }

  return backend;
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

  if (backend->loop) {
    g_main_loop_quit (backend->loop);
    g_main_loop_unref (backend->loop);
    backend->loop = NULL;
  }
  if (backend->thread) {
    g_thread_unref (backend->thread);
    backend->thread = NULL;
  }
  if (backend->thread_context) {
    g_main_context_unref (backend->thread_context);
    backend->thread_context = NULL;
  }
}

static gpointer
gst_cairo_backend_thread_init (gpointer data)
{
  GstCairoBackend *backend = (GstCairoBackend *) data;

  backend->thread_context = g_main_context_new ();
  g_main_context_push_thread_default (backend->thread_context);
  backned->loop = g_main_loop_new (backend->thread_context, FALSE);
  g_main_loop_run (backend->loop);
  return NULL;
}

void
gst_cairo_backend_use_main_context (GstCairoBackend * backend,
    GMainContext * context)
{
  if (context == NULL)
    return;

  if (backend->thread_context) {
    GST_ERROR ("Cannot change main context once it's been set");
    return;
  }
  backend->thread_context = g_main_context_ref (context);
}
