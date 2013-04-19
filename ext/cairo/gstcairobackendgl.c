#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* FIXME: work with gles */

#ifdef USE_CAIRO_GL
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#define CAIROSINK_USE_PBO
#elif USE_CAIRO_GLESV2
#include <GLES2/gl2.h>
#endif

#include <gst/gst.h>

#include <cairo-gl.h>

#include "gstcairogldebug.h"
#include "gstcairobackend.h"

GST_DEBUG_CATEGORY_EXTERN (gst_cairo_sink_debug_category);
#define GST_CAT_DEFAULT gst_cairo_sink_debug_category

static cairo_surface_t *_gl_create_surface (GstCairoBackend * backend,
    cairo_device_t * device, gint width, gint height,
    GstCairoBackendSurfaceInfo ** _surface_info);
static void _gl_destroy_surface (cairo_surface_t * surface,
    GstCairoBackendSurfaceInfo * surface_info);
static void _gl_show (cairo_surface_t * surface);
static void _gl_get_size (cairo_surface_t * surface, gint * width,
    gint * height);

static gpointer _gl_surface_map (cairo_surface_t * surface,
    GstCairoBackendSurfaceInfo * surface_info, GstMapFlags flags);
static void _gl_surface_unmap (cairo_surface_t * surface,
    GstCairoBackendSurfaceInfo * surface_info);

GstCairoBackend gst_cairo_backend_gl = {
  _gl_create_surface,
  _gl_destroy_surface,
  _gl_get_size,
  _gl_show,
  _gl_surface_map,
  _gl_surface_unmap,
  GST_CAIRO_BACKEND_GL
};

typedef struct
{
  GstCairoBackendSurfaceInfo parent;
  GLuint texture;
#ifdef CAIROSINK_USE_PBO
  GLuint pbo;
#endif
  GLuint data_size;
  GLuint width;
  GLuint height;
} GstCairoBackendGLSurfaceInfo;

void
_gl_show (cairo_surface_t * surface)
{
  gl_debug ("swapping buffers");

  /* XXX: forcing pending drawing.
   * Calling cairo_surface_flush () on cairo_gl_surface does 
   * not resolve multisampling on the current surface, application
   * must call glBlitFrameBuffer if it is supported and the surface
   * is texture based, or it must call glDisable (GL_MULTISAMPLE) if
   * application would like to use this surface with direct GL calls.
   * if application only uses cairo API, then cairo takes care of
   * resolving multisampling. */
  cairo_surface_flush (surface);

  cairo_gl_surface_swapbuffers (surface);
}

static cairo_surface_t *
_gl_create_surface (GstCairoBackend * backend,
    cairo_device_t * device, gint width, gint height,
    GstCairoBackendSurfaceInfo ** _surface_info)
{
  GstCairoBackendGLSurfaceInfo *gl_surface_info =
      g_slice_new (GstCairoBackendGLSurfaceInfo);
  GstCairoBackendSurfaceInfo *surface_info;
  cairo_surface_t *surface;

  gl_debug ("GL: create surface");

  surface_info = (GstCairoBackendSurfaceInfo *) gl_surface_info;
  surface_info->backend = backend;

  /* not sure whether we really need to cairo_device_acquire()+_release()
   * here, but cairo_gl_surface_create() seems to do it when it creates its
   * texture  */
  cairo_device_acquire (device);
  {
    /* RGBA: 4 bytes per pixel */
    gl_surface_info->data_size = width * height * 4;
    gl_surface_info->width = width;
    gl_surface_info->height = height;

    /* create texture */
    glGenTextures (1, &gl_surface_info->texture);
    glBindTexture (GL_TEXTURE_2D, gl_surface_info->texture);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA,
        GL_UNSIGNED_BYTE, NULL);

#ifdef CAIROSINK_USE_PBO
    /* create PBO */
    glGenBuffers (1, &gl_surface_info->pbo);
    glBindBuffer (GL_PIXEL_UNPACK_BUFFER, gl_surface_info->pbo);
    glBufferData (GL_PIXEL_UNPACK_BUFFER, gl_surface_info->data_size,
        NULL, GL_STREAM_DRAW);
    glBindBuffer (GL_PIXEL_UNPACK_BUFFER, 0);
#endif
  }
  cairo_device_release (device);

  surface = cairo_gl_surface_create_for_texture (device, CAIRO_CONTENT_COLOR,
      gl_surface_info->texture, width, height);
  *_surface_info = surface_info;

  gl_debug ("exit");
  return surface;
}

