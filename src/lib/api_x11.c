#include "config.h"
#include <Imlib2.h>
#include "common.h"

#include <stdio.h>

#include "api.h"
#include "blend.h"
#include "image.h"
#include "updates.h"

#include "x11_color.h"
#include "x11_grab.h"
#include "x11_pixmap.h"
#include "x11_rend.h"
#include "x11_ximage.h"

EAPI void
imlib_context_set_display(Display * display)
{
   ctx->x11.dpy = display;
}

EAPI Display       *
imlib_context_get_display(void)
{
   return ctx->x11.dpy;
}

EAPI void
imlib_context_disconnect_display(void)
{
   if (!ctx->x11.dpy)
      return;
   __imlib_RenderDisconnect(ctx->x11.dpy);
   ctx->x11.dpy = NULL;
}

EAPI void
imlib_context_set_visual(Visual * visual)
{
   ctx->x11.vis = visual;
   ctx->x11.depth = imlib_get_visual_depth(ctx->x11.dpy, ctx->x11.vis);
}

EAPI Visual        *
imlib_context_get_visual(void)
{
   return ctx->x11.vis;
}

EAPI void
imlib_context_set_colormap(Colormap colormap)
{
   ctx->x11.cmap = colormap;
}

EAPI                Colormap
imlib_context_get_colormap(void)
{
   return ctx->x11.cmap;
}

EAPI void
imlib_context_set_drawable(Drawable drawable)
{
   ctx->drawable = drawable;
}

EAPI                Drawable
imlib_context_get_drawable(void)
{
   return ctx->drawable;
}

EAPI void
imlib_context_set_mask(Pixmap mask)
{
   ctx->mask = mask;
}

EAPI                Pixmap
imlib_context_get_mask(void)
{
   return ctx->mask;
}

EAPI void
imlib_context_set_dither_mask(char dither_mask)
{
   ctx->dither_mask = dither_mask;
}

EAPI char
imlib_context_get_dither_mask(void)
{
   return ctx->dither_mask;
}

EAPI void
imlib_context_set_mask_alpha_threshold(int mask_alpha_threshold)
{
   ctx->mask_alpha_threshold = mask_alpha_threshold;
}

EAPI int
imlib_context_get_mask_alpha_threshold(void)
{
   return ctx->mask_alpha_threshold;
}

EAPI int
imlib_get_ximage_cache_count_used(void)
{
   return __imlib_GetXImageCacheCountUsed(&ctx->x11);
}

EAPI int
imlib_get_ximage_cache_count_max(void)
{
   return __imlib_GetXImageCacheCountMax(&ctx->x11);
}

EAPI void
imlib_set_ximage_cache_count_max(int count)
{
   __imlib_SetXImageCacheCountMax(&ctx->x11, count);
}

EAPI int
imlib_get_ximage_cache_size_used(void)
{
   return __imlib_GetXImageCacheSizeUsed(&ctx->x11);
}

EAPI int
imlib_get_ximage_cache_size_max(void)
{
   return __imlib_GetXImageCacheSizeMax(&ctx->x11);
}

EAPI void
imlib_set_ximage_cache_size_max(int bytes)
{
   __imlib_SetXImageCacheSizeMax(&ctx->x11, bytes);
}

EAPI int
imlib_get_color_usage(void)
{
   return (int)_max_colors;
}

EAPI void
imlib_set_color_usage(int max)
{
   if (max < 2)
      max = 2;
   else if (max > 256)
      max = 256;
   _max_colors = max;
}

EAPI int
imlib_get_visual_depth(Display * display, Visual * visual)
{
   CHECK_PARAM_POINTER_RETURN("display", display, 0);
   CHECK_PARAM_POINTER_RETURN("visual", visual, 0);
   return __imlib_XActualDepth(display, visual);
}

EAPI Visual        *
imlib_get_best_visual(Display * display, int screen, int *depth_return)
{
   CHECK_PARAM_POINTER_RETURN("display", display, NULL);
   CHECK_PARAM_POINTER_RETURN("depth_return", depth_return, NULL);
   return __imlib_BestVisual(display, screen, depth_return);
}

EAPI void
imlib_render_pixmaps_for_whole_image(Pixmap * pixmap_return,
                                     Pixmap * mask_return)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("pixmap_return", pixmap_return);
   CAST_IMAGE(im, ctx->image);
   ctx->error = __imlib_LoadImageData(im);
   if (ctx->error)
      return;
   __imlib_CreatePixmapsForImage(&ctx->x11, ctx->drawable, im, pixmap_return,
                                 mask_return, 0, 0, im->w, im->h, im->w, im->h,
                                 0, ctx->dither, ctx->dither_mask,
                                 ctx->mask_alpha_threshold,
                                 ctx->color_modifier);
}

