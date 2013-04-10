#include <glib.h>
#include <gst/gst.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>

#ifdef CAIROSINK_GL_DEBUG

static inline void
_gl_debug (const char *function, const char *message)
{
#ifdef GL_GREMEDY_string_marker
  gchar *full_message = g_strdup_printf ("%s: %s", function, message);

  glStringMarkerGREMEDY (0, full_message);
  GST_TRACE (message);

  g_free (full_message);
#endif
}

#define gl_debug(message) _gl_debug(__func__, message)
#else
#define gl_debug GST_TRACE
#endif

#include <cairo-gl.h>

#include "gstcairobackendglx.h"

GST_DEBUG_CATEGORY_EXTERN (gst_cairo_sink_debug_category);
#define GST_CAT_DEFAULT gst_cairo_sink_debug_category

static cairo_surface_t *gst_cairo_backend_glx_create_display_surface (gint
    width, gint height);
static cairo_surface_t *gst_cairo_backend_glx_create_surface (GstCairoBackend *
    backend, cairo_device_t * device, gint width, gint height,
    GstCairoBackendSurfaceInfo ** _surface_info);
static void gst_cairo_backend_glx_destroy_surface (cairo_surface_t * surface,
    GstCairoBackendSurfaceInfo * surface_info);

static gpointer gst_cairo_backend_glx_surface_map (cairo_surface_t * surface,
    GstCairoBackendSurfaceInfo * surface_info, GstMapFlags flags);
static void gst_cairo_backend_glx_surface_unmap (cairo_surface_t * surface,
    GstCairoBackendSurfaceInfo * surface_info);

static void gst_cairo_backend_glx_show (cairo_surface_t * surface);

typedef struct
{
  GstCairoBackendSurfaceInfo parent;
  GLuint texture;
  GLuint pbo;
  GLuint data_size;
  GLuint width;
  GLuint height;
} GstCairoBackendGLXSurfaceInfo;

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
  GstCairoBackend *backend;

  gl_debug ("GLX: Creating backend");

  backend = g_slice_new (GstCairoBackend);

  backend->create_display_surface =
      gst_cairo_backend_glx_create_display_surface;
  backend->create_surface = gst_cairo_backend_glx_create_surface;
  backend->destroy_surface = gst_cairo_backend_glx_destroy_surface;
  backend->surface_map = gst_cairo_backend_glx_surface_map;
  backend->surface_unmap = gst_cairo_backend_glx_surface_unmap;
  backend->show = gst_cairo_backend_glx_show;
  backend->need_own_thread = TRUE;
  backend->can_map = TRUE;

  gl_debug ("exit");
  return backend;
}

void
gst_cairo_backend_glx_show (cairo_surface_t * surface)
{
  gl_debug ("swapping buffers");
  cairo_gl_surface_swapbuffers (surface);
}

void
gst_cairo_backend_glx_destroy (GstCairoBackend * backend)
{
  gl_debug ("GLX: Destroying backend");
  g_slice_free (GstCairoBackend, backend);
  gl_debug ("exit");
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
gst_cairo_backend_glx_create_display_surface (int width, int height)
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

  gl_debug ("exit");
  return surface;
}

