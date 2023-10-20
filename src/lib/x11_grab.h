#ifndef X11_GRAB_H
#define X11_GRAB_H 1

#include "common.h"

int                 __imlib_GrabDrawableToRGBA(DATA32 * data, int x_dst,
                                               int y_dst, int w_dst, int h_dst,
                                               Display * d, Drawable p,
                                               Pixmap m, Visual * v,
                                               Colormap cm, int depth,
                                               int x_src, int y_src, int w_src,
                                               int h_src, char *domask,
                                               int grab);

int                 __imlib_GrabDrawableScaledToRGBA(DATA32 * data, int x_dst,
                                                     int y_dst, int w_dst,
                                                     int h_dst, Display * d,
                                                     Drawable p, Pixmap m,
                                                     Visual * v, Colormap cm,
                                                     int depth, int x_src,
                                                     int y_src, int w_src,
                                                     int h_src, char *pdomask,
                                                     int grab);

void                __imlib_GrabXImageToRGBA(DATA32 * data, int x_dst,
                                             int y_dst, int w_dst, int h_dst,
                                             Display * d, XImage * xim,
                                             XImage * mxim, Visual * v,
                                             int depth, int x_src, int y_src,
                                             int w_src, int h_src, int grab);

#endif /* X11_GRAB_H */
