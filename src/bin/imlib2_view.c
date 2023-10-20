#include "config.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "Imlib2.h"

Display            *disp;
Window              win;
Pixmap              pm = 0;
int                 depth;
int                 image_width = 0, image_height = 0;
int                 window_width = 0, window_height = 0;
double              scale_x = 1.;
double              scale_y = 1.;
Imlib_Image         bg_im = NULL;

static Atom         ATOM_WM_DELETE_WINDOW = None;
static Atom         ATOM_WM_PROTOCOLS = None;

#define MAX_DIM	32767

#define SCALE_X(x) (int)(scale_x * (x) + .5)
#define SCALE_Y(x) (int)(scale_y * (x) + .5)

static int
progress(Imlib_Image im, char percent, int update_x, int update_y,
         int update_w, int update_h)
{
   int                 up_wx, up_wy, up_ww, up_wh;

   /* first time it's called */
   imlib_context_set_drawable(pm);
   imlib_context_set_anti_alias(0);
   imlib_context_set_dither(0);
   imlib_context_set_blend(0);
   if (image_width == 0)
     {
        int                 x, y, onoff;

        imlib_context_set_image(im);
        image_width = imlib_image_get_width();
        image_height = imlib_image_get_height();
        window_width = SCALE_X(image_width);
        window_height = SCALE_Y(image_height);
        if (window_width > MAX_DIM)
          {
             window_width = MAX_DIM;
             scale_x = (double)MAX_DIM / image_width;
          }
        if (window_height > MAX_DIM)
          {
             window_height = MAX_DIM;
             scale_y = (double)MAX_DIM / image_height;
          }
        if (pm)
           XFreePixmap(disp, pm);
        pm = XCreatePixmap(disp, win, window_width, window_height, depth);
        imlib_context_set_drawable(pm);
        if (bg_im)
          {
             imlib_context_set_image(bg_im);
             imlib_free_image_and_decache();
          }
        bg_im = imlib_create_image(image_width, image_height);
        imlib_context_set_image(bg_im);
        for (y = 0; y < image_height; y += 8)
          {
             onoff = (y / 8) & 0x1;
             for (x = 0; x < image_width; x += 8)
               {
                  if (onoff)
                     imlib_context_set_color(144, 144, 144, 255);
                  else
                     imlib_context_set_color(100, 100, 100, 255);
                  imlib_image_fill_rectangle(x, y, 8, 8);
                  onoff++;
                  if (onoff == 2)
                     onoff = 0;
               }
          }
        imlib_render_image_part_on_drawable_at_size(0, 0, image_width,
                                                    image_height, 0, 0,
                                                    window_width,
                                                    window_height);
        XSetWindowBackgroundPixmap(disp, win, pm);
        XResizeWindow(disp, win, window_width, window_height);
        XMapWindow(disp, win);
        XSync(disp, False);
     }

   imlib_context_set_anti_alias(0);
   imlib_context_set_dither(0);
   imlib_context_set_blend(1);
   imlib_blend_image_onto_image(im, 0,
                                update_x, update_y, update_w, update_h,
                                update_x, update_y, update_w, update_h);

   up_wx = SCALE_X(update_x);
   up_wy = SCALE_Y(update_y);
   up_ww = SCALE_X(update_w);
   up_wh = SCALE_Y(update_h);
   imlib_context_set_blend(0);
   imlib_render_image_part_on_drawable_at_size(update_x, update_y,
                                               update_w, update_h,
                                               up_wx, up_wy, up_ww, up_wh);
   XClearArea(disp, win, up_wx, up_wy, up_ww, up_wh, False);
   XFlush(disp);
   return 1;
}

