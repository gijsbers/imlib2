AM_CFLAGS            = $(CFLAGS_WARNINGS) $(CFLAGS_VISIBILITY) $(CFLAGS_ASAN)
AM_CPPFLAGS          = -I$(top_builddir) -I$(top_srcdir)/src/lib \
		       -D PACKAGE_DATA_DIR=\"$(pkgdatadir)\"

pkgdir               = $(libdir)/imlib2/loaders

pkg_LTLIBRARIES = \
ani.la \
argb.la \
bmp.la \
ff.la \
ico.la \
lbm.la \
pnm.la \
qoi.la \
tga.la \
xbm.la \
xpm.la

if BUILD_AVIF_LOADER
pkg_LTLIBRARIES += avif.la
endif
if BUILD_GIF_LOADER
pkg_LTLIBRARIES += gif.la
endif
if BUILD_HEIF_LOADER
pkg_LTLIBRARIES += heif.la
endif
if BUILD_JPEG_LOADER
pkg_LTLIBRARIES += jpeg.la
endif
if BUILD_J2K_LOADER
pkg_LTLIBRARIES += j2k.la
endif
if BUILD_JXL_LOADER
pkg_LTLIBRARIES += jxl.la
endif
if BUILD_PNG_LOADER
pkg_LTLIBRARIES += png.la
endif
if BUILD_PS_LOADER
pkg_LTLIBRARIES += ps.la
endif
if BUILD_RAW_LOADER
pkg_LTLIBRARIES += raw.la
endif
if BUILD_SVG_LOADER
pkg_LTLIBRARIES += svg.la
endif
if BUILD_TIFF_LOADER
pkg_LTLIBRARIES += tiff.la
endif
if BUILD_WEBP_LOADER
pkg_LTLIBRARIES += webp.la
endif
if BUILD_Y4M_LOADER
pkg_LTLIBRARIES += y4m.la
endif

if BUILD_BZ2_LOADER
pkg_LTLIBRARIES += bz2.la
endif
if BUILD_LZMA_LOADER
pkg_LTLIBRARIES += lzma.la
endif
if BUILD_ZLIB_LOADER
pkg_LTLIBRARIES += zlib.la
endif

if BUILD_ID3_LOADER
pkg_LTLIBRARIES += id3.la
endif

 SRCS_EXIF = exif.c exif.h
 SRCS_SAVE = ldrs_util.c ldrs_util.h

if ENABLE_RTLD_LOCAL_SUPPORT
IMLIB2_LIBS = $(top_builddir)/src/lib/libImlib2.la
endif

LIBTOOLFLAGS = --tag=disable-static

ani_la_SOURCES       = loader_ani.c
ani_la_LDFLAGS       = -module -avoid-version
ani_la_LIBADD        = $(IMLIB2_LIBS)

argb_la_SOURCES      = loader_argb.c
argb_la_LDFLAGS      = -module -avoid-version
argb_la_LIBADD       = $(IMLIB2_LIBS)

avif_la_SOURCES      = loader_avif.c $(SRCS_SAVE)
avif_la_CPPFLAGS     = $(AM_CPPFLAGS) $(AVIF_CFLAGS)
avif_la_LDFLAGS      = -module -avoid-version
avif_la_LIBADD       = $(IMLIB2_LIBS) $(AVIF_LIBS)

bmp_la_SOURCES       = loader_bmp.c
bmp_la_LDFLAGS       = -module -avoid-version
bmp_la_LIBADD        = $(IMLIB2_LIBS)

ff_la_SOURCES        = loader_ff.c
ff_la_LDFLAGS        = -module -avoid-version
ff_la_LIBADD         = $(IMLIB2_LIBS)

gif_la_SOURCES       = loader_gif.c
gif_la_LDFLAGS       = -module -avoid-version
gif_la_LIBADD        = $(IMLIB2_LIBS) $(GIF_LIBS)

heif_la_SOURCES      = loader_heif.c $(SRCS_SAVE)
heif_la_CPPFLAGS     = $(AM_CPPFLAGS) $(HEIF_CFLAGS)
heif_la_LDFLAGS      = -module -avoid-version
heif_la_LIBADD       = $(IMLIB2_LIBS) $(HEIF_LIBS)

ico_la_SOURCES       = loader_ico.c
ico_la_LDFLAGS       = -module -avoid-version
ico_la_LIBADD        = $(IMLIB2_LIBS)

jpeg_la_SOURCES      = loader_jpeg.c $(SRCS_EXIF) $(SRCS_SAVE)
jpeg_la_CPPFLAGS     = $(AM_CPPFLAGS) $(JPEG_CFLAGS)
jpeg_la_LDFLAGS      = -module -avoid-version
jpeg_la_LIBADD       = $(IMLIB2_LIBS) $(JPEG_LIBS)

j2k_la_SOURCES       = loader_j2k.c
j2k_la_CPPFLAGS      = $(AM_CPPFLAGS) $(J2K_CFLAGS)
j2k_la_LDFLAGS       = -module -avoid-version
j2k_la_LIBADD        = $(IMLIB2_LIBS) $(J2K_LIBS)

