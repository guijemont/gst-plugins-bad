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
#include "gstglmemory.h"

GST_DEBUG_CATEGORY_EXTERN (gst_cairo_sink_debug_category);
#define GST_CAT_DEFAULT gst_cairo_sink_debug_category

static cairo_surface_t *_gl_create_surface (GstCairoBackend * backend,
    cairo_device_t * device, GstMemory * mem);

static void _gl_destroy_surface (cairo_surface_t * surface);

static void _gl_show (cairo_surface_t * surface);

GstCairoBackend gst_cairo_backend_gl = {
  _gl_create_surface,
  _gl_destroy_surface,
  _gl_show,
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

static void
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
_gl_create_surface (GstCairoBackend * backend, cairo_device_t * device,
    GstMemory * mem)
{
  cairo_surface_t *surface;
  GstGLMemory *glmem = (GstGLMemory *) mem;

  gl_debug ("GL: create surface");

  /* not sure whether we really need to cairo_device_acquire()+_release()
   * here, but cairo_gl_surface_create() seems to do it when it creates its
   * texture  */

  surface = cairo_gl_surface_create_for_texture (device, CAIRO_CONTENT_COLOR,
      glmem->texture, glmem->width, glmem->height);
  gl_debug ("exit");
  return surface;
}

static void
_gl_destroy_surface (cairo_surface_t * surface)
{
  gl_debug ("GL: destroy surface");
  cairo_surface_destroy (surface);
  gl_debug ("exit");
}
