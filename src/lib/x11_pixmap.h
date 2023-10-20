#ifndef X11_PIXMAP_H
#define X11_PIXMAP_H 1

#include <X11/Xlib.h>

#include "image.h"

typedef struct _imlibimagepixmap ImlibImagePixmap;

struct _imlibimagepixmap {
   int                 w, h;
   Pixmap              pixmap, mask;
   Display            *display;
   Visual             *visual;
   int                 depth;
   int                 source_x, source_y, source_w, source_h;
   Colormap            colormap;
   char                antialias, hi_quality, dither_mask;
   ImlibBorder         border;
   ImlibImage         *image;
   char               *file;
   char                dirty;
   int                 references;
   DATABIG             modification_count;
   ImlibImagePixmap   *next;
};

ImlibImagePixmap   *__imlib_FindCachedImagePixmap(ImlibImage * im, int w, int h,
                                                  Display * d, Visual * v,
                                                  int depth, int sx, int sy,
                                                  int sw, int sh, Colormap cm,
                                                  char aa, char hiq, char dmask,
                                                  DATABIG modification_count);
ImlibImagePixmap   *__imlib_AddImagePixmapToCache(ImlibImage * im,
                                                  Pixmap p, Pixmap m,
                                                  int w, int h,
                                                  Display * d, Visual * v,
                                                  int depth, int sx, int sy,
                                                  int sw, int sh, Colormap cm,
                                                  char aa, char hiq, char dmask,
                                                  DATABIG modification_count);
void                __imlib_CleanupImagePixmapCache(void);
int                 __imlib_PixmapCacheSize(void);

void                __imlib_FreePixmap(Display * d, Pixmap p);
void                __imlib_DirtyPixmapsForImage(ImlibImage * im);
void                __imlib_PixmapUnrefImage(ImlibImage * im);

#endif /* X11_PIXMAP_H */
