#ifndef X11_REND_H
#define X11_REND_H 1

#include "types.h"
#include "x11_types.h"

uint32_t        __imlib_RenderGetPixel(const ImlibContextX11 * x11,
                                       Drawable w, uint8_t r, uint8_t g,
                                       uint8_t b);

void            __imlib_RenderDisconnect(Display * d);

void            __imlib_RenderImage(const ImlibContextX11 * ctx,
                                    ImlibImage * im, Drawable w, Drawable m,
                                    int sx, int sy, int sw, int sh, int dx,
                                    int dy, int dw, int dh, char antialias,
                                    char hiq, char blend, char dither_mask,
                                    int mat, ImlibColorModifier * cmod,
                                    ImlibOp op);

void            __imlib_RenderImageSkewed(const ImlibContextX11 * x11,
                                          ImlibImage * im,
                                          Drawable w, Drawable m,
                                          int sx, int sy, int sw, int sh,
                                          int dx, int dy,
                                          int hsx, int hsy,
                                          int vsx, int vsy,
                                          char antialias, char hiq,
                                          char blend, char dither_mask,
                                          int mat,
                                          ImlibColorModifier * cmod,
                                          ImlibOp op);

#endif                          /* X11_REND_H */
