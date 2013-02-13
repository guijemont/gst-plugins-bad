#include <GL/gl.h>
#include <GL/glx.h>

#include <cairo-gl.h>

#include "gstcairobackendglx.h"
#include <gst/gst.h>

static cairo_surface_t *gst_cairo_backend_glx_create_surface (gint width,
    gint height);

static int multisampleAttributes[] = {
  GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
  GLX_RENDER_TYPE, GLX_RGBA_BIT,
  GLX_DOUBLEBUFFER, True,       /* Request a double-buffered color buffer with */
  GLX_RED_SIZE, 1,              /* the maximum number of bits per component    */
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_STENCIL_SIZE, 1,
  GLX_SAMPLES, 4,
  GLX_SAMPLE_BUFFERS, 1,
  None
};

static int singleSampleAttributes[] = {
  GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
  GLX_RENDER_TYPE, GLX_RGBA_BIT,
  GLX_DOUBLEBUFFER, True,       /* Request a double-buffered color buffer with */
  GLX_RED_SIZE, 1,              /* the maximum number of bits per component    */
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_STENCIL_SIZE, 1,
  None
};

GstCairoBackend *
gst_cairo_backend_glx_new (void)
{
  GstCairoBackend *backend = g_slice_new (GstCairoBackend);

  backend->create_surface = gst_cairo_backend_glx_create_surface;
  backend->need_own_thread = TRUE;

  return backend;
}

void
gst_cairo_backend_glx_destroy (GstCairoBackend * backend)
{
  g_slice_free (GstCairoBackend, backend);
}

static Display *
get_display (void)
{
  static Display *display = NULL;
  if (display)
    return display;

  display = XOpenDisplay (NULL);
  if (!display) {
    GST_ERROR ("Could not open display.");
    return NULL;
  }
  return display;
}

static Bool
waitForNotify (Display * dpy, XEvent * event, XPointer arg)
{
  return (event->type == MapNotify) && (event->xmap.window == (Window) arg);
}

/* Mostly cut and paste from glx-utils.c in cairo-gl-smoke-tests */
static cairo_surface_t *
gst_cairo_backend_glx_create_surface (int width, int height)
{
  Window window, root_window;
  Display *display;
  XVisualInfo *visual_info;
  XSetWindowAttributes window_attributes;
  XEvent event;
  GLXContext glx_context;
  GLXFBConfig *fb_configs;

  cairo_device_t *device;
  cairo_surface_t *surface;
  int num_returned = 0;

  display = get_display ();
  if (!display)
    return NULL;

  fb_configs =
      glXChooseFBConfig (display, DefaultScreen (display),
      multisampleAttributes, &num_returned);
  if (fb_configs == NULL) {
    fb_configs =
        glXChooseFBConfig (display, DefaultScreen (display),
        singleSampleAttributes, &num_returned);
  }

  if (fb_configs == NULL) {
    GST_ERROR ("Unable to create a GL context with appropriate attributes.");
    /* FIXME: leak? */
    return NULL;
  }

  visual_info = glXGetVisualFromFBConfig (display, fb_configs[0]);
  root_window = RootWindow (display, visual_info->screen);

  window_attributes.border_pixel = 0;
  window_attributes.event_mask = StructureNotifyMask;
  window_attributes.colormap =
      XCreateColormap (display, root_window, visual_info->visual, AllocNone);

  /* Create the XWindow. */
  window = XCreateWindow (display, root_window, 0, 0, width, height,
      0, visual_info->depth, InputOutput, visual_info->visual,
      CWBorderPixel | CWColormap | CWEventMask, &window_attributes);

  /* Create a GLX context for OpenGL rendering */
  glx_context =
      glXCreateNewContext (display, fb_configs[0], GLX_RGBA_TYPE, NULL, True);

  /* Map the window to the screen, and wait for it to appear */
  XMapWindow (get_display (), window);
  XIfEvent (get_display (), &event, waitForNotify, (XPointer) window);

  device = cairo_glx_device_create (get_display (), glx_context);
  surface = cairo_gl_surface_create_for_window (device, window, width, height);

  return surface;
}
