#ifndef X11_PIXMAP_H
#define X11_PIXMAP_H 1

#include "types.h"
#include "x11_types.h"

void                __imlib_CleanupImagePixmapCache(void);
int                 __imlib_PixmapCacheSize(void);

void                __imlib_FreePixmap(Display * d, Pixmap p);
void                __imlib_DirtyPixmapsForImage(const ImlibImage * im);
void                __imlib_PixmapUnrefImage(const ImlibImage * im);

int                 __imlib_CreatePixmapsForImage(const ImlibContextX11 * x11,
                                                  Drawable w, ImlibImage * im,
                                                  Pixmap * p, Mask * m,
                                                  int sx, int sy,
                                                  int sw, int sh,
                                                  int dw, int dh,
                                                  char anitalias, char hiq,
                                                  char dither_mask, int mat,
                                                  ImlibColorModifier * cmod);

#endif /* X11_PIXMAP_H */
