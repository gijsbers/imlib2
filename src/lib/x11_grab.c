#include "common.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "x11_grab.h"
#include "x11_ximage.h"

static char         _x_err = 0;
static uint8_t      rtab[256], gtab[256], btab[256];

static int
Tmp_HandleXError(Display * d, XErrorEvent * ev)
{
   _x_err = 1;
   return 0;
}

/* xim->data is properly aligned (malloc'ed), assuming bytes_per_line behaves */
#define PTR(T, im_, y_) (T*)(void*)(im_->data + (im_->bytes_per_line * y_))

void
__imlib_GrabXImageToRGBA(const ImlibContextX11 * x11, uint32_t * data,
                         int x_dst, int y_dst, int w_dst, int h_dst,
                         XImage * xim, XImage * mxim,
                         int x_src, int y_src, int w_src, int h_src, int grab)
{
   int                 x, y, inx, iny;
   int                 depth;
   const uint32_t     *src;
   const uint16_t     *s16;
   uint32_t           *ptr;
   int                 pixel;
   uint16_t            p16;
   int                 bgr = 0;

   if (!data)
      return;

   if (grab)
      XGrabServer(x11->dpy);    /* This may prevent the image to be changed under our feet */

   if (x11->vis->blue_mask > x11->vis->red_mask)
      bgr = 1;

   if (x_src < 0)
      inx = -x_src;
   else
      inx = x_dst;
   if (y_src < 0)
      iny = -y_src;
   else
      iny = y_dst;

   /* go thru the XImage and convert */

   depth = x11->depth;
   if ((depth == 24) && (xim->bits_per_pixel == 32))
      depth = 25;               /* fake depth meaning 24 bit in 32 bpp ximage */

   /* data needs swapping */
#ifdef WORDS_BIGENDIAN
   if (xim->bitmap_bit_order == LSBFirst)
#else
   if (xim->bitmap_bit_order == MSBFirst)
#endif
     {
        switch (depth)
          {
          case 0:
          case 1:
          case 2:
          case 3:
          case 4:
          case 5:
          case 6:
          case 7:
          case 8:
             break;
          case 15:
          case 16:
             for (y = 0; y < h_src; y++)
               {
                  uint16_t           *tmp;

                  tmp = PTR(uint16_t, xim, y);
                  for (x = 0; x < w_src; x++)
                    {
                       *tmp = SWAP16(*tmp);
                       tmp++;
                    }
               }
             break;
          case 24:
          case 25:
          case 30:
          case 32:
             for (y = 0; y < h_src; y++)
               {
                  uint32_t           *tmp;

                  tmp = PTR(uint32_t, xim, y);
                  for (x = 0; x < w_src; x++)
                    {
                       *tmp = SWAP32(*tmp);
                       tmp++;
                    }
               }
             break;
          default:
             break;
          }
     }

   switch (depth)
     {
     case 0:
     case 1:
     case 2:
     case 3:
     case 4:
     case 5:
     case 6:
     case 7:
     case 8:
        if (mxim)
          {
             for (y = 0; y < h_src; y++)
               {
                  ptr = data + ((y + iny) * w_dst) + inx;
                  for (x = 0; x < w_src; x++)
                    {
                       pixel = XGetPixel(xim, x, y);
                       pixel = (btab[pixel & 0xff]) |
                          (gtab[pixel & 0xff] << 8) |
                          (rtab[pixel & 0xff] << 16);
                       if (XGetPixel(mxim, x, y))
                          pixel |= 0xff000000;
                       *ptr++ = pixel;
                    }
               }
          }
        else
          {
             for (y = 0; y < h_src; y++)
               {
                  ptr = data + ((y + iny) * w_dst) + inx;
                  for (x = 0; x < w_src; x++)
                    {
                       pixel = XGetPixel(xim, x, y);
                       *ptr++ = 0xff000000 |
                          (btab[pixel & 0xff]) |
                          (gtab[pixel & 0xff] << 8) |
                          (rtab[pixel & 0xff] << 16);
                    }
               }
          }
        break;
     case 16:
#undef MP
#undef RMSK
#undef GMSK
#undef BMSK
#undef RSH
#undef GSH
#undef BSH
#define MP(x, y) ((XGetPixel(mxim, (x), (y))) ? 0xff: 0)
#define RMSK  0x1f
#define GMSK  0x3f
#define BMSK  0x1f
#define RSH(p)  ((p) >> 11)
#define GSH(p)  ((p) >>  5)
#define BSH(p)  ((p) >>  0)
#define RVAL(p) (255 * (RSH(p) & RMSK)) / RMSK
#define GVAL(p) (255 * (GSH(p) & GMSK)) / GMSK
#define BVAL(p) (255 * (BSH(p) & BMSK)) / BMSK
        if (mxim)
          {
             for (y = 0; y < h_src; y++)
               {
                  s16 = PTR(uint16_t, xim, y);
                  ptr = data + ((y + iny) * w_dst) + inx;
                  for (x = 0; x < w_src; x++)
                    {
                       p16 = *s16++;
                       *ptr++ =
                          PIXEL_ARGB(MP(x, y), RVAL(p16), GVAL(p16), BVAL(p16));
                    }
               }
          }
        else
          {
             for (y = 0; y < h_src; y++)
               {
                  s16 = PTR(uint16_t, xim, y);
                  ptr = data + ((y + iny) * w_dst) + inx;
                  for (x = 0; x < w_src; x++)
                    {
                       p16 = *s16++;
                       *ptr++ =
                          PIXEL_ARGB(0xff, RVAL(p16), GVAL(p16), BVAL(p16));
                    }
               }
          }
        break;
     case 15:
#undef MP
#undef RMSK
#undef GMSK
#undef BMSK
#undef RSH
#undef GSH
#undef BSH
#define MP(x, y) ((XGetPixel(mxim, (x), (y))) ? 0xff: 0)
#define RMSK  0x1f
#define GMSK  0x1f
#define BMSK  0x1f
#define RSH(p)  ((p) >> 10)
#define GSH(p)  ((p) >>  5)
#define BSH(p)  ((p) >>  0)
#define RVAL(p) (255 * (RSH(p) & RMSK)) / RMSK
#define GVAL(p) (255 * (GSH(p) & GMSK)) / GMSK
#define BVAL(p) (255 * (BSH(p) & BMSK)) / BMSK
        if (mxim)
          {
             for (y = 0; y < h_src; y++)
               {
                  s16 = PTR(uint16_t, xim, y);
                  ptr = data + ((y + iny) * w_dst) + inx;
                  for (x = 0; x < w_src; x++)
                    {
                       p16 = *s16++;
                       *ptr++ =
                          PIXEL_ARGB(MP(x, y), RVAL(p16), GVAL(p16), BVAL(p16));
                    }
               }
          }
        else
          {
             for (y = 0; y < h_src; y++)
               {
                  s16 = PTR(uint16_t, xim, y);
                  ptr = data + ((y + iny) * w_dst) + inx;
                  for (x = 0; x < w_src; x++)
                    {
                       p16 = *s16++;
                       *ptr++ =
                          PIXEL_ARGB(0xff, RVAL(p16), GVAL(p16), BVAL(p16));
                    }
               }
          }
        break;
     case 24:
        if (bgr)
          {
             if (mxim)
               {
                  for (y = 0; y < h_src; y++)
                    {
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            pixel = XGetPixel(xim, x, y);
                            pixel = ((pixel << 16) & 0xff0000) |
                               ((pixel) & 0x00ff00) |
                               ((pixel >> 16) & 0x0000ff);
                            if (XGetPixel(mxim, x, y))
                               pixel |= 0xff000000;
                            *ptr++ = pixel;
                         }
                    }
               }
             else
               {
                  for (y = 0; y < h_src; y++)
                    {
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            pixel = XGetPixel(xim, x, y);
                            *ptr++ = 0xff000000 |
                               ((pixel << 16) & 0xff0000) |
                               ((pixel) & 0x00ff00) |
                               ((pixel >> 16) & 0x0000ff);
                         }
                    }
               }
          }
        else
          {
             if (mxim)
               {
                  for (y = 0; y < h_src; y++)
                    {
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            pixel = XGetPixel(xim, x, y) & 0x00ffffff;
                            if (XGetPixel(mxim, x, y))
                               pixel |= 0xff000000;
                            *ptr++ = pixel;
                         }
                    }
               }
             else
               {
                  for (y = 0; y < h_src; y++)
                    {
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            pixel = XGetPixel(xim, x, y);
                            *ptr++ = 0xff000000 | (pixel & 0x00ffffff);
                         }
                    }
               }
          }
        break;
     case 25:
        if (bgr)
          {
             if (mxim)
               {
                  for (y = 0; y < h_src; y++)
                    {
                       src = PTR(uint32_t, xim, y);
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            pixel = ((*src << 16) & 0xff0000) |
                               ((*src) & 0x00ff00) | ((*src >> 16) & 0x0000ff);
                            if (XGetPixel(mxim, x, y))
                               pixel |= 0xff000000;
                            *ptr++ = pixel;
                            src++;
                         }
                    }
               }
             else
               {
                  for (y = 0; y < h_src; y++)
                    {
                       src = PTR(uint32_t, xim, y);
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            *ptr++ = 0xff000000 |
                               ((*src << 16) & 0xff0000) |
                               ((*src) & 0x00ff00) | ((*src >> 16) & 0x0000ff);
                            src++;
                         }
                    }
               }
          }
        else
          {
             if (mxim)
               {
                  for (y = 0; y < h_src; y++)
                    {
                       src = PTR(uint32_t, xim, y);
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            pixel = (*src) & 0x00ffffff;
                            if (XGetPixel(mxim, x, y))
                               pixel |= 0xff000000;
                            *ptr++ = pixel;
                            src++;
                         }
                    }
               }
             else
               {
                  for (y = 0; y < h_src; y++)
                    {
                       src = PTR(uint32_t, xim, y);
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            *ptr++ = 0xff000000 | ((*src) & 0x00ffffff);
                            src++;
                         }
                    }
               }
          }
        break;
     case 30:
        if (bgr)
          {
             if (mxim)
               {
                  for (y = 0; y < h_src; y++)
                    {
                       src = PTR(uint32_t, xim, y);
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            pixel = (((*src) & 0x000003ff) << 14 & 0x00ff0000) |
                               (((*src) & 0x000ffc00) >> 4 & 0x0000ff00) |
                               (((*src) & 0x3ff00000) >> 22 & 0x000000ff);
                            if (XGetPixel(mxim, x, y))
                               pixel |= 0xff000000;
                            *ptr++ = pixel;
                            src++;
                         }
                    }
               }
             else
               {
                  for (y = 0; y < h_src; y++)
                    {
                       src = PTR(uint32_t, xim, y);
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            *ptr++ = 0xff000000 |
                               (((*src) & 0x000003ff) << 14 & 0x00ff0000) |
                               (((*src) & 0x000ffc00) >> 4 & 0x0000ff00) |
                               (((*src) & 0x3ff00000) >> 22 & 0x000000ff);
                            src++;
                         }
                    }
               }
          }
        else
          {
             if (mxim)
               {
                  for (y = 0; y < h_src; y++)
                    {
                       src = PTR(uint32_t, xim, y);
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            pixel = (((*src) & 0x3ff00000) >> 6 & 0x00ff0000) |
                               (((*src) & 0x000ffc00) >> 4 & 0x0000ff00) |
                               (((*src) & 0x000003ff) >> 2 & 0x000000ff);
                            if (XGetPixel(mxim, x, y))
                               pixel |= 0xff000000;
                            *ptr++ = pixel;
                            src++;
                         }
                    }
               }
             else
               {
                  for (y = 0; y < h_src; y++)
                    {
                       src = PTR(uint32_t, xim, y);
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            *ptr++ = 0xff000000 |
                               (((*src) & 0x3ff00000) >> 6 & 0x00ff0000) |
                               (((*src) & 0x000ffc00) >> 4 & 0x0000ff00) |
                               (((*src) & 0x000003ff) >> 2 & 0x000000ff);
                            src++;
                         }
                    }
               }
          }
        break;
     case 32:
        if (bgr)
          {
             if (mxim)
               {
                  for (y = 0; y < h_src; y++)
                    {
                       src = PTR(uint32_t, xim, y);
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            pixel = SWAP32(*src);
                            if (!XGetPixel(mxim, x, y))
                               pixel &= 0x00ffffff;
                            *ptr++ = pixel;
                            src++;
                         }
                    }
               }
             else
               {
                  for (y = 0; y < h_src; y++)
                    {
                       src = PTR(uint32_t, xim, y);
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            *ptr++ = SWAP32(*src);
                            src++;
                         }
                    }
               }
          }
        else
          {
             if (mxim)
               {
                  for (y = 0; y < h_src; y++)
                    {
                       src = PTR(uint32_t, xim, y);
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            pixel = *src++;
                            if (!XGetPixel(mxim, x, y))
                               pixel &= 0x00ffffff;
                            *ptr++ = pixel;
                         }
                    }
               }
             else
               {
                  for (y = 0; y < h_src; y++)
                    {
                       src = PTR(uint32_t, xim, y);
                       ptr = data + ((y + iny) * w_dst) + inx;
                       for (x = 0; x < w_src; x++)
                         {
                            *ptr++ = *src++;
                         }
                    }
               }
          }
        break;
     default:
        break;
     }

   if (grab)
      XUngrabServer(x11->dpy);
}

