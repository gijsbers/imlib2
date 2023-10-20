AM_CFLAGS            = $(CFLAGS_WARNINGS) $(CFLAGS_VISIBILITY) $(CFLAGS_ASAN)
AM_CPPFLAGS          = -I$(top_builddir) -I$(top_srcdir)/src/lib

pkgdir               = $(libdir)/imlib2/loaders

if BUILD_BZ2_LOADER
BZ2_L                = bz2.la
endif
if BUILD_GIF_LOADER
GIF_L                = gif.la
endif
if BUILD_ID3_LOADER
ID3_L                = id3.la
endif
if BUILD_JPEG_LOADER
JPEG_L               = jpeg.la
endif
if BUILD_PNG_LOADER
PNG_L                = png.la
endif
if BUILD_TIFF_LOADER
TIFF_L               = tiff.la
endif
if BUILD_WEBP_LOADER
WEBP_L		     = webp.la
endif
if BUILD_ZLIB_LOADER
ZLIB_L               = zlib.la
endif

pkg_LTLIBRARIES      = \
argb.la \
bmp.la \
$(BZ2_L) \
ff.la \
$(GIF_L) \
ico.la \
$(ID3_L) \
$(JPEG_L) \
lbm.la \
$(PNG_L) \
pnm.la \
tga.la \
$(TIFF_L) \
$(WEBP_L) \
xbm.la \
xpm.la \
$(ZLIB_L)

EXTRA_DIST = loader_common.h

argb_la_SOURCES      = loader_argb.c
argb_la_LDFLAGS      = -module -avoid-version
argb_la_LIBADD       = $(top_builddir)/src/lib/libImlib2.la
argb_la_LIBTOOLFLAGS = --tag=disable-static

bmp_la_SOURCES       = loader_bmp.c
bmp_la_LDFLAGS       = -module -avoid-version
bmp_la_LIBADD        = $(top_builddir)/src/lib/libImlib2.la
bmp_la_LIBTOOLFLAGS = --tag=disable-static

bz2_la_SOURCES       = loader_bz2.c
bz2_la_CPPFLAGS      = $(BZ2_CFLAGS) $(AM_CPPFLAGS)
bz2_la_LDFLAGS       = -module -avoid-version
bz2_la_LIBADD        = $(BZ2_LIBS) $(top_builddir)/src/lib/libImlib2.la
bz2_la_LIBTOOLFLAGS = --tag=disable-static

ff_la_SOURCES        = loader_ff.c
ff_la_LDFLAGS        = -module -avoid-version
ff_la_LIBADD         = $(top_builddir)/src/lib/libImlib2.la
ff_la_LIBTOOLFLAGS   = --tag=disable-static

gif_la_SOURCES       = loader_gif.c
gif_la_LDFLAGS       = -module -avoid-version
gif_la_LIBADD        = $(GIF_LIBS) $(top_builddir)/src/lib/libImlib2.la
gif_la_LIBTOOLFLAGS = --tag=disable-static

ico_la_SOURCES       = loader_ico.c
ico_la_LDFLAGS       = -module -avoid-version
ico_la_LIBADD        = $(top_builddir)/src/lib/libImlib2.la
ico_la_LIBTOOLFLAGS = --tag=disable-static

id3_la_SOURCES       = loader_id3.c
id3_la_CPPFLAGS      = $(ID3_CFLAGS) $(AM_CPPFLAGS)
id3_la_LDFLAGS       = -module -avoid-version
id3_la_LIBADD        = $(ID3_LIBS) $(top_builddir)/src/lib/libImlib2.la
id3_la_LIBTOOLFLAGS = --tag=disable-static

jpeg_la_SOURCES      = loader_jpeg.c exif.c exif.h
jpeg_la_CPPFLAGS     = $(JPEG_CFLAGS) $(AM_CPPFLAGS)
jpeg_la_LDFLAGS      = -module -avoid-version
jpeg_la_LIBADD       = $(JPEG_LIBS) $(top_builddir)/src/lib/libImlib2.la
jpeg_la_LIBTOOLFLAGS = --tag=disable-static

lbm_la_SOURCES       = loader_lbm.c
lbm_la_LDFLAGS       = -module -avoid-version
lbm_la_LIBADD        = $(top_builddir)/src/lib/libImlib2.la
lbm_la_LIBTOOLFLAGS = --tag=disable-static

png_la_SOURCES       = loader_png.c
png_la_CPPFLAGS      = $(PNG_CFLAGS) $(AM_CPPFLAGS)
png_la_LDFLAGS       = -module -avoid-version
png_la_LIBADD        = $(PNG_LIBS) $(top_builddir)/src/lib/libImlib2.la
png_la_LIBTOOLFLAGS = --tag=disable-static

pnm_la_SOURCES       = loader_pnm.c
pnm_la_LDFLAGS       = -module -avoid-version
pnm_la_LIBADD        = $(top_builddir)/src/lib/libImlib2.la
pnm_la_LIBTOOLFLAGS = --tag=disable-static

tga_la_SOURCES       = loader_tga.c
tga_la_LDFLAGS       = -module -avoid-version
tga_la_LIBADD        = $(top_builddir)/src/lib/libImlib2.la
tga_la_LIBTOOLFLAGS = --tag=disable-static

tiff_la_SOURCES      = loader_tiff.c
tiff_la_CPPFLAGS     = $(TIFF_CFLAGS) $(AM_CPPFLAGS)
tiff_la_LDFLAGS      = -module -avoid-version
tiff_la_LIBADD       = $(TIFF_LIBS) $(top_builddir)/src/lib/libImlib2.la
tiff_la_LIBTOOLFLAGS = --tag=disable-static

webp_la_SOURCES       = loader_webp.c
webp_la_CPPFLAGS      = $(WEBP_CFLAGS) $(AM_CPPFLAGS)
webp_la_LDFLAGS       = -module -avoid-version
webp_la_LIBADD        = $(WEBP_LIBS) $(top_builddir)/src/lib/libImlib2.la
webp_la_LIBTOOLFLAGS = --tag=disable-static

xbm_la_SOURCES       = loader_xbm.c
xbm_la_LDFLAGS       = -module -avoid-version
xbm_la_LIBADD        = $(top_builddir)/src/lib/libImlib2.la
xbm_la_LIBTOOLFLAGS = --tag=disable-static

xpm_la_SOURCES       = loader_xpm.c
xpm_la_LDFLAGS       = -module -avoid-version
xpm_la_LIBADD        = $(top_builddir)/src/lib/libImlib2.la
xpm_la_LIBTOOLFLAGS = --tag=disable-static

zlib_la_SOURCES      = loader_zlib.c
zlib_la_CPPFLAGS     = $(ZLIB_CFLAGS) $(AM_CPPFLAGS)
zlib_la_LDFLAGS      = -module -avoid-version
zlib_la_LIBADD       = $(ZLIB_LIBS) $(top_builddir)/src/lib/libImlib2.la
zlib_la_LIBTOOLFLAGS = --tag=disable-static
