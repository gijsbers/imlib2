#ifndef X11_DRAW_H
#define X11_DRAW_H 1

#include "colormod.h"
#include "common.h"

char                __imlib_CreatePixmapsForImage(Display * d, Drawable w,
                                                  Visual * v, int depth,
                                                  Colormap cm, ImlibImage * im,
                                                  Pixmap * p, Mask * m, int sx,
                                                  int sy, int sw, int sh,
                                                  int dw, int dh,
                                                  char anitalias, char hiq,
                                                  char dither_mask, int mat,
                                                  ImlibColorModifier * cmod);

#endif /* X11_DRAW_H */
