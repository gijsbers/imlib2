#ifndef X11_XIMAGE_H
#define X11_XIMAGE_H 1

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "x11_types.h"

void                __imlib_SetXImageCacheCountMax(const ImlibContextX11 * x11,
                                                   int num);
int                 __imlib_GetXImageCacheCountMax(const ImlibContextX11 * x11);
int                 __imlib_GetXImageCacheCountUsed(const ImlibContextX11 *
                                                    x11);
void                __imlib_SetXImageCacheSizeMax(const ImlibContextX11 * x11,
                                                  int num);
int                 __imlib_GetXImageCacheSizeMax(const ImlibContextX11 * x11);
int                 __imlib_GetXImageCacheSizeUsed(const ImlibContextX11 * x11);
void                __imlib_ConsumeXImage(const ImlibContextX11 * x11,
                                          XImage * xim);
XImage             *__imlib_ProduceXImage(const ImlibContextX11 * x11,
                                          int depth, int w, int h,
                                          char *shared);
XImage             *__imlib_ShmGetXImage(const ImlibContextX11 * x11,
                                         Drawable draw, int depth,
                                         int x, int y, int w, int h,
                                         XShmSegmentInfo * si);
void                __imlib_ShmDestroyXImage(const ImlibContextX11 * x11,
                                             XImage * xim,
                                             XShmSegmentInfo * si);

#endif /* X11_XIMAGE_H */
