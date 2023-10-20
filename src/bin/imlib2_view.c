#include "config.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "Imlib2.h"
#include "props.h"

Display            *disp;

static int          debug = 0;
static int          verbose = 0;
static Window       win;
static Pixmap       pm = 0;
static int          image_width = 0, image_height = 0;
static int          window_width = 0, window_height = 0;
static Imlib_Image  bg_im = NULL;

static char         opt_cache = 0;
static char         opt_scale = 0;
static double       opt_scale_x = 1.;
static double       opt_scale_y = 1.;
static double       opt_sgrab_x = 1.;
static double       opt_sgrab_y = 1.;
static char         opt_progress_granularity = 10;
static char         opt_progress_print = 0;
static int          opt_progress_delay = 0;

#define Dprintf if (debug) printf
#define Vprintf if (verbose) printf

#define MAX_DIM	32767

#define SCALE_X(x) (int)(scale_x * (x) + .5)
#define SCALE_Y(x) (int)(scale_y * (x) + .5)

#define HELP \
   "Usage:\n" \
   "  imlib2_view [OPTIONS] {FILE | XID}...\n" \
   "OPTIONS:\n" \
   "  -c         : Enable image caching\n" \
   "  -d         : Enable debug\n" \
   "  -g N       : Set progress granularity to N%% (default 10(%%))\n" \
   "  -l N       : Introduce N ms delay in progress callback (default 0)\n" \
   "  -p         : Print info in progress callback (default no)\n" \
   "  -s Sx[,Sy] : Set render x/y scaling factors to Sx,Sy (default 1.0)\n" \
   "  -S Sx[,Sy] : Set grab x/y scaling factors to Sx,Sy (default 1.0)\n" \
   "  -v         : Increase verbosity\n"

static void
usage(void)
{
   printf(HELP);
}