static              Pixmap
_WindowGetShapeMask(Display * d, Window p,
                    int x, int y, int w, int h, int ww, int wh)
{
   Pixmap              mask;
   XRectangle         *rect;
   int                 rect_num, rect_ord, i;
   XGCValues           gcv;
   GC                  mgc;

   mask = None;

   rect = XShapeGetRectangles(d, p, ShapeBounding, &rect_num, &rect_ord);
   if (!rect)
      return mask;

   if (rect_num == 1 &&
       rect[0].x == 0 && rect[0].y == 0 &&
       rect[0].width == ww && rect[0].height == wh)
      goto done;

   mask = XCreatePixmap(d, p, w, h, 1);

   gcv.foreground = 0;
   gcv.graphics_exposures = False;
   mgc = XCreateGC(d, mask, GCForeground | GCGraphicsExposures, &gcv);

   XFillRectangle(d, mask, mgc, 0, 0, w, h);

   XSetForeground(d, mgc, 1);
   for (i = 0; i < rect_num; i++)
      XFillRectangle(d, mask, mgc, rect[i].x - x, rect[i].y - y,
                     rect[i].width, rect[i].height);

   if (mgc)
      XFreeGC(d, mgc);

 done:
   XFree(rect);

   return mask;
}

static bool
_DrawableCheck(Display * dpy, Drawable draw, XWindowAttributes * patt)
{
   XErrorHandler       prev_erh;

   XSync(dpy, False);
   prev_erh = XSetErrorHandler(Tmp_HandleXError);
   _x_err = 0;

   /* lets see if its a pixmap or not */
   XGetWindowAttributes(dpy, draw, patt);
   XSync(dpy, False);

   XSetErrorHandler(prev_erh);

   return _x_err != 0;
}

