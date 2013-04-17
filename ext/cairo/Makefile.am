plugin_LTLIBRARIES = libgstcairobad.la

noinst_HEADERS = \
	gstcairosink.h
libgstcairobad_la_SOURCES = \
	gstcairobad.c \
	gstcairosink.c \
	gstcairobackend.c
libgstcairobad_la_CFLAGS = \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) $(CAIRO_CFLAGS)
libgstcairobad_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) -lgstvideo-$(GST_API_VERSION) \
	$(GST_BASE_LIBS) $(GST_LIBS) $(CAIRO_LIBS) $(LIBM)
libgstcairobad_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstcairobad_la_LIBTOOLFLAGS = --tag=disable-static


if USE_CAIRO_GLX
libgstcairobad_la_SOURCES += gstcairobackendglx.c
libgstcairobad_la_CFLAGS += $(CAIRO_GLX_CFLAGS)
libgstcairobad_la_LIBADD += $(CAIRO_GLX_LIBS)
endif
if USE_CAIRO_EGL
libgstcairobad_la_SOURCES += gstcairobackendegl.c
libgstcairobad_la_CFLAGS += $(CAIRO_GLESV2_CFLAGS) $(CAIRO_EGL_CFLAGS)
libgstcairobad_la_LIBADD += $(CAIRO_GLESV2_LIBS) $(CAIRO_EGL_LIBS)
endif
