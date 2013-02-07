#include "gstcairobackend.h"
#ifdef HAVE_GLX
#include "gstcairobackendglx.h"
#endif

static gpointer gst_cairo_backend_thread_init (gpointer data);

static gboolean
gst_cairo_backend_source_prepare (GSource * source, gint * timeout_);
static gboolean gst_cairo_backend_source_check (GSource * source);
static gboolean gst_cairo_backend_source_dispatch (GSource * source,
    GSourceFunc callback, gpointer user_data);

typedef struct
{
  GSource parent;
  GstCairoBackend *backend;
} CairoBackendSource;

GSourceFuncs gst_cairo_backend_source_funcs = {
  gst_cairo_backend_source_prepare,
  gst_cairo_backend_source_check,
  gst_cairo_backend_source_dispatch,
};

static gboolean
queue_check_full_func (GstDataQueue * queue, guint visible, guint bytes,
    guint64 time, gpointer checkdata)
{
  return visible != 0;
}

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

  if (backend && backend->need_own_thread && !backend->thread_context) {
    GError *error = NULL;

    backend->thread_context = g_main_context_new ();
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

  if (backend->thread_context) {
    backend->queue =
        gst_data_queue_new (queue_check_full_func, NULL, NULL, NULL);
    backend->source =
        g_source_new (&gst_cairo_backend_source_funcs,
        sizeof (gst_cairo_backend_source_funcs));
    g_source_attach (backend->source, backend->thread_context);
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

  if (backend->source) {
    g_source_destroy (backend->source);
    backend->source = NULL;
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

  g_main_context_push_thread_default (backend->thread_context);
  backend->loop = g_main_loop_new (backend->thread_context, FALSE);
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

static gboolean
gst_cairo_backend_source_prepare (GSource * source, gint * timeout_)
{
  return gst_cairo_backend_source_check (source);
}

static gboolean
gst_cairo_backend_source_check (GSource * source)
{
  CairoBackendSource *backend_source = (CairoBackendSource *) source;

  return gst_data_queue_is_full (backend_source->backend->queue);
}

static gboolean
gst_cairo_backend_source_dispatch (GSource * source,
    GSourceFunc callback, gpointer user_data)
{
  GstDataQueueItem *item = NULL;
  CairoBackendSource *backend_source = (CairoBackendSource *) source;
  GstCairoBackend *backend;
  GstMiniObject *object;

  backend = backend_source->backend;

  if (!gst_data_queue_pop (backend->queue, &item))
    return FALSE;

  object = item->object;

  if (GST_IS_CAPS (object)) {
  } else if (GST_IS_QUERY (object)) {
  }


  return TRUE;
}
