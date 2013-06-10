#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <EGL/egl.h>

#include <cairo-gl.h>
#include <gst/gst.h>

#include "gstcairosystem.h"

static cairo_surface_t *_egl_create_display_surface (gint width, gint height);

static gboolean _egl_query_can_map (cairo_surface_t * surface);


typedef struct
{
  GstCairoSystem parent;
  Display *display;
} GstCairoSystemEGL;

GstCairoSystemEGL gst_cairo_system_egl = {
  {
        _egl_create_display_surface,
        _egl_query_can_map,
      GST_CAIRO_SYSTEM_EGL},
  NULL,                         /* display */
};

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

static void
create_display (GstCairoSystemEGL * system)
{
  if (system->display)
    return;

  system->display = XOpenDisplay (NULL);
  if (!system->display)
    GST_ERROR ("Could not open display.");
}

/* Mostly cut and paste from glx-utils.c in cairo-gl-smoke-tests */
static cairo_surface_t *
_egl_create_display_surface (gint width, gint height)
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

  if (gst_cairo_system_egl.display)
    /* We already created display and surface and don't keep track of the
     * surface. */
    return NULL;

  create_display (&gst_cairo_system_egl);
  display = gst_cairo_system_egl.display;


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

  return cairo_surface;

CLEANUP_CONTEXT:
  eglDestroyContext (egl_display, egl_context);
CLEANUP_DISPLAY:
  eglTerminate (egl_display);
CLEANUP_X:
  GST_WARNING ("Could not initialise EGL context correctly");
  XDestroyWindow (display, window);
  XCloseDisplay (display);
  return NULL;
}

static gboolean
_egl_query_can_map (cairo_surface_t * surface)
{
  /* map not implemented yet */
  return FALSE;
}