EAPI void
imlib_render_pixmaps_for_whole_image_at_size(Pixmap * pixmap_return,
                                             Pixmap * mask_return, int width,
                                             int height)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("pixmap_return", pixmap_return);
   CAST_IMAGE(im, ctx->image);
   ctx->error = __imlib_LoadImageData(im);
   if (ctx->error)
      return;
   __imlib_CreatePixmapsForImage(&ctx->x11, ctx->drawable, im, pixmap_return,
                                 mask_return, 0, 0, im->w, im->h, width,
                                 height, ctx->anti_alias, ctx->dither,
                                 ctx->dither_mask, ctx->mask_alpha_threshold,
                                 ctx->color_modifier);
}

EAPI void
imlib_free_pixmap_and_mask(Pixmap pixmap)
{
   __imlib_FreePixmap(ctx->x11.dpy, pixmap);
}

EAPI void
imlib_render_image_on_drawable(int x, int y)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   ctx->error = __imlib_LoadImageData(im);
   if (ctx->error)
      return;
   __imlib_RenderImage(&ctx->x11, im, ctx->drawable, ctx->mask,
                       0, 0, im->w, im->h, x, y, im->w, im->h,
                       0, ctx->dither, ctx->blend,
                       ctx->dither_mask, ctx->mask_alpha_threshold,
                       ctx->color_modifier, ctx->operation);
}

EAPI void
imlib_render_image_on_drawable_at_size(int x, int y, int width, int height)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   ctx->error = __imlib_LoadImageData(im);
   if (ctx->error)
      return;
   __imlib_RenderImage(&ctx->x11, im, ctx->drawable, ctx->mask,
                       0, 0, im->w, im->h, x, y, width, height,
                       ctx->anti_alias, ctx->dither, ctx->blend,
                       ctx->dither_mask, ctx->mask_alpha_threshold,
                       ctx->color_modifier, ctx->operation);
}

EAPI void
imlib_render_image_part_on_drawable_at_size(int src_x, int src_y,
                                            int src_width, int src_height,
                                            int dst_x, int dst_y,
                                            int dst_width, int dst_height)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   ctx->error = __imlib_LoadImageData(im);
   if (ctx->error)
      return;
   __imlib_RenderImage(&ctx->x11, im, ctx->drawable, 0,
                       src_x, src_y, src_width, src_height,
                       dst_x, dst_y, dst_width, dst_height,
                       ctx->anti_alias, ctx->dither, ctx->blend, 0, 0,
                       ctx->color_modifier, ctx->operation);
}

EAPI                uint32_t
imlib_render_get_pixel_color(void)
{
   return __imlib_RenderGetPixel(&ctx->x11, ctx->drawable,
                                 (uint8_t) ctx->color.red,
                                 (uint8_t) ctx->color.green,
                                 (uint8_t) ctx->color.blue);
}

EAPI                Imlib_Image
imlib_create_image_from_drawable(Pixmap mask, int x, int y, int width,
                                 int height, char need_to_grab_x)
{
   ImlibImage         *im;
   int                 err;
   char                domask = 0;

   if (mask)
     {
        domask = 1;
        if (mask == (Pixmap) 1)
           mask = None;
     }

   im = __imlib_CreateImage(width, height, NULL, 0);
   if (!im)
      return NULL;

   err = __imlib_GrabDrawableToRGBA(&ctx->x11, im->data, 0, 0, width, height,
                                    ctx->drawable, mask,
                                    x, y, width, height,
                                    &domask, need_to_grab_x, true, NULL);
   if (err)
     {
        __imlib_FreeImage(im);
        return NULL;
     }

   im->has_alpha = domask;

   return im;
}

EAPI                Imlib_Image
imlib_create_image_from_ximage(XImage * image, XImage * mask, int x, int y,
                               int width, int height, char need_to_grab_x)
{
   ImlibImage         *im;

   im = __imlib_CreateImage(width, height, NULL, 0);
   if (!im)
      return NULL;

   __imlib_GrabXImageToRGBA(&ctx->x11, im->data, 0, 0, width, height,
                            image, mask, x, y, width, height, need_to_grab_x);
   return im;
}

