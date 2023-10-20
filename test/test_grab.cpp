#include <gtest/gtest.h>

#include <X11/Xutil.h>
#include <Imlib2.h>

#include "config.h"
#include "test.h"

typedef struct {
   Display            *dpy;
   Window              root;
   Visual             *vis;
   Colormap            cmap;
   Visual             *argb_vis;
   Colormap            argb_cmap;
   int                 depth;
   int                 scale;
   const char         *test;
} xd_t;

static xd_t         xd;

static Visual      *
_x11_vis_argb(void)
{
   XVisualInfo        *xvi, xvit;
   int                 i, num;
   Visual             *vis;

   xvit.screen = DefaultScreen(xd.dpy);
   xvit.depth = 32;
   xvit.c_class = TrueColor;

   xvi = XGetVisualInfo(xd.dpy,
                        VisualScreenMask | VisualDepthMask | VisualClassMask,
                        &xvit, &num);
   if (!xvi)
      return NULL;

   for (i = 0; i < num; i++)
     {
        if (xvi[i].depth == 32)
           break;
     }

   vis = (i < num) ? xvi[i].visual : NULL;

   XFree(xvi);

   xd.argb_vis = vis;
   xd.argb_cmap = XCreateColormap(xd.dpy, xd.root, vis, AllocNone);

   return vis;
}

static void
_x11_init(int depth)
{
   xd.dpy = XOpenDisplay(NULL);
   if (!xd.dpy)
     {
        fprintf(stderr, "Can't open display\n");
        exit(1);
     }

   xd.root = DefaultRootWindow(xd.dpy);
   if (depth == 32)
     {
        xd.vis = _x11_vis_argb();
        xd.cmap = xd.argb_cmap;
     }
   else
     {
        xd.vis = DefaultVisual(xd.dpy, DefaultScreen(xd.dpy));
        xd.cmap = DefaultColormap(xd.dpy, DefaultScreen(xd.dpy));
     }

   imlib_context_set_display(xd.dpy);
   imlib_context_set_visual(xd.vis);
   imlib_context_set_colormap(xd.cmap);
}

static void
_x11_fini(void)
{
   XCloseDisplay(xd.dpy);
}

static              Pixmap
_pmap_mk_fill_solid(int w, int h, unsigned int color)
{
   XGCValues           gcv;
   GC                  gc;
   Pixmap              pmap;

   pmap = XCreatePixmap(xd.dpy, xd.root, w, h, xd.depth);

   gcv.foreground = color;
   gcv.graphics_exposures = False;
   gc = XCreateGC(xd.dpy, pmap, GCForeground | GCGraphicsExposures, &gcv);

   XFillRectangle(xd.dpy, pmap, gc, 0, 0, w, h);

   XFreeGC(xd.dpy, gc);

   return pmap;
}

static void
_img_dump(Imlib_Image im, const char *file)
{
   static int          seqn = 0;
   char                buf[1024];

   snprintf(buf, sizeof(buf), file, seqn++);
   imlib_context_set_image(im);
   imlib_save_image(buf);
}

DATA32              color = 0;

static void
_test_grab_1(int w, int h, int x0, int y0)
{
   Imlib_Image         im;
   DATA32             *dptr, pix, col;
   int                 x, y, err;
   int                 xs, ys, ws, hs;
   char                buf[128];

   xs = x0;
   ws = w;
   ys = y0;
   hs = h;
   if (xd.scale > 0)
     {
        xs *= xd.scale;
        ws *= xd.scale;
        ys *= xd.scale;
        hs *= xd.scale;
     }
   if (xd.scale < 0)
     {
        xs = (xs & ~1) / -xd.scale;
        ws /= -xd.scale;
        ys = (ys & ~1) / -xd.scale;
        hs /= -xd.scale;
     }

   D("%s: %3dx%3d(%3d,%3d) -> %3dx%3d(%d,%d)\n", __func__,
     w, h, x0, y0, ws, hs, xs, ys);

   if (xd.scale == 0)
      im = imlib_create_image_from_drawable(None, x0, y0, w, h, 0);
   else
      im = imlib_create_scaled_image_from_drawable(None, x0, y0, w, h,
                                                   ws, hs, 0, 0);
   imlib_context_set_image(im);
   snprintf(buf, sizeof(buf), "%s/%s-%%d.png", IMG_GEN, xd.test);
   _img_dump(im, buf);

   dptr = imlib_image_get_data_for_reading_only();
   err = 0;
   for (y = 0; y < hs; y++)
     {
        for (x = 0; x < ws; x++)
          {
             pix = *dptr++;
             if (xs + x < 0 || ys + y < 0 || xs + x >= ws || ys + y >= hs)
               {
#if 1
                  // FIXME - Pixels outside source drawable are not properly initialized
                  if (x0 != 0 || y0 != 0)
                     continue;
#endif
                  col = 0x00000000;     // Use 0xff000000 if non-alpa
               }
             else
               {
                  col = color;
               }
             if (pix != col)
               {
                  D2("%3d,%3d (%3d,%3d): %08x != %08x\n",
                     x, y, x0, y0, pix, col);
                  err = 1;
               }
          }
     }

   EXPECT_FALSE(err);

   imlib_free_image_and_decache();
}