jxl_la_SOURCES       = loader_jxl.c $(SRCS_SAVE)
jxl_la_CPPFLAGS      = $(AM_CPPFLAGS) $(JXL_CFLAGS)
jxl_la_LDFLAGS       = -module -avoid-version
jxl_la_LIBADD        = $(IMLIB2_LIBS) $(JXL_LIBS)

lbm_la_SOURCES       = loader_lbm.c
lbm_la_LDFLAGS       = -module -avoid-version
lbm_la_LIBADD        = $(IMLIB2_LIBS)

png_la_SOURCES       = loader_png.c $(SRCS_SAVE)
png_la_CPPFLAGS      = $(AM_CPPFLAGS) $(PNG_CFLAGS)
png_la_LDFLAGS       = -module -avoid-version
png_la_LIBADD        = $(IMLIB2_LIBS) $(PNG_LIBS)

pnm_la_SOURCES       = loader_pnm.c
pnm_la_LDFLAGS       = -module -avoid-version
pnm_la_LIBADD        = $(IMLIB2_LIBS)

ps_la_SOURCES        = loader_ps.c
ps_la_CPPFLAGS       = $(AM_CPPFLAGS) $(PS_CFLAGS)
ps_la_LDFLAGS        = -module -avoid-version
ps_la_LIBADD         = $(IMLIB2_LIBS) $(PS_LIBS)

raw_la_SOURCES       = loader_raw.c
raw_la_CPPFLAGS      = $(AM_CPPFLAGS) $(RAW_CFLAGS)
raw_la_LDFLAGS       = -module -avoid-version
raw_la_LIBADD        = $(IMLIB2_LIBS) $(RAW_LIBS)

qoi_la_SOURCES       = loader_qoi.c
qoi_la_LDFLAGS       = -module -avoid-version
qoi_la_LIBADD        = $(IMLIB2_LIBS)

svg_la_SOURCES       = loader_svg.c
svg_la_CPPFLAGS      = $(AM_CPPFLAGS) $(SVG_CFLAGS)
svg_la_LDFLAGS       = -module -avoid-version
svg_la_LIBADD        = $(IMLIB2_LIBS) $(SVG_LIBS)

tga_la_SOURCES       = loader_tga.c
tga_la_LDFLAGS       = -module -avoid-version
tga_la_LIBADD        = $(IMLIB2_LIBS)

tiff_la_SOURCES      = loader_tiff.c $(SRCS_SAVE)
tiff_la_CPPFLAGS     = $(AM_CPPFLAGS) $(TIFF_CFLAGS)
tiff_la_LDFLAGS      = -module -avoid-version
tiff_la_LIBADD       = $(IMLIB2_LIBS) $(TIFF_LIBS)

webp_la_SOURCES      = loader_webp.c $(SRCS_SAVE)
webp_la_CPPFLAGS     = $(AM_CPPFLAGS) $(WEBP_CFLAGS)
webp_la_LDFLAGS      = -module -avoid-version
webp_la_LIBADD       = $(IMLIB2_LIBS) $(WEBP_LIBS)

xbm_la_SOURCES       = loader_xbm.c
xbm_la_LDFLAGS       = -module -avoid-version
xbm_la_LIBADD        = $(IMLIB2_LIBS)

xpm_la_SOURCES       = loader_xpm.c
xpm_la_LDFLAGS       = -module -avoid-version
xpm_la_LIBADD        = $(IMLIB2_LIBS)

y4m_la_SOURCES       = loader_y4m.c
y4m_la_CPPFLAGS      = $(AM_CPPFLAGS) $(Y4M_CFLAGS)
y4m_la_LDFLAGS       = -module -avoid-version
y4m_la_LIBADD        = $(IMLIB2_LIBS) $(Y4M_LIBS)

bz2_la_SOURCES       = loader_bz2.c decompress_load.c compression.h
bz2_la_CPPFLAGS      = $(AM_CPPFLAGS) $(BZ2_CFLAGS)
bz2_la_LDFLAGS       = -module -avoid-version
bz2_la_LIBADD        = $(IMLIB2_LIBS) $(BZ2_LIBS)

lzma_la_SOURCES      = loader_lzma.c decompress_load.c compression.h
lzma_la_CPPFLAGS     = $(AM_CPPFLAGS) $(LZMA_CFLAGS)
lzma_la_LDFLAGS      = -module -avoid-version
lzma_la_LIBADD       = $(IMLIB2_LIBS) $(LZMA_LIBS)

zlib_la_SOURCES      = loader_zlib.c decompress_load.c compression.h
zlib_la_CPPFLAGS     = $(AM_CPPFLAGS) $(ZLIB_CFLAGS)
zlib_la_LDFLAGS      = -module -avoid-version
zlib_la_LIBADD       = $(IMLIB2_LIBS) $(ZLIB_LIBS)

id3_la_SOURCES       = loader_id3.c
id3_la_CPPFLAGS      = $(AM_CPPFLAGS) $(ID3_CFLAGS)
id3_la_LDFLAGS       = -module -avoid-version
id3_la_LIBADD        = $(IMLIB2_LIBS) $(ID3_LIBS)
