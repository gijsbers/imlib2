#include "config.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>

#include "Imlib2.h"

Display            *disp;
Window              win;

int
main(int argc, char **argv)
{
   int                 w, h;
   Imlib_Image         im_bg = NULL;
   XEvent              ev;
   KeySym              keysym;
   ImlibPolygon        poly, poly1, poly2;

   /**
    * First tests to determine which rendering task to perform
    */
   disp = XOpenDisplay(NULL);
   if (!disp)
     {
        fprintf(stderr, "Cannot open display\n");
        return 1;
     }

   win = XCreateSimpleWindow(disp, DefaultRootWindow(disp), 0, 0, 100, 100,
                             0, 0, 0);
   XSelectInput(disp, win,
                ButtonPressMask | ButtonReleaseMask | ButtonMotionMask |
                PointerMotionMask | ExposureMask | KeyPressMask);

   /**
    * Start rendering
    */
   imlib_context_set_display(disp);
   imlib_context_set_visual(DefaultVisual(disp, DefaultScreen(disp)));
   imlib_context_set_colormap(DefaultColormap(disp, DefaultScreen(disp)));
   imlib_context_set_drawable(win);
   imlib_context_set_blend(0);
   imlib_context_set_color_modifier(NULL);
   imlib_context_set_blend(0);

   im_bg = imlib_create_image(400, 400);
   imlib_context_set_image(im_bg);
   w = imlib_image_get_width();
   h = imlib_image_get_height();
   imlib_context_set_color(0, 0, 0, 255);
   imlib_image_fill_rectangle(0, 0, w, h);

   XResizeWindow(disp, win, w, h);
   XMapWindow(disp, win);
   XSync(disp, False);

   poly = imlib_polygon_new();
   imlib_polygon_add_point(poly, 20, 20);
   imlib_polygon_add_point(poly, 70, 20);
   imlib_polygon_add_point(poly, 70, 70);
   imlib_polygon_add_point(poly, 20, 70);

   poly1 = imlib_polygon_new();
   imlib_polygon_add_point(poly1, 100, 20);
   imlib_polygon_add_point(poly1, 190, 100);
   imlib_polygon_add_point(poly1, 120, 70);

   poly2 = imlib_polygon_new();
   imlib_polygon_add_point(poly2, 290, 20);
   imlib_polygon_add_point(poly2, 200, 100);
   imlib_polygon_add_point(poly2, 270, 70);

   while (1)
     {
        do
          {
             XNextEvent(disp, &ev);
             switch (ev.type)
               {
               case KeyPress:
                  keysym = XLookupKeysym(&ev.xkey, 0);
                  switch (keysym)
                    {
                    case ' ':
                       imlib_context_set_anti_alias
                          (!imlib_context_get_anti_alias());
                       printf("AA is %s\n",
                              imlib_context_get_anti_alias()? "on" : "off");
                       break;
                    case XK_q:
                    case XK_Escape:
                       goto quit;
                    default:
                       break;
                    }
                  break;
               case ButtonRelease:
                  goto quit;
               default:
                  break;

               }
          }
        while (XPending(disp));

        imlib_context_set_image(im_bg);
        imlib_context_set_color(0, 0, 0, 255);
        imlib_image_fill_rectangle(0, 0, w, h);
        imlib_context_set_color(255, 255, 255, 255);
        imlib_image_fill_polygon(poly);
        imlib_image_fill_polygon(poly1);
        imlib_image_fill_polygon(poly2);
        imlib_render_image_on_drawable(0, 0);
     }

 quit:
   imlib_polygon_free(poly);
   imlib_polygon_free(poly1);
   imlib_polygon_free(poly2);

   return 0;
}