int
__imlib_GrabDrawableToRGBA(const ImlibContextX11 * x11, uint32_t * data,
                           int x_dst, int y_dst,
                           int w_dst, int h_dst,
                           Drawable draw, Pixmap mask_,
                           int x_src, int y_src, int w_src, int h_src,
                           char *pdomask, int grab, bool clear,
                           const XWindowAttributes * attr)
{
   XWindowAttributes   xatt;
   bool                is_pixmap, is_shm, is_mshm;
   char                domask;
   int                 i, j;
   int                 width, height;
   Pixmap              mask = mask_;
   XShmSegmentInfo     shminfo, mshminfo;
   XImage             *xim, *mxim;
   XColor              cols[256];

   domask = (pdomask) ? *pdomask : 0;

   if (grab)
      XGrabServer(x11->dpy);

   width = w_src;
   height = h_src;

   if (attr)
     {
        is_pixmap = true;
        xatt.width = attr->width;
        xatt.height = attr->height;
        xatt.depth = attr->depth;
     }
   else
     {
        is_pixmap = _DrawableCheck(x11->dpy, draw, &xatt);

        if (is_pixmap)
          {
             Window              rret;
             unsigned int        bw;

             XGetGeometry(x11->dpy, draw, &rret, &xatt.x, &xatt.y,
                          (unsigned int *)&xatt.width,
                          (unsigned int *)&xatt.height,
                          &bw, (unsigned int *)&xatt.depth);
          }
        else
          {
             XWindowAttributes   ratt;
             Window              cret;

             if (xatt.map_state != IsViewable &&
                 xatt.backing_store == NotUseful)
                goto bail;

             /* Clip source to screen */
             XGetWindowAttributes(x11->dpy, xatt.root, &ratt);
             XTranslateCoordinates(x11->dpy, draw, xatt.root,
                                   0, 0, &xatt.x, &xatt.y, &cret);

             if (xatt.x + x_src < 0)
               {
                  width += xatt.x + x_src;
                  x_src = -xatt.x;
               }
             if (xatt.x + x_src + width > ratt.width)
                width = ratt.width - (xatt.x + x_src);

             if (xatt.y + y_src < 0)
               {
                  height += xatt.y + y_src;
                  y_src = -xatt.y;
               }
             if (xatt.y + y_src + height > ratt.height)
                height = ratt.height - (xatt.y + y_src);

             /* Is this ever relevant? */
             if (xatt.colormap == None)
                xatt.colormap = ratt.colormap;
          }
     }

   /* Clip source to drawable */
   if (x_src < 0)
     {
        x_dst += -x_src;
        width += x_src;
        x_src = 0;
     }
   if (x_src + width > xatt.width)
      width = xatt.width - x_src;

   if (y_src < 0)
     {
        y_dst += -y_src;
        height += y_src;
        y_src = 0;
     }
   if (y_src + height > xatt.height)
      height = xatt.height - y_src;

   /* Clip source to destination */
   if (x_dst < 0)
     {
        x_src -= x_dst;
        width += x_dst;
        x_dst = 0;
     }
   if (x_dst + width > w_dst)
      width = w_dst - x_dst;

   if (y_dst < 0)
     {
        y_src -= y_dst;
        height += y_dst;
        y_dst = 0;
     }
   else if (y_dst + height > h_dst)
      height = h_dst - y_dst;

   if (width <= 0 || height <= 0)
      goto bail;

   w_src = width;
   h_src = height;

   if ((!is_pixmap) && (domask) && (mask == None))
      mask = _WindowGetShapeMask(x11->dpy, draw,
                                 x_src, y_src, w_src, h_src,
                                 xatt.width, xatt.height);

   /* Create an Ximage (shared or not) */
   xim = __imlib_ShmGetXImage(x11, draw, xatt.depth,
                              x_src, y_src, w_src, h_src, &shminfo);
   is_shm = !!xim;

   if (!xim)
      xim = XGetImage(x11->dpy, draw, x_src, y_src,
                      w_src, h_src, 0xffffffff, ZPixmap);
   if (!xim)
      goto bail;

   is_mshm = false;
   mxim = NULL;
   if ((mask) && (domask))
     {
        mxim = __imlib_ShmGetXImage(x11, mask, 1, 0, 0, w_src, h_src,
                                    &mshminfo);
        is_mshm = !!mxim;
        if (!mxim)
           mxim = XGetImage(x11->dpy, mask, 0, 0, w_src, h_src,
                            0xffffffff, ZPixmap);
     }

   if ((is_shm) || (is_mshm))
     {
        XSync(x11->dpy, False);
        if (grab)
           XUngrabServer(x11->dpy);
        XSync(x11->dpy, False);
     }
   else if (grab)
      XUngrabServer(x11->dpy);

   if ((xatt.depth == 1) && (!x11->cmap) && (is_pixmap))
     {
        rtab[0] = 255;
        gtab[0] = 255;
        btab[0] = 255;
        rtab[1] = 0;
        gtab[1] = 0;
        btab[1] = 0;
     }
   else if (xatt.depth <= 8)
     {
        Colormap            cmap = x11->cmap;

        if (!cmap)
          {
             if (is_pixmap)
                cmap = DefaultColormap(x11->dpy, DefaultScreen(x11->dpy));
             else
                cmap = xatt.colormap;
          }

        for (i = 0; i < (1 << xatt.depth); i++)
          {
             cols[i].pixel = i;
             cols[i].flags = DoRed | DoGreen | DoBlue;
          }
        XQueryColors(x11->dpy, cmap, cols, 1 << xatt.depth);
        for (i = 0; i < (1 << xatt.depth); i++)
          {
             rtab[i] = cols[i].red >> 8;
             gtab[i] = cols[i].green >> 8;
             btab[i] = cols[i].blue >> 8;
          }
     }

#define CLEAR(x0, x1, y0, y1) \
   do { \
   for (j = y0; j < y1; j++) \
      for (i = x0; i < x1; i++) \
         data[j * w_dst + i] = 0; \
   } while(0)

   if (clear)
     {
        CLEAR(0, w_dst, 0, y_dst);
        CLEAR(0, w_dst, y_dst + h_src, h_dst);
        CLEAR(0, x_dst, y_dst, y_dst + h_src);
        CLEAR(x_dst + w_src, w_dst, y_dst, y_dst + h_src);
     }

   __imlib_GrabXImageToRGBA(x11, data, x_dst, y_dst, w_dst, h_dst,
                            xim, mxim, x_src, y_src, w_src, h_src, 0);

   /* destroy the Ximage */
   if (is_shm)
      __imlib_ShmDestroyXImage(x11, xim, &shminfo);
   else
      XDestroyImage(xim);

   if (mxim)
     {
        if (is_mshm)
           __imlib_ShmDestroyXImage(x11, mxim, &mshminfo);
        else
           XDestroyImage(mxim);
     }

   if (mask != None && mask != mask_)
      XFreePixmap(x11->dpy, mask);

   if (pdomask)
     {
        /* Set domask according to whether or not we have useful alpha data */
        if (xatt.depth == 32)
           *pdomask = 1;
        else if (mask == None)
           *pdomask = 0;
     }

   return 0;

 bail:
   if (grab)
      XUngrabServer(x11->dpy);
   return 1;
}

