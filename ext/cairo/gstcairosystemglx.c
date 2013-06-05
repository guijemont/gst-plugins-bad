#include <string.h>
#include <stdlib.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/glx.h>
#include <cairo-gl.h>
#include "gstcairosystem.h"
#include "gstcairogldebug.h"

static cairo_surface_t *_glx_create_display_surface (gint width, gint height);

static gboolean _glx_query_can_map (cairo_surface_t * surface);

typedef struct
{
  GstCairoSystem parent;
  Display *display;
} GstCairoSystemGLX;

GstCairoSystemGLX gst_cairo_system_glx = {
  {_glx_create_display_surface,
        _glx_query_can_map,
      GST_CAIRO_SYSTEM_GLX},
  NULL
};

#define GL_VERSION_ENCODE(major, minor) ( \
  ((major) * 256) + ((minor) * 1))

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

/* copy from cairo */
static int
get_gl_version (void)
{
  int major, minor;
  const char *version = (const char *) glGetString (GL_VERSION);
  const char *dot = version == NULL ? NULL : strchr (version, '.');
  const char *major_start = dot;

  if (dot == NULL || dot == version || *(dot + 1) == '\0') {
    major = 0;
    minor = 0;
  } else {
    while (major_start > version && *major_start != ' ')
      --major_start;
    major = strtol (major_start, NULL, 10);
    minor = strtol (dot + 1, NULL, 10);
  }

  return GL_VERSION_ENCODE (major, minor);
}

static Display *
create_display (void)
{
  Display *display;

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
_glx_create_display_surface (gint width, gint height)
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

  gl_debug ("GLX: creating display window and surface");

  if (!gst_cairo_system_glx.display)
    gst_cairo_system_glx.display = create_display ();

  display = gst_cairo_system_glx.display;

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
  XMapWindow (display, window);
  XIfEvent (display, &event, waitForNotify, (XPointer) window);

  device = cairo_glx_device_create (display, glx_context);
  surface = cairo_gl_surface_create_for_window (device, window, width, height);

  gl_debug ("exit");
  return surface;
}

static gboolean
_glx_query_can_map (cairo_surface_t * surface)
{
  Display *display;
  GLXContext glx_context;
  int gl_version;

  Window window, root_window;
  XVisualInfo *visual_info;
  XSetWindowAttributes window_attributes;
  GLXFBConfig *fb_configs;

  int num_returned = 0;

  if (surface) {
    cairo_device_t *device = cairo_surface_get_device (surface);
    display = cairo_glx_device_get_display (device);
    glx_context = cairo_glx_device_get_context (device);

    if (!display || !glx_context)
      return FALSE;

    cairo_device_acquire (device);
    gl_version = get_gl_version ();
    cairo_device_release (device);
    if (gl_version >= GL_VERSION_ENCODE (1, 5))
      return TRUE;

    return FALSE;
  }

  if (!gst_cairo_system_glx.display)
    gst_cairo_system_glx.display = create_display ();

  display = gst_cairo_system_glx.display;
  if (!display)
    return FALSE;

  fb_configs =
      glXChooseFBConfig (display, DefaultScreen (display),
      singleSampleAttributes, &num_returned);

  if (fb_configs == NULL) {
    GST_ERROR ("Unable to create a GL context with appropriate attributes.");
    /* FIXME: leak? */
    return FALSE;
  }

  visual_info = glXGetVisualFromFBConfig (display, fb_configs[0]);
  root_window = RootWindow (display, visual_info->screen);

  window_attributes.border_pixel = 0;
  window_attributes.event_mask = StructureNotifyMask;
  window_attributes.colormap =
      XCreateColormap (display, root_window, visual_info->visual, AllocNone);

  /* Create the XWindow. */
  window = XCreateWindow (display, root_window, 0, 0, 1, 1,
      0, visual_info->depth, InputOutput, visual_info->visual,
      CWBorderPixel | CWColormap | CWEventMask, &window_attributes);

  glx_context =
      glXCreateNewContext (display, fb_configs[0], GLX_RGBA_TYPE, NULL, True);
  XFree (visual_info);

  if (!glx_context) {
    GST_ERROR ("Unable to create a GL context.");
    /* FIXME: leak? */
    return FALSE;
  }

  /* switch to current context */
  glXMakeCurrent (display, window, glx_context);
  gl_version = get_gl_version ();

  /* cleanup */
  glXMakeCurrent (display, None, None);
  XDestroyWindow (display, window);
  glXDestroyContext (display, glx_context);
  XSync (display, True);

  if (gl_version >= GL_VERSION_ENCODE (1, 5))
    return TRUE;

  return FALSE;
}