static void
_test_grab(const char *test, int depth, int scale, int opt)
{
   Pixmap              pmap;
   int                 w, h, d;

   color = 0xffff0000;

   _x11_init(depth);

   xd.depth = depth;
   xd.scale = scale;
   xd.test = test;

   w = 32;
   h = 45;

   pmap = _pmap_mk_fill_solid(w, h, color);
   imlib_context_set_drawable(pmap);

   switch (opt)
     {
     case 0:
        _test_grab_1(w, h, 0, 0);
        break;
     case 1:
        d = 1;
        _test_grab_1(w, h, -d, -d);
        _test_grab_1(w, h, -d, d);
        _test_grab_1(w, h, d, d);
        _test_grab_1(w, h, d, -d);
        if (scale < 0)
          {
             d = 2;
             _test_grab_1(w, h, -d, -d);
             _test_grab_1(w, h, -d, d);
             _test_grab_1(w, h, d, d);
             _test_grab_1(w, h, d, -d);
          }
        break;
     }

   XFreePixmap(xd.dpy, pmap);

   _x11_fini();
}

TEST(GRAB, grab_noof_24_s0)
{
   _test_grab("grab_noof_24_s0", 24, 0, 0);
}

TEST(GRAB, grab_noof_24_s1)
{
   _test_grab("grab_noof_24_s1", 24, 1, 0);
}

TEST(GRAB, grab_noof_24_su2)
{
   _test_grab("grab_noof_24_su2", 24, 2, 0);
}

TEST(GRAB, grab_noof_24_sd2)
{
   _test_grab("grab_noof_24_sd2", 24, -2, 0);
}

TEST(GRAB, grab_noof_32_s0)
{
   _test_grab("grab_noof_32_s0", 32, 0, 0);
}

TEST(GRAB, grab_noof_32_s1)
{
   _test_grab("grab_noof_32_s1", 32, 1, 0);
}

TEST(GRAB, grab_noof_32_su2)
{
   _test_grab("grab_noof_32_su2", 32, 2, 0);
}

TEST(GRAB, grab_noof_32_sd2)
{
   _test_grab("grab_noof_32_sd2", 32, -2, 0);
}

TEST(GRAB, grab_offs_24_s0)
{
   _test_grab("grab_offs_24_s0", 24, 0, 1);
}

TEST(GRAB, grab_offs_24_s1)
{
   _test_grab("grab_offs_24_s1", 24, 1, 1);
}

TEST(GRAB, grab_offs_24_su2)
{
   _test_grab("grab_offs_24_su2", 24, 2, 1);
}

TEST(GRAB, grab_offs_24_sd2)
{
   _test_grab("grab_offs_24_sd2", 24, -2, 1);
}

TEST(GRAB, grab_offs_32_s0)
{
   _test_grab("grab_offs_32_s0", 32, 0, 1);
}

TEST(GRAB, grab_offs_32_s1)
{
   _test_grab("grab_offs_32_s1", 32, 1, 1);
}

TEST(GRAB, grab_offs_32_su2)
{
   _test_grab("grab_offs_32_su2", 32, 2, 1);
}

TEST(GRAB, grab_offs_32_sd2)
{
   _test_grab("grab_offs_32_sd2", 32, -2, 1);
}