int
main(int argc, char **argv)
{
   char               *s;
   Imlib_Image        *im = NULL;
   char               *file = NULL;
   int                 no, inc;
   int                 verbose;

   verbose = 0;

   for (;;)
     {
        argv++;
        argc--;
        if (argc <= 0)
           break;
        s = argv[0];
        if (*s++ != '-')
           break;
        switch (*s)
          {
          default:
             break;
          case 's':            /* Scale (window size wrt. image size) */
             if (argc-- < 2)
                break;
             argv++;
             scale_x = scale_y = atof(argv[0]);
             break;
          case 'v':
             verbose += 1;
             break;
          }
     }

   if (argc <= 0)
     {
        fprintf(stderr, "imlib2_view [-s <scale factor>] file...\n");
        return 1;
     }

   disp = XOpenDisplay(NULL);
   if (!disp)
     {
        fprintf(stderr, "Cannot open display\n");
        return 1;
     }

   depth = DefaultDepth(disp, DefaultScreen(disp));

   win = XCreateSimpleWindow(disp, DefaultRootWindow(disp), 0, 0, 10, 10,
                             0, 0, 0);
   XSelectInput(disp, win, KeyPressMask | ButtonPressMask | ButtonReleaseMask |
                ButtonMotionMask | PointerMotionMask);

   ATOM_WM_PROTOCOLS = XInternAtom(disp, "WM_PROTOCOLS", False);
   ATOM_WM_DELETE_WINDOW = XInternAtom(disp, "WM_DELETE_WINDOW", False);
   XSetWMProtocols(disp, win, &ATOM_WM_DELETE_WINDOW, 1);

   imlib_context_set_display(disp);
   imlib_context_set_visual(DefaultVisual(disp, DefaultScreen(disp)));
   imlib_context_set_colormap(DefaultColormap(disp, DefaultScreen(disp)));
   imlib_context_set_progress_function(progress);
   imlib_context_set_progress_granularity(10);
   imlib_context_set_drawable(win);

   no = -1;
   for (im = NULL; !im;)
     {
        no++;
        if (no == argc)
          {
             fprintf(stderr, "No loadable image\n");
             exit(0);
          }
        file = argv[no];
        if (verbose)
           printf("Show  %d: '%s'\n", no, file);
        image_width = 0;
        im = imlib_load_image(file);
     }

   imlib_context_set_image(im);

   for (;;)
     {
        int                 x, y, b, count, fdsize, xfd, timeout = 0;
        XEvent              ev;
        static int          zoom_mode = 0, zx, zy;
        static double       zoom = 1.0;
        struct timeval      tval;
        fd_set              fdset;
        double              t1;
        KeySym              key;

        XFlush(disp);
        XNextEvent(disp, &ev);
        switch (ev.type)
          {
          default:
             break;

          case ClientMessage:
             if (ev.xclient.message_type == ATOM_WM_PROTOCOLS &&
                 (Atom) ev.xclient.data.l[0] == ATOM_WM_DELETE_WINDOW)
                return 0;
             break;
          case KeyPress:
             key = XLookupKeysym(&ev.xkey, 0);
             if (key == XK_q || key == XK_Escape)
                return 0;
             if (key == XK_Right)
                goto show_next;
             if (key == XK_Left)
                goto show_prev;
             break;
          case ButtonPress:
             b = ev.xbutton.button;
             x = ev.xbutton.x;
             y = ev.xbutton.y;
             if (b == 3)
               {
                  zoom_mode = 1;
                  zx = x;
                  zy = y;
                  imlib_context_set_drawable(pm);
                  imlib_context_set_image(bg_im);
                  imlib_context_set_anti_alias(0);
                  imlib_context_set_dither(0);
                  imlib_context_set_blend(0);
                  imlib_render_image_part_on_drawable_at_size
                     (0, 0, image_width, image_height,
                      0, 0, window_width, window_height);
                  XClearWindow(disp, win);
               }
             break;
          case ButtonRelease:
             b = ev.xbutton.button;
             x = ev.xbutton.x;
             y = ev.xbutton.y;
             if (b == 3)
                zoom_mode = 0;
             if (b == 1)
                goto show_next;
             if (b == 2)
                goto show_prev;
             break;
          case MotionNotify:
             while (XCheckTypedWindowEvent(disp, win, MotionNotify, &ev));
             x = ev.xmotion.x;
             y = ev.xmotion.y;
             if (zoom_mode)
               {
                  int                 sx, sy, sw, sh, dx, dy, dw, dh;

                  zoom = ((double)x - (double)zx) / 32.0;
                  if (zoom < 0)
                     zoom = 1.0 + ((zoom * 32.0) / ((double)(zx + 1)));
                  else
                     zoom += 1.0;
                  if (zoom <= 0.0001)
                     zoom = 0.0001;
                  if (zoom > 1.0)
                    {
                       dx = 0;
                       dy = 0;
                       dw = window_width;
                       dh = window_height;

                       sx = zx - (zx / zoom);
                       sy = zy - (zy / zoom);
                       sw = image_width / zoom;
                       sh = image_height / zoom;
                    }
                  else
                    {
                       dx = zx - (zx * zoom);
                       dy = zy - (zy * zoom);
                       dw = window_width * zoom;
                       dh = window_height * zoom;

                       sx = 0;
                       sy = 0;
                       sw = image_width;
                       sh = image_height;
                    }
                  imlib_context_set_anti_alias(0);
                  imlib_context_set_dither(0);
                  imlib_context_set_blend(0);
                  imlib_context_set_image(bg_im);
                  imlib_render_image_part_on_drawable_at_size
                     (sx, sy, sw, sh, dx, dy, dw, dh);
                  XClearWindow(disp, win);
                  XFlush(disp);
                  timeout = 1;
               }
             break;

           show_next:
             inc = 1;
             goto show_next_prev;
           show_prev:
             inc = -1;
             goto show_next_prev;
           show_next_prev:
             zoom = 1.0;
             zoom_mode = 0;
             imlib_context_set_image(im);
             imlib_free_image_and_decache();
             for (im = NULL; !im;)
               {
                  no += inc;
                  if (no >= argc)
                    {
                       inc = -1;
                       continue;
                    }
                  else if (no < 0)
                    {
                       inc = 1;
                       continue;
                    }
                  file = argv[no];
                  if (verbose)
                     printf("Show  %d: '%s'\n", no, file);
                  image_width = 0;
                  im = imlib_load_image(file);
               }
             imlib_context_set_image(im);
             break;
          }

        if (XPending(disp))
           continue;

        t1 = 0.2;
        tval.tv_sec = (long)t1;
        tval.tv_usec = (long)((t1 - ((double)tval.tv_sec)) * 1000000);
        xfd = ConnectionNumber(disp);
        fdsize = xfd + 1;
        FD_ZERO(&fdset);
        FD_SET(xfd, &fdset);

        if (timeout)
           count = select(fdsize, &fdset, NULL, NULL, &tval);
        else
           count = select(fdsize, &fdset, NULL, NULL, NULL);

        if (count < 0)
          {
             if ((errno == ENOMEM) || (errno == EINVAL) || (errno == EBADF))
                exit(1);
          }
        else
          {
             if ((count == 0) && (timeout))
               {
                  int                 sx, sy, sw, sh, dx, dy, dw, dh;

                  if (zoom > 1.0)
                    {
                       dx = 0;
                       dy = 0;
                       dw = window_width;
                       dh = window_height;

                       sx = zx - (zx / zoom);
                       sy = zy - (zy / zoom);
                       sw = image_width / zoom;
                       sh = image_height / zoom;
                    }
                  else
                    {
                       dx = zx - (zx * zoom);
                       dy = zy - (zy * zoom);
                       dw = window_width * zoom;
                       dh = window_height * zoom;

                       sx = 0;
                       sy = 0;
                       sw = image_width;
                       sh = image_height;
                    }
                  imlib_context_set_anti_alias(1);
                  imlib_context_set_dither(1);
                  imlib_context_set_blend(0);
                  imlib_context_set_image(bg_im);
                  imlib_render_image_part_on_drawable_at_size
                     (sx, sy, sw, sh, dx, dy, dw, dh);
                  XClearWindow(disp, win);
                  XFlush(disp);
                  timeout = 0;
               }
          }
     }
   return 0;
}