int
__imlib_GrabDrawableScaledToRGBA(const ImlibContextX11 * x11, uint32_t * data,
                                 int nu_x_dst, int nu_y_dst,
                                 int w_dst, int h_dst,
                                 Drawable draw, Pixmap mask_,
                                 int x_src, int y_src, int w_src, int h_src,
                                 char *pdomask, int grab, bool clear)
{
   int                 rc;
   XWindowAttributes   xatt;
   bool                is_pixmap;
   int                 h_tmp, i, xx;
   int                 x_src_, y_src_, width, height, x_dst_, y_dst_;
   XGCValues           gcv;
   GC                  gc, mgc = NULL;
   Pixmap              mask = mask_;
   Pixmap              psc, msc;

   h_tmp = h_dst > h_src ? h_dst : h_src;

   gcv.foreground = 0;
   gcv.subwindow_mode = IncludeInferiors;
   gcv.graphics_exposures = False;
   gc = XCreateGC(x11->dpy, draw, GCSubwindowMode | GCGraphicsExposures, &gcv);

   is_pixmap = _DrawableCheck(x11->dpy, draw, &xatt);

   width = w_dst;
   height = h_dst;

   if (is_pixmap)
     {
        Window              rret;
        unsigned int        bw;

        XGetGeometry(x11->dpy, draw, &rret, &xatt.x, &xatt.y,
                     (unsigned int *)&xatt.width, (unsigned int *)&xatt.height,
                     &bw, (unsigned int *)&xatt.depth);
     }
   else
     {
        XWindowAttributes   ratt;
        Window              cret;

        if ((xatt.map_state != IsViewable) && (xatt.backing_store == NotUseful))
           return 1;

        /* Clip source to screen */
        XGetWindowAttributes(x11->dpy, xatt.root, &ratt);
        XTranslateCoordinates(x11->dpy, draw, xatt.root,
                              0, 0, &xatt.x, &xatt.y, &cret);

#if 0
        if (xatt.x + x_src < 0)
          {
             width += xatt.x + x_src;
             x_src = -xatt.x;
          }
        if (xatt.x + x_src + width > ratt.width)
           width = ratt.width - (xatt.x + x_src);

        if (xatt.y + y_src < 0)
          {
             height += xatt.y + y_src;
             y_src = -xatt.y;
          }
        if (xatt.y + y_src + height > ratt.height)
           height = ratt.height - (xatt.y + y_src);
#endif

        if (*pdomask && mask == None)
          {
             mask = _WindowGetShapeMask(x11->dpy, draw,
                                        0, 0, w_src, h_src, w_src, h_src);
             if (mask == None)
                *pdomask = 0;
          }
     }

   psc = XCreatePixmap(x11->dpy, draw, w_dst, h_tmp, xatt.depth);

   if (!is_pixmap && *pdomask)
     {
        msc = XCreatePixmap(x11->dpy, draw, w_dst, h_tmp, 1);
        mgc =
           XCreateGC(x11->dpy, msc, GCForeground | GCGraphicsExposures, &gcv);
     }
   else
      msc = None;

   /* Copy source height vertical lines from input to intermediate */
   for (i = 0; i < w_dst; i++)
     {
        xx = (w_src * i) / w_dst;
        XCopyArea(x11->dpy, draw, psc, gc, x_src + xx, y_src, 1, h_src, i, 0);
        if (msc != None)
           XCopyArea(x11->dpy, mask, msc, mgc, xx, 0, 1, h_src, i, 0);
     }

   /* Copy destination width horzontal lines from/to intermediate */
   if (h_dst > h_src)
     {
        for (i = h_dst - 1; i > 0; i--)
          {
             xx = (h_src * i) / h_dst;
             if (xx == i)
                continue;       /* Don't copy onto self */
             XCopyArea(x11->dpy, psc, psc, gc, 0, xx, w_dst, 1, 0, i);
             if (msc != None)
                XCopyArea(x11->dpy, msc, msc, mgc, 0, xx, w_dst, 1, 0, i);
          }
     }
   else
     {
        for (i = 0; i < h_dst; i++)
          {
             xx = (h_src * i) / h_dst;
             if (xx == i)
                continue;       /* Don't copy onto self */
             XCopyArea(x11->dpy, psc, psc, gc, 0, xx, w_dst, 1, 0, i);
             if (msc != None)
                XCopyArea(x11->dpy, msc, msc, mgc, 0, xx, w_dst, 1, 0, i);
          }
     }

   x_dst_ = y_dst_ = 0;         /* Clipped x_dst, y_dst */
   x_src_ = y_src_ = 0;         /* Clipped, scaled x_src, y_src */

   /* Adjust offsets and areas to not touch "non-existing" pixels */
   if (x_src < 0)
     {
        xx = (-x_src * w_dst + w_src - 1) / w_src;
        width -= xx;
        x_src_ = x_dst_ = xx;
     }
   if (x_src + w_src > xatt.width)
     {
        xx = x_src + w_src - xatt.width;
        xx = (xx * w_dst + w_src - 1) / w_src;
        width -= xx;
     }

   if (y_src < 0)
     {
        xx = (-y_src * h_dst + h_src - 1) / h_src;
        height -= xx;
        y_src_ = y_dst_ = xx;
     }
   if (y_src + h_src > xatt.height)
     {
        xx = y_src + h_src - xatt.height;
        xx = (xx * h_dst + h_src - 1) / h_src;
        height -= xx;
     }

   xatt.width = w_dst;          /* Not the actual pixmap size but the scaled size */
   xatt.height = h_dst;
   rc = __imlib_GrabDrawableToRGBA(x11, data, x_dst_, y_dst_, w_dst, h_dst,
                                   psc, msc, x_src_, y_src_, width, height,
                                   pdomask, grab, clear, &xatt);

   if (mgc)
      XFreeGC(x11->dpy, mgc);
   if (msc != None && msc != mask)
      XFreePixmap(x11->dpy, msc);
   if (mask != None && mask != mask_)
      XFreePixmap(x11->dpy, mask);
   XFreeGC(x11->dpy, gc);
   if (psc != draw)
      XFreePixmap(x11->dpy, psc);

   return rc;
}