static void
_gl_destroy_surface (cairo_surface_t * surface,
    GstCairoBackendSurfaceInfo * surface_info)
{
  GstCairoBackendGLSurfaceInfo *gl_surface_info =
      (GstCairoBackendGLSurfaceInfo *) surface_info;
  cairo_device_t *device;

  gl_debug ("GL: destroy surface");

  device = cairo_surface_get_device (surface);

  cairo_surface_destroy (surface);

  cairo_device_acquire (device);
  {
#ifdef CAIROSINK_USE_PBO
    glDeleteBuffers (1, &gl_surface_info->pbo);
#endif
    glDeleteTextures (1, &gl_surface_info->texture);
  }
  cairo_device_release (device);

  g_slice_free (GstCairoBackendGLSurfaceInfo, gl_surface_info);
  gl_debug ("exit");
}

static gpointer
_gl_surface_map (cairo_surface_t * surface,
    GstCairoBackendSurfaceInfo * surface_info, GstMapFlags flags)
{
#ifdef CAIROSINK_USE_PBO
  GstCairoBackendGLSurfaceInfo *gl_surface_info =
      (GstCairoBackendGLSurfaceInfo *) surface_info;
  cairo_device_t *device = cairo_surface_get_device (surface);
  gpointer data_area;

  if (flags != GST_MAP_WRITE) {
    GST_WARNING ("Mapping gl surface only implemented for writing");
    return NULL;
  }

  gl_debug ("GL: map surface");

  cairo_device_acquire (device);
  glBindBuffer (GL_PIXEL_UNPACK_BUFFER, gl_surface_info->pbo);

  /* We put a NULL buffer so that GL discards the current buffer if it is
   * still being used, instead of waiting for the end of an operation on it.
   * Makes sure the call to glMapBuffer() won't cause a sync */
  glBufferData (GL_PIXEL_UNPACK_BUFFER, gl_surface_info->data_size,
      NULL, GL_STREAM_DRAW);

  data_area = glMapBuffer (GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);

  glBindBuffer (GL_PIXEL_UNPACK_BUFFER, 0);
  cairo_device_release (device);

  gl_debug ("exit");
  return data_area;
#else
  /* we could implement other methods here, such as EGL_KHR_lock_surface2 */
  g_assert_not_reached ();
  return NULL;
#endif
}

static void
_gl_surface_unmap (cairo_surface_t * surface,
    GstCairoBackendSurfaceInfo * surface_info)
{
#ifdef CAIROSINK_USE_PBO
  GstCairoBackendGLSurfaceInfo *gl_surface_info =
      (GstCairoBackendGLSurfaceInfo *) surface_info;
  cairo_device_t *device = cairo_surface_get_device (surface);

  /* FIXME: how/when do we know that the transfer is done? */

  gl_debug ("GL: unmap surface");

  cairo_device_acquire (device);
  glBindBuffer (GL_PIXEL_UNPACK_BUFFER, gl_surface_info->pbo);
  glUnmapBuffer (GL_PIXEL_UNPACK_BUFFER);

  glBindTexture (GL_TEXTURE_2D, gl_surface_info->texture);
  /* copies data from pbo to texture */
  glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, gl_surface_info->width,
      gl_surface_info->height, GL_BGRA, GL_UNSIGNED_BYTE, 0);

  glBindBuffer (GL_PIXEL_UNPACK_BUFFER, 0);
  glBindTexture (GL_TEXTURE_2D, 0);
  cairo_device_release (device);

  gl_debug ("exit");
#else
  g_assert_not_reached ();
#endif
}

static void
_gl_get_size (cairo_surface_t * surface, gint * width, gint * height)
{
  *width = cairo_gl_surface_get_width (surface);
  *height = cairo_gl_surface_get_height (surface);
}