static int
progress(Imlib_Image im, char percent, int update_x, int update_y,
         int update_w, int update_h)
{
   static double       scale_x = 0., scale_y = 0.;
   int                 up_wx, up_wy, up_ww, up_wh;

   if (opt_progress_print)
      printf("%s: %3d%% %4d,%4d %4dx%4d\n",
             __func__, percent, update_x, update_y, update_w, update_h);

   /* first time it's called */
   imlib_context_set_drawable(pm);
   imlib_context_set_anti_alias(0);
   imlib_context_set_dither(0);
   imlib_context_set_blend(0);
   if (image_width == 0)
     {
        int                 x, y, onoff;

        scale_x = opt_scale_x;
        scale_y = opt_scale_y;

        window_width = DisplayWidth(disp, DefaultScreen(disp));
        window_height = DisplayHeight(disp, DefaultScreen(disp));
        window_width -= 32;     /* Allow for decorations */
        window_height -= 32;
        Dprintf("Screen WxH=%dx%d\n", window_width, window_height);

        imlib_context_set_image(im);
        image_width = imlib_image_get_width();
        image_height = imlib_image_get_height();
        Dprintf("Image  WxH=%dx%d\n", image_width, image_height);

        if (!opt_scale &&
            (image_width > window_width || image_height > window_height))
          {
             scale_x = scale_y = 1.;
             while (window_width < SCALE_X(image_width) ||
                    window_height < SCALE_Y(image_height))
               {
                  scale_x *= .5;
                  scale_y = scale_x;
                  Dprintf(" scale WxH=%.3fx%.3f\n", scale_x, scale_y);
               }
          }

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
        Dprintf("Window WxH=%dx%d\n", window_width, window_height);

        if (pm)
           XFreePixmap(disp, pm);
        pm = XCreatePixmap(disp, win, window_width, window_height,
                           DefaultDepth(disp, DefaultScreen(disp)));
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
        XClearWindow(disp, win);
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

   if (opt_progress_delay > 0)
      usleep(opt_progress_delay);

   return 1;
}

static              Imlib_Image
load_image(int no, const char *name)
{
   Imlib_Image         im;
   char               *ptr;
   Drawable            draw;

   Vprintf("Show  %d: '%s'\n", no, name);

   image_width = 0;             /* Force redraw in progress() */

   draw = strtoul(name, &ptr, 0);
   if (*ptr != '\0')
      draw = 0;

   if (draw)
     {
        Window              rr;
        int                 x, y;
        unsigned int        w, h, bw;
        unsigned int        wo, ho;
        unsigned int        depth;
        int                 get_alpha = 1;

        XGetGeometry(disp, draw, &rr, &x, &y, &w, &h, &bw, &depth);

        imlib_context_set_drawable(draw);

        Vprintf("Drawable: %#lx: x,y: %d,%d  wxh=%ux%u  bw=%u  depth=%u\n",
                draw, x, y, w, h, bw, depth);

        wo = w * opt_sgrab_x;
        ho = h * opt_sgrab_y;
        im = imlib_create_scaled_image_from_drawable(None, 0, 0, w, h, wo, ho,
                                                     1, (get_alpha) ? 1 : 0);

        progress(im, 100, 0, 0, wo, ho);
     }
   else
     {
        im = imlib_load_image(name);
     }

   return im;
}

int
main(int argc, char **argv)
{
   int                 opt;
   Imlib_Image        *im;
   int                 no, inc;

   verbose = 0;

   while ((opt = getopt(argc, argv, "cdg:l:ps:S:v")) != -1)
     {
        switch (opt)
          {
          case 'c':
             opt_cache = 1;
             break;
          case 'd':
             debug += 1;
             break;
          case 'g':
             opt_progress_granularity = atoi(optarg);
             break;
          case 'l':
             opt_progress_delay = 1000 * atoi(optarg);
             break;
          case 'p':
             opt_progress_print = 1;
             break;
          case 's':            /* Scale (window size wrt. image size) */
             opt_scale = 1;
             opt_scale_y = 0.f;
             sscanf(optarg, "%lf,%lf", &opt_scale_x, &opt_scale_y);
             if (opt_scale_y == 0.f)
                opt_scale_y = opt_scale_x;
             break;
          case 'S':            /* Scale on grab */
             opt_sgrab_y = 0.f;
             sscanf(optarg, "%lf,%lf", &opt_sgrab_x, &opt_sgrab_y);
             if (opt_sgrab_y == 0.f)
                opt_sgrab_y = opt_sgrab_x;
             break;
          case 'v':
             verbose += 1;
             break;
          default:
             usage();
             return 1;
          }
     }

   argc -= optind;
   argv += optind;

   if (argc <= 0)
     {
        usage();
        return 1;
     }

   disp = XOpenDisplay(NULL);
   if (!disp)
     {
        fprintf(stderr, "Cannot open display\n");
        return 1;
     }

   win = XCreateSimpleWindow(disp, DefaultRootWindow(disp), 0, 0, 10, 10,
                             0, 0, 0);
   XSelectInput(disp, win, KeyPressMask | ButtonPressMask | ButtonReleaseMask |
                ButtonMotionMask | PointerMotionMask);

   props_win_set_proto_quit(win);

   imlib_context_set_display(disp);
   imlib_context_set_visual(DefaultVisual(disp, DefaultScreen(disp)));
   imlib_context_set_colormap(DefaultColormap(disp, DefaultScreen(disp)));
   imlib_context_set_progress_function(progress);
   imlib_context_set_progress_granularity(opt_progress_granularity);
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
        im = load_image(no, argv[no]);
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
        Imlib_Image        *im2;
        int                 no2;

        XFlush(disp);
        XNextEvent(disp, &ev);
        switch (ev.type)
          {
          default:
             break;

          case ClientMessage:
             if (props_clientmsg_check_quit(&ev.xclient))
                goto quit;
             break;
          case KeyPress:
             while (XCheckTypedWindowEvent(disp, win, KeyPress, &ev))
                ;
             key = XLookupKeysym(&ev.xkey, 0);
             if (key == XK_q || key == XK_Escape)
                goto quit;
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
             if (b == 3)
                zoom_mode = 0;
             if (b == 1)
                goto show_next;
             if (b == 2)
                goto show_prev;
             break;
          case MotionNotify:
             while (XCheckTypedWindowEvent(disp, win, MotionNotify, &ev))
                ;
             x = ev.xmotion.x;
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
             for (no2 = no;;)
               {
                  no2 += inc;
                  if (no2 >= argc)
                    {
                       inc = -1;
                       continue;
                    }
                  else if (no2 < 0)
                    {
                       inc = 1;
                       continue;
                    }
                  if (no2 == no)
                     break;
                  im2 = load_image(no2, argv[no2]);
                  if (!im2)
                    {
                       Vprintf("*** Error loading image: %s\n", argv[no2]);
                       continue;
                    }
                  zoom = 1.0;
                  zoom_mode = 0;
                  imlib_context_set_image(im);
                  if (!opt_cache)
                     imlib_free_image_and_decache();
                  no = no2;
                  im = im2;
                  imlib_context_set_image(im);
                  break;
               }
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
        else if (count == 0)
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
          }
     }

 quit:
   return 0;
}
