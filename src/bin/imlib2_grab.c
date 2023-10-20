#include "config.h"

#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "Imlib2.h"

Display            *disp;
int                 image_width = 0, image_height = 0;

static void
usage(void)
{
   printf
      ("Usage: imlib2_grab [-v] [-id <drawable id>] [-width <width>] [-height <height>] [-noshape] <output file>\n");
}

int
main(int argc, char **argv)
{
   char               *s;
   Imlib_Image        *im = NULL;
   char               *file = NULL;
   int                 verbose;
   int                 get_alpha;
   const char         *display_name = getenv("DISPLAY");
   Drawable            draw;
   int                 x, y;
   unsigned int        w, h, bw;
   unsigned int        wo, ho;
   unsigned int        depth;
   Window              rr;

   verbose = 0;
   get_alpha = 1;
   draw = None;
   wo = ho = 0;

   for (;;)
     {
        argc--;
        argv++;
        if (argc <= 0)
           break;
        s = argv[0];
        if (*s++ != '-')
           break;
        if (!strcmp(s, "id"))
          {
             argc--;
             argv++;
             if (argc <= 1)
                break;
             draw = strtoul(argv[0], NULL, 0);
          }
        else if (!strcmp(s, "w") || !strcmp(s, "width"))
          {
             argc--;
             argv++;
             if (argc <= 1)
                break;
             wo = strtoul(argv[0], NULL, 0);
          }
        else if (!strcmp(s, "h") || !strcmp(s, "height"))
          {
             argc--;
             argv++;
             if (argc <= 1)
                break;
             ho = strtoul(argv[0], NULL, 0);
          }
        else if (!strcmp(s, "noshape"))
          {
             get_alpha = 0;
          }
        else if (!strcmp(s, "v"))
          {
             verbose = 1;
          }
        else if (!strcmp(s, "help"))
          {
             usage();
             return 0;
          }
     }
   if (argc <= 0)
     {
        usage();
        return 1;
     }

   file = argv[0];

   if (!display_name)
      display_name = ":0";
   disp = XOpenDisplay(display_name);
   if (!disp)
     {
        fprintf(stderr, "Can't open display %s\n", display_name);
        return 1;
     }

   imlib_context_set_display(disp);
   imlib_context_set_visual(DefaultVisual(disp, DefaultScreen(disp)));
   imlib_context_set_colormap(DefaultColormap(disp, DefaultScreen(disp)));

   if (draw == None)
      draw = DefaultRootWindow(disp);
   imlib_context_set_drawable(draw);

   XGetGeometry(disp, draw, &rr, &x, &y, &w, &h, &bw, &depth);
   if (wo == 0)
      wo = w;
   if (ho == 0)
      ho = h;
   if (verbose)
     {
        printf("Drawable: %#lx: x,y: %d,%d  wxh=%ux%u  bw=%u  depth=%u\n",
               draw, x, y, w, h, bw, depth);
        if ((wo != w) || (ho != h))
           printf("Output  : wxh=%ux%u\n", wo, ho);
     }

   im = imlib_create_scaled_image_from_drawable(None, 0, 0, w, h, wo, ho, 1,
                                                (get_alpha) ? 1 : 0);
   if (!im)
     {
        fprintf(stderr, "Cannot grab image!\n");
        exit(0);
     }

   imlib_context_set_image(im);
   imlib_save_image(file);

   return 0;
}