EAPI                Imlib_Image
imlib_create_scaled_image_from_drawable(Pixmap mask, int src_x, int src_y,
                                        int src_width, int src_height,
                                        int dst_width, int dst_height,
                                        char need_to_grab_x,
                                        char get_mask_from_shape)
{
   ImlibImage         *im;
   int                 err;
   char                domask;

   if (!IMAGE_DIMENSIONS_OK(src_width, src_height))
      return NULL;

   im = __imlib_CreateImage(dst_width, dst_height, NULL, 0);
   if (!im)
      return NULL;

   domask = mask != 0 || get_mask_from_shape;

   if (src_width == dst_width && src_height == dst_height)
      err = __imlib_GrabDrawableToRGBA(&ctx->x11, im->data,
                                       0, 0, dst_width, dst_height,
                                       ctx->drawable, mask,
                                       src_x, src_y, src_width, src_height,
                                       &domask, need_to_grab_x, true, NULL);
   else
      err = __imlib_GrabDrawableScaledToRGBA(&ctx->x11, im->data,
                                             0, 0, dst_width, dst_height,
                                             ctx->drawable, mask,
                                             src_x, src_y,
                                             src_width, src_height,
                                             &domask, need_to_grab_x, true);
   if (err)
     {
        __imlib_FreeImage(im);
        return NULL;
     }

   im->has_alpha = domask;

   return im;
}

EAPI char
imlib_copy_drawable_to_image(Pixmap mask, int src_x, int src_y, int src_width,
                             int src_height, int dst_x, int dst_y,
                             char need_to_grab_x)
{
   ImlibImage         *im;
   char                domask = 0;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, 0);
   if (mask)
     {
        domask = 1;
        if (mask == (Pixmap) 1)
           mask = None;
     }
   CAST_IMAGE(im, ctx->image);

   ctx->error = __imlib_LoadImageData(im);
   if (ctx->error)
      return 0;

   __imlib_DirtyImage(im);

   return !__imlib_GrabDrawableToRGBA(&ctx->x11, im->data,
                                      dst_x, dst_y, im->w, im->h,
                                      ctx->drawable, mask,
                                      src_x, src_y, src_width, src_height,
                                      &domask, need_to_grab_x, false, NULL);
}

EAPI void
imlib_render_image_updates_on_drawable(Imlib_Updates updates, int x, int y)
{
   ImlibUpdate        *u;
   ImlibImage         *im;
   int                 ximcs;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   u = (ImlibUpdate *) updates;
   if (!updates)
      return;
   ctx->error = __imlib_LoadImageData(im);
   if (ctx->error)
      return;
   ximcs = __imlib_GetXImageCacheCountMax(&ctx->x11);   /* Save */
   if (ximcs == 0)              /* Only if we don't set this up elsewhere */
      __imlib_SetXImageCacheCountMax(&ctx->x11, 10);
   for (; u; u = u->next)
     {
        __imlib_RenderImage(&ctx->x11, im, ctx->drawable, 0,
                            u->x, u->y, u->w, u->h,
                            x + u->x, y + u->y, u->w, u->h, 0, ctx->dither, 0,
                            0, 0, ctx->color_modifier, OP_COPY);
     }
   if (ximcs == 0)
      __imlib_SetXImageCacheCountMax(&ctx->x11, ximcs);
}

EAPI void
imlib_render_image_on_drawable_skewed(int src_x, int src_y,
                                      int src_width, int src_height,
                                      int dst_x, int dst_y,
                                      int h_angle_x, int h_angle_y,
                                      int v_angle_x, int v_angle_y)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   ctx->error = __imlib_LoadImageData(im);
   if (ctx->error)
      return;
   __imlib_RenderImageSkewed(&ctx->x11, im, ctx->drawable, ctx->mask,
                             src_x, src_y, src_width, src_height,
                             dst_x, dst_y, h_angle_x, h_angle_y, v_angle_x,
                             v_angle_y, ctx->anti_alias, ctx->dither,
                             ctx->blend, ctx->dither_mask,
                             ctx->mask_alpha_threshold, ctx->color_modifier,
                             ctx->operation);
}

EAPI void
imlib_render_image_on_drawable_at_angle(int src_x, int src_y,
                                        int src_width, int src_height,
                                        int dst_x, int dst_y,
                                        int angle_x, int angle_y)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   ctx->error = __imlib_LoadImageData(im);
   if (ctx->error)
      return;
   __imlib_RenderImageSkewed(&ctx->x11, im, ctx->drawable, ctx->mask,
                             src_x, src_y, src_width, src_height,
                             dst_x, dst_y, angle_x, angle_y,
                             0, 0, ctx->anti_alias, ctx->dither, ctx->blend,
                             ctx->dither_mask, ctx->mask_alpha_threshold,
                             ctx->color_modifier, ctx->operation);
}
