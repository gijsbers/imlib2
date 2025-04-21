AM_CFLAGS   = $(CFLAGS_WARNINGS) $(CFLAGS_ASAN)
AM_CPPFLAGS = \
-D PACKAGE_DATA_DIR=\"$(pkgdatadir)\" \
-I$(top_builddir)/src/lib \
$(X_CFLAGS)

if BUILD_X11
X_BASED_PROGS = imlib2_show imlib2_test imlib2_poly imlib2_colorspace imlib2_view imlib2_grab
if ENABLE_FILTERS
X_BASED_PROGS += imlib2_bumpmap
endif
endif

IMLIB2_LIBS = $(top_builddir)/src/lib/libImlib2.la

bin_PROGRAMS = \
imlib2_conv \
imlib2_load \
$(X_BASED_PROGS)

SRCS_X11  = prog_x11.c  prog_x11.h
SRCS_UTIL = prog_util.c prog_util.h

imlib2_conv_SOURCES = imlib2_conv.c $(SRCS_UTIL)
imlib2_conv_LDADD   = $(IMLIB2_LIBS)

imlib2_load_SOURCES  = imlib2_load.c $(SRCS_UTIL)
imlib2_load_CPPFLAGS = $(AM_CPPFLAGS) $(ZLIB_CFLAGS)
imlib2_load_LDADD    = $(IMLIB2_LIBS) $(ZLIB_LIBS) $(CLOCK_LIBS)

imlib2_show_SOURCES = imlib2_show.c $(SRCS_X11)
imlib2_show_LDADD   = $(IMLIB2_LIBS) -lX11 -lm

imlib2_test_SOURCES = imlib2_test.c $(SRCS_X11)
imlib2_test_LDADD   = $(IMLIB2_LIBS) -lX11

imlib2_bumpmap_SOURCES = imlib2_bumpmap.c $(SRCS_X11)
imlib2_bumpmap_LDADD   = $(IMLIB2_LIBS) -lX11

imlib2_poly_SOURCES = imlib2_poly.c $(SRCS_X11)
imlib2_poly_LDADD   = $(IMLIB2_LIBS) -lX11

imlib2_colorspace_SOURCES = imlib2_colorspace.c $(SRCS_X11)
imlib2_colorspace_LDADD   = $(IMLIB2_LIBS) -lX11

imlib2_view_SOURCES  = imlib2_view.c $(SRCS_X11)
imlib2_view_CPPFLAGS = $(AM_CPPFLAGS) $(ZLIB_CFLAGS)
imlib2_view_LDADD    = $(IMLIB2_LIBS) $(ZLIB_LIBS) -lX11

imlib2_grab_SOURCES = imlib2_grab.c $(SRCS_X11)
imlib2_grab_LDADD   = $(IMLIB2_LIBS) -lX11
