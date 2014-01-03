plugin_LTLIBRARIES = libgstcairobad.la

noinst_HEADERS = \
	gstcairosink.h
libgstcairobad_la_SOURCES = \
	gstcairobad.c \
	gstcairosink.c
libgstcairobad_la_CFLAGS = \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) $(CAIRO_CFLAGS)
libgstcairobad_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) -lgstvideo-$(GST_API_VERSION) \
	$(GST_BASE_LIBS) $(GST_LIBS) $(CAIRO_LIBS) $(LIBM)
libgstcairobad_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstcairobad_la_LIBTOOLFLAGS = --tag=disable-static

GL_SOURCES = gstcairobackendgl.c gstglmemory.c gstcairothreading.c

if USE_CAIRO_GL
libgstcairobad_la_SOURCES += $(GL_SOURCES)
libgstcairobad_la_CFLAGS += $(CAIRO_GL_CFLAGS)
libgstcairobad_la_LIBADD += $(CAIRO_GL_LIBS)
else
if USE_CAIRO_GLESV2
libgstcairobad_la_SOURCES += $(GL_SOURCES)
libgstcairobad_la_CFLAGS += $(CAIRO_GLESV2_CFLAGS)
libgstcairobad_la_LIBADD += $(CAIRO_GLESV2_LIBS)
endif
endif

if HAVE_CAIRO_GLX
libgstcairobad_la_SOURCES += gstcairosystemglx.c
libgstcairobad_la_CFLAGS += $(CAIRO_GLX_CFLAGS)
libgstcairobad_la_LIBADD += $(CAIRO_GLX_LIBS)
endif
if HAVE_CAIRO_EGL
libgstcairobad_la_SOURCES += gstcairosystemegl.c
libgstcairobad_la_CFLAGS += $(CAIRO_EGL_CFLAGS)
libgstcairobad_la_LIBADD += $(CAIRO_EGL_LIBS)
endif