static cairo_surface_t *
gst_cairo_backend_glx_create_surface (GstCairoBackend * backend,
    cairo_device_t * device, gint width, gint height,
    GstCairoBackendSurfaceInfo ** _surface_info)
{
  GstCairoBackendGLXSurfaceInfo *glx_surface_info =
      g_slice_new (GstCairoBackendGLXSurfaceInfo);
  GstCairoBackendSurfaceInfo *surface_info;
  cairo_surface_t *surface;

  gl_debug ("GLX: create surface");

  surface_info = (GstCairoBackendSurfaceInfo *) glx_surface_info;
  surface_info->backend = backend;

  /* not sure whether we really need to cairo_device_acquire()+_release()
   * here, but cairo_gl_surface_create() seems to do it when it creates its
   * texture  */
  cairo_device_acquire (device);
  {
    /* RGBA: 4 bytes per pixel */
    glx_surface_info->data_size = width * height * 4;
    glx_surface_info->width = width;
    glx_surface_info->height = height;

    /* create texture */
    glGenTextures (1, &glx_surface_info->texture);
    glBindTexture (GL_TEXTURE_2D, glx_surface_info->texture);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA,
        GL_UNSIGNED_BYTE, NULL);

    /* create PBO */
    glGenBuffersARB (1, &glx_surface_info->pbo);
    glBindBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, glx_surface_info->pbo);
    glBufferDataARB (GL_PIXEL_UNPACK_BUFFER_ARB, glx_surface_info->data_size,
        NULL, GL_STREAM_DRAW_ARB);
    glBindBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, 0);
  }
  cairo_device_release (device);

  surface = cairo_gl_surface_create_for_texture (device, CAIRO_CONTENT_COLOR,
      glx_surface_info->texture, width, height);
  *_surface_info = surface_info;

  gl_debug ("exit");
  return surface;
}

static void
gst_cairo_backend_glx_destroy_surface (cairo_surface_t * surface,
    GstCairoBackendSurfaceInfo * surface_info)
{
  GstCairoBackendGLXSurfaceInfo *glx_surface_info =
      (GstCairoBackendGLXSurfaceInfo *) surface_info;
  cairo_device_t *device;

  gl_debug ("GLX: destroy surface");

  device = cairo_surface_get_device (surface);

  cairo_surface_destroy (surface);

  cairo_device_acquire (device);
  {
    glDeleteBuffersARB (1, &glx_surface_info->pbo);
    glDeleteTextures (1, &glx_surface_info->texture);
  }
  cairo_device_release (device);

  g_slice_free (GstCairoBackendGLXSurfaceInfo, glx_surface_info);
  gl_debug ("exit");
}

static gpointer
gst_cairo_backend_glx_surface_map (cairo_surface_t * surface,
    GstCairoBackendSurfaceInfo * surface_info, GstMapFlags flags)
{
  GstCairoBackendGLXSurfaceInfo *glx_surface_info =
      (GstCairoBackendGLXSurfaceInfo *) surface_info;
  cairo_device_t *device = cairo_surface_get_device (surface);
  gpointer data_area;

  if (flags != GST_MAP_WRITE) {
    GST_WARNING ("Mapping glx surface only implemented for writing");
    return NULL;
  }

  gl_debug ("GLX: map surface");

  cairo_device_acquire (device);
  glBindBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, glx_surface_info->pbo);

  /* We put a NULL buffer so that GL discards the current buffer if it is
   * still being used, instead of waiting for the end of an operation on it.
   * Makes sure the call to glMapBufferARB() won't cause a sync */
  glBufferDataARB (GL_PIXEL_UNPACK_BUFFER_ARB, glx_surface_info->data_size,
      NULL, GL_STREAM_DRAW_ARB);

  data_area = glMapBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

  glBindBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, 0);
  cairo_device_release (device);

  gl_debug ("exit");
  return data_area;
}

static void
gst_cairo_backend_glx_surface_unmap (cairo_surface_t * surface,
    GstCairoBackendSurfaceInfo * surface_info)
{
  GstCairoBackendGLXSurfaceInfo *glx_surface_info =
      (GstCairoBackendGLXSurfaceInfo *) surface_info;
  cairo_device_t *device = cairo_surface_get_device (surface);

  /* FIXME: how/when do we know that the transfer is done? */

  gl_debug ("GLX: unmap surface");

  cairo_device_acquire (device);
  glBindBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, glx_surface_info->pbo);
  glUnmapBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB);

  glBindTexture (GL_TEXTURE_2D, glx_surface_info->texture);
  /* copies data from pbo to texture */
  glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, glx_surface_info->width,
      glx_surface_info->height, GL_BGRA, GL_UNSIGNED_BYTE, 0);

  glBindBufferARB (GL_PIXEL_UNPACK_BUFFER_ARB, 0);
  glBindTexture (GL_TEXTURE_2D, 0);
  cairo_device_release (device);

  gl_debug ("exit");
}
