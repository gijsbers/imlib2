#ifndef X11_GRAB_H
#define X11_GRAB_H 1

#include "types.h"
#include "x11_types.h"

int             __imlib_GrabDrawableToRGBA(const ImlibContextX11 * x11,
                                           uint32_t * data,
                                           int x_dst, int y_dst,
                                           int w_dst, int h_dst,
                                           Drawable p, Pixmap m,
                                           int x_src, int y_src,
                                           int w_src, int h_src,
                                           char *domask, int grab,
                                           bool clear,
                                           const XWindowAttributes * attr);

int             __imlib_GrabDrawableScaledToRGBA(const ImlibContextX11 *
                                                 x11, uint32_t * data,
                                                 int x_dst, int y_dst,
                                                 int w_dst, int h_dst,
                                                 Drawable p, Pixmap m,
                                                 int x_src, int y_src,
                                                 int w_src, int h_src,
                                                 char *domask, int grab,
                                                 bool clear);

void            __imlib_GrabXImageToRGBA(const ImlibContextX11 * x11,
                                         int depth, uint32_t * data,
                                         int x_dst, int y_dst,
                                         int w_dst, int h_dst,
                                         XImage * xim, XImage * mxim,
                                         int x_src, int y_src,
                                         int w_src, int h_src, int grab);

#endif                          /* X11_GRAB_H */
