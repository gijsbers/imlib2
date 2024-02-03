#ifndef X11_TYPES_H
#define X11_TYPES_H 1

#include <X11/Xlib.h>

typedef struct {
    Display        *dpy;
    Visual         *vis;
    Colormap        cmap;
    int             depth;
} ImlibContextX11;

#endif                          /* X11_TYPES_H */
