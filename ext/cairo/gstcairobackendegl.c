#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <cairo-gl.h>

#include "gstcairobackendegl.h"
#include <gst/gst.h>

#include <glib.h>

static gboolean gst_cairo_backend_egl_create_display_surface (gint
    width, gint height, cairo_device_t ** device, cairo_surface_t ** surface);
static cairo_surface_t *gst_cairo_backend_egl_create_surface (cairo_device_t *
    device, gint width, gint height);
static void gst_cairo_backend_egl_get_size (cairo_surface_t * surface,
    gint * width, gint * height);

static gboolean gst_cairo_backend_egl_query_can_map (cairo_surface_t * surface);

static EGLint multisampleAttributes[] = {
  EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
  EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
  EGL_RED_SIZE, 1,              /* the minmal number of bits per component    */
  EGL_GREEN_SIZE, 1,
  EGL_BLUE_SIZE, 1,
  EGL_STENCIL_SIZE, 1,
  EGL_SAMPLES, 4,
  EGL_SAMPLE_BUFFERS, 1,
  EGL_NONE
};

static EGLint singleSampleAttributes[] = {
  EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
  EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
  EGL_RED_SIZE, 1,              /* the minmal number of bits per component    */
  EGL_GREEN_SIZE, 1,
  EGL_BLUE_SIZE, 1,
  EGL_STENCIL_SIZE, 1,
  EGL_NONE
};

GstCairoBackend *
gst_cairo_backend_egl_new (void)
{
  GstCairoBackend *backend = g_slice_new (GstCairoBackend);

  backend->create_display_surface =
      gst_cairo_backend_egl_create_display_surface;
  backend->create_surface = gst_cairo_backend_egl_create_surface;
  backend->show = cairo_gl_surface_swapbuffers;
  backend->need_own_thread = TRUE;
  backend->get_size = gst_cairo_backend_egl_get_size;
  backend->query_can_map = gst_cairo_backend_egl_query_can_map;

  return backend;
}

void
gst_cairo_backend_egl_destroy (GstCairoBackend * backend)
{
  g_slice_free (GstCairoBackend, backend);
}

static Display *
get_display (void)
{
  static Display *display = NULL;
  if (display)
    return NULL;

  display = XOpenDisplay (NULL);
  if (!display) {
    GST_ERROR ("Could not open display.");
    return NULL;
  }
  return display;
}

/* Mostly cut and paste from glx-utils.c in cairo-gl-smoke-tests */
static gboolean
gst_cairo_backend_egl_create_display_surface (gint width, gint height,
    cairo_device_t ** device, cairo_surface_t ** surface)
{
  Window window;
  Display *display;

  EGLDisplay egl_display;
  EGLContext egl_context;
  EGLSurface egl_surface;
  EGLConfig egl_config;
  EGLint major, minor;
  EGLint num;

  EGLint ctx_attrs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };

  cairo_device_t *cairo_device;
  cairo_surface_t *cairo_surface;

  display = get_display ();
  if (!display)
    return FALSE;

  /* Create the XWindow. */
  window = XCreateSimpleWindow (display, DefaultRootWindow (display),
      0, 0, width, height, 0, 0, 0);

  egl_display = eglGetDisplay ((EGLNativeDisplayType) display);
  if (egl_display == EGL_NO_DISPLAY)
    goto CLEANUP_X;

  if (!eglInitialize (egl_display, &major, &minor))
    goto CLEANUP_DISPLAY;

  if (!eglBindAPI (EGL_OPENGL_ES_API))
    goto CLEANUP_DISPLAY;

  if (!eglChooseConfig (egl_display, multisampleAttributes, &egl_config,
          1, &num) || num == 0) {
    if (!eglChooseConfig (egl_display, singleSampleAttributes, &egl_config,
            1, &num))
      goto CLEANUP_DISPLAY;
  }

  if (num != 1)
    goto CLEANUP_DISPLAY;

  egl_context = eglCreateContext (egl_display, egl_config, NULL, ctx_attrs);
  if (egl_context == EGL_NO_CONTEXT)
    goto CLEANUP_DISPLAY;

  egl_surface = eglCreateWindowSurface (egl_display, egl_config,
      (NativeWindowType) window, NULL);
  if (egl_surface == EGL_NO_SURFACE)
    goto CLEANUP_CONTEXT;

  XMapWindow (display, window);
  XFlush (display);

  cairo_device = cairo_egl_device_create (egl_display, egl_context);
  cairo_surface = cairo_gl_surface_create_for_egl (cairo_device, egl_surface,
      width, height);

  *surface = cairo_surface;
  *device = cairo_device;
  return TRUE;

CLEANUP_CONTEXT:
  eglDestroyContext (egl_display, egl_context);
CLEANUP_DISPLAY:
  eglTerminate (egl_display);
CLEANUP_X:
  XDestroyWindow (display, window);
  XCloseDisplay (display);
  return FALSE;
}

static cairo_surface_t *
gst_cairo_backend_egl_create_surface (cairo_device_t * device, gint width,
    gint height)
{

  return cairo_gl_surface_create (device, CAIRO_CONTENT_COLOR, width, height);
}

static void
gst_cairo_backend_egl_get_size (cairo_surface_t * surface, gint * width,
    gint * height)
{
  *width = cairo_gl_surface_get_width (surface);
  *height = cairo_gl_surface_get_height (surface);
}

static gboolean
gst_cairo_backend_egl_query_can_map (cairo_surface_t * surface)
{
  /* map not implemented yet */
  return FALSE;
}
