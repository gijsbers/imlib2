#include "common.h"

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "blend.h"
#include "colormod.h"
#include "image.h"
#include "x11_draw.h"
#include "x11_pixmap.h"
#include "x11_rend.h"

char
__imlib_CreatePixmapsForImage(Display * d, Drawable w, Visual * v, int depth,
                              Colormap cm, ImlibImage * im, Pixmap * p,
                              Mask * m, int sx, int sy, int sw, int sh, int dw,
                              int dh, char antialias, char hiq,
                              char dither_mask, int mat,
                              ImlibColorModifier * cmod)
{
   ImlibImagePixmap   *ip = NULL;
   Pixmap              pmap = 0;
   Pixmap              mask = 0;
   long long           mod_count = 0;

   if (cmod)
      mod_count = cmod->modification_count;
   ip = __imlib_FindCachedImagePixmap(im, dw, dh, d, v, depth, sx, sy,
                                      sw, sh, cm, antialias, hiq, dither_mask,
                                      mod_count);
   if (ip)
     {
        if (p)
           *p = ip->pixmap;
        if (m)
           *m = ip->mask;
        ip->references++;
#ifdef DEBUG_CACHE
        fprintf(stderr,
                "[Imlib2]  Match found in cache.  Reference count is %d, pixmap 0x%08lx, mask 0x%08lx\n",
                ip->references, ip->pixmap, ip->mask);
#endif
        return 2;
     }
   if (p)
     {
        pmap = XCreatePixmap(d, w, dw, dh, depth);
        *p = pmap;
     }
   if (m)
     {
        if (IMAGE_HAS_ALPHA(im))
           mask = XCreatePixmap(d, w, dw, dh, 1);
        *m = mask;
     }
   __imlib_RenderImage(d, im, pmap, mask, v, cm, depth, sx, sy, sw, sh, 0, 0,
                       dw, dh, antialias, hiq, 0, dither_mask, mat, cmod,
                       OP_COPY);
   ip = __imlib_AddImagePixmapToCache(im, pmap, mask,
                                      dw, dh, d, v, depth, sx, sy,
                                      sw, sh, cm, antialias, hiq, dither_mask,
                                      mod_count);
#ifdef DEBUG_CACHE
   fprintf(stderr,
           "[Imlib2]  Created pixmap.  Reference count is %d, pixmap 0x%08lx, mask 0x%08lx\n",
           ip->references, ip->pixmap, ip->mask);
#endif
   return 1;
}
