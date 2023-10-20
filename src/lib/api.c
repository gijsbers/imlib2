#include "config.h"
#include <Imlib2.h>
#include "common.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "blend.h"
#include "colormod.h"
#include "color_helpers.h"
#include "dynamic_filters.h"
#include "file.h"
#include "filter.h"
#include "font.h"
#include "grad.h"
#include "image.h"
#include "loaders.h"
#include "rgbadraw.h"
#include "rotate.h"
#include "scale.h"
#include "script.h"
#include "updates.h"
#ifdef BUILD_X11
#include "x11_color.h"
#include "x11_grab.h"
#include "x11_pixmap.h"
#include "x11_rend.h"
#include "x11_ximage.h"
#endif

/* convenience macros */
#define   CAST_IMAGE(im, image) (im) = (ImlibImage *)(image)
#define   CHECK_PARAM_POINTER_RETURN(sparam, param, ret) \
if (!(param)) \
{ \
  fprintf(stderr, "***** Imlib2 Developer Warning ***** :\n" \
                  "\tThis program is calling the Imlib call:\n\n" \
                  "\t%s();\n\n" \
                  "\tWith the parameter:\n\n" \
                  "\t%s\n\n" \
                  "\tbeing NULL. Please fix your program.\n", __func__, sparam); \
  return ret; \
}

#define   CHECK_PARAM_POINTER(sparam, param) \
if (!(param)) \
{ \
  fprintf(stderr, "***** Imlib2 Developer Warning ***** :\n" \
                  "\tThis program is calling the Imlib call:\n\n" \
                  "\t%s();\n\n" \
                  "\tWith the parameter:\n\n" \
                  "\t%s\n\n" \
                  "\tbeing NULL. Please fix your program.\n", __func__, sparam); \
  return; \
}

#define ILA0(ctx, imm, noc) \
   .pfunc = (ImlibProgressFunction)(ctx)->progress_func, \
   .pgran = (ctx)->progress_granularity, \
   .immed = imm, .nocache = noc

/* internal typedefs for function pointers */
typedef void        (*Imlib_Internal_Progress_Function)(void *, char, int, int,
                                                        int, int);
typedef void        (*Imlib_Internal_Data_Destructor_Function)(void *, void *);

typedef struct {
#ifdef BUILD_X11
   Display            *display;
   Visual             *visual;
   Colormap            colormap;
   int                 depth;
   Drawable            drawable;
   Pixmap              mask;
#endif
   char                anti_alias;
   char                dither;
   char                blend;
   Imlib_Color_Modifier color_modifier;
   ImlibOp             operation;
   Imlib_Font          font;
   Imlib_Text_Direction direction;
   double              angle;
   Imlib_Color         color;
   uint32_t            pixel;
   Imlib_Color_Range   color_range;
   Imlib_Image         image;
   Imlib_Image_Data_Memory_Function image_data_memory_func;
   Imlib_Progress_Function progress_func;
   char                progress_granularity;
   char                dither_mask;
   int                 mask_alpha_threshold;
   Imlib_Filter        filter;
   Imlib_Rectangle     cliprect;

   int                 references;
   char                dirty;
} ImlibContext;

typedef struct _ImlibContextItem {
   ImlibContext       *context;
   struct _ImlibContextItem *below;
} ImlibContextItem;

#define DefaultContext { \
   .anti_alias = 1,				\
   .dither = 0,					\
   .blend = 1,					\
   .operation = (ImlibOp) IMLIB_OP_COPY,	\
   .direction = IMLIB_TEXT_TO_RIGHT,		\
   .angle = 0.0,				\
   .color.alpha = 255,				\
   .color.red = 255,				\
   .color.green = 255,				\
   .color.blue = 255,				\
   .pixel = 0xffffffff,				\
   .mask_alpha_threshold = 128,			\
}

/* A default context, only used for initialization */
static const ImlibContext ctx_default = DefaultContext;

/* The initial context */
static ImlibContext ctx0 = DefaultContext;

/* Current context */
static ImlibContext *ctx = &ctx0;

/* a stack of contexts -- only used by context-handling functions. */
static ImlibContextItem contexts0 = {.context = &ctx0 };
static ImlibContextItem *contexts = &contexts0;

/* Return Imlib2 version */
int
imlib_version(void)
{
   return IMLIB2_VERSION;
}

static              Imlib_Load_Error
__imlib_ErrorFromErrno(int err, int save)
{
   switch (err)
     {
     default:
        return IMLIB_LOAD_ERROR_UNKNOWN;
     case 0:
        return IMLIB_LOAD_ERROR_NONE;
     case IMLIB_ERR_NO_LOADER:
     case IMLIB_ERR_NO_SAVER:
        return IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT;
     case IMLIB_ERR_BAD_IMAGE:
        return IMLIB_LOAD_ERROR_IMAGE_READ;
     case IMLIB_ERR_BAD_FRAME:
        return IMLIB_LOAD_ERROR_IMAGE_FRAME;
     case ENOENT:
        return IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST;
     case EISDIR:
        return IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY;
     case EACCES:
     case EROFS:
        return (save) ? IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_WRITE :
           IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ;
     case ENAMETOOLONG:
        return IMLIB_LOAD_ERROR_PATH_TOO_LONG;
     case ENOTDIR:
        return IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY;
     case EFAULT:
        return IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE;
     case ELOOP:
        return IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS;
     case ENOMEM:
        return IMLIB_LOAD_ERROR_OUT_OF_MEMORY;
     case EMFILE:
        return IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS;
     case ENOSPC:
        return IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE;
     }
}

/* frees the given context including all its members */
static void
__imlib_free_context(ImlibContext * context)
{
   ImlibContextItem   *next = contexts;

   if (ctx == context)
     {
        next = contexts->below;
        free(contexts);
        contexts = next;
     }

   ctx = context;

   if (ctx->image)
     {
        imlib_free_image();
        ctx->image = NULL;
     }
   if (ctx->font)
     {
        imlib_free_font();
        ctx->font = NULL;
     }
   if (ctx->color_modifier)
     {
        imlib_free_color_modifier();
        ctx->color_modifier = NULL;
     }
   if (ctx->filter)
     {
        imlib_free_filter();
        ctx->filter = NULL;
     }

   free(ctx);
   ctx = next->context;
}

EAPI                Imlib_Context
imlib_context_new(void)
{
   ImlibContext       *context = malloc(sizeof(ImlibContext));

   if (context)
      *context = ctx_default;

   return context;
}

/* frees the given context if it doesn't have any reference anymore. The
   last (default) context can never be freed.
   If context is the current context, the context below will be made the
   current context.
*/
EAPI void
imlib_context_free(Imlib_Context context)
{
   ImlibContext       *c = (ImlibContext *) context;

   CHECK_PARAM_POINTER("context", context);
   if (c == ctx && !contexts->below)
      return;

   if (c->references == 0)
      __imlib_free_context(c);
   else
      c->dirty = 1;
}

EAPI void
imlib_context_push(Imlib_Context context)
{
   ImlibContextItem   *item;

   CHECK_PARAM_POINTER("context", context);
   ctx = (ImlibContext *) context;

   item = malloc(sizeof(ImlibContextItem));
   item->context = ctx;
   item->below = contexts;
   contexts = item;

   ctx->references++;
}

EAPI void
imlib_context_pop(void)
{
   ImlibContextItem   *item = contexts;
   ImlibContext       *current_ctx = item->context;

   if (!item->below)
      return;

   contexts = item->below;
   ctx = contexts->context;
   current_ctx->references--;
   if (current_ctx->dirty && current_ctx->references <= 0)
      __imlib_free_context(current_ctx);

   free(item);
}

EAPI                Imlib_Context
imlib_context_get(void)
{
   return ctx;
}

/* context setting/getting functions */

EAPI void
imlib_context_set_cliprect(int x, int y, int w, int h)
{
   ctx->cliprect.x = x;
   ctx->cliprect.y = y;
   ctx->cliprect.w = w;
   ctx->cliprect.h = h;
}

EAPI void
imlib_context_get_cliprect(int *x, int *y, int *w, int *h)
{
   *x = ctx->cliprect.x;
   *y = ctx->cliprect.y;
   *w = ctx->cliprect.w;
   *h = ctx->cliprect.h;
}

#ifdef BUILD_X11
EAPI void
imlib_context_set_display(Display * display)
{
   ctx->display = display;
}

EAPI Display       *
imlib_context_get_display(void)
{
   return ctx->display;
}

EAPI void
imlib_context_disconnect_display(void)
{
   if (!ctx->display)
      return;
   __imlib_RenderDisconnect(ctx->display);
   ctx->display = NULL;
}

EAPI void
imlib_context_set_visual(Visual * visual)
{
   ctx->visual = visual;
   ctx->depth = imlib_get_visual_depth(ctx->display, ctx->visual);
}

EAPI Visual        *
imlib_context_get_visual(void)
{
   return ctx->visual;
}

EAPI void
imlib_context_set_colormap(Colormap colormap)
{
   ctx->colormap = colormap;
}

EAPI                Colormap
imlib_context_get_colormap(void)
{
   return ctx->colormap;
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
   return __imlib_GetXImageCacheCountUsed(ctx->display);
}

EAPI int
imlib_get_ximage_cache_count_max(void)
{
   return __imlib_GetXImageCacheCountMax(ctx->display);
}

EAPI void
imlib_set_ximage_cache_count_max(int count)
{
   __imlib_SetXImageCacheCountMax(ctx->display, count);
}

EAPI int
imlib_get_ximage_cache_size_used(void)
{
   return __imlib_GetXImageCacheSizeUsed(ctx->display);
}

EAPI int
imlib_get_ximage_cache_size_max(void)
{
   return __imlib_GetXImageCacheSizeMax(ctx->display);
}

EAPI void
imlib_set_ximage_cache_size_max(int bytes)
{
   __imlib_SetXImageCacheSizeMax(ctx->display, bytes);
}
#endif

EAPI void
imlib_context_set_anti_alias(char anti_alias)
{
   ctx->anti_alias = anti_alias;
}

EAPI char
imlib_context_get_anti_alias(void)
{
   return ctx->anti_alias;
}

EAPI void
imlib_context_set_dither(char dither)
{
   ctx->dither = dither;
}

EAPI char
imlib_context_get_dither(void)
{
   return ctx->dither;
}

EAPI void
imlib_context_set_blend(char blend)
{
   ctx->blend = blend;
}

EAPI char
imlib_context_get_blend(void)
{
   return ctx->blend;
}

EAPI void
imlib_context_set_color_modifier(Imlib_Color_Modifier color_modifier)
{
   ctx->color_modifier = color_modifier;
}

EAPI                Imlib_Color_Modifier
imlib_context_get_color_modifier(void)
{
   return ctx->color_modifier;
}

EAPI void
imlib_context_set_operation(Imlib_Operation operation)
{
   ctx->operation = (ImlibOp) operation;
}

EAPI                Imlib_Operation
imlib_context_get_operation(void)
{
   return (Imlib_Operation) ctx->operation;
}

EAPI void
imlib_context_set_font(Imlib_Font font)
{
   ctx->font = font;
}

EAPI                Imlib_Font
imlib_context_get_font(void)
{
   return ctx->font;
}

EAPI void
imlib_context_set_direction(Imlib_Text_Direction direction)
{
   ctx->direction = direction;
}

EAPI void
imlib_context_set_angle(double angle)
{
   ctx->angle = angle;
}

EAPI double
imlib_context_get_angle(void)
{
   return ctx->angle;
}

EAPI                Imlib_Text_Direction
imlib_context_get_direction(void)
{
   return ctx->direction;
}

EAPI void
imlib_context_set_color(int red, int green, int blue, int alpha)
{
   uint8_t             r, g, b, a;

   r = red;
   g = green;
   b = blue;
   a = alpha;

   ctx->color.red = r;
   ctx->color.green = g;
   ctx->color.blue = b;
   ctx->color.alpha = a;

   ctx->pixel = PIXEL_ARGB(a, r, g, b);
}

EAPI void
imlib_context_get_color(int *red, int *green, int *blue, int *alpha)
{
   *red = ctx->color.red;
   *green = ctx->color.green;
   *blue = ctx->color.blue;
   *alpha = ctx->color.alpha;
}

EAPI Imlib_Color   *
imlib_context_get_imlib_color(void)
{
   return &ctx->color;
}

EAPI void
imlib_context_set_color_hsva(float hue, float saturation, float value,
                             int alpha)
{
   int                 r, g, b;

   __imlib_hsv_to_rgb(hue, saturation, value, &r, &g, &b);
   imlib_context_set_color(r, g, b, alpha);
}

EAPI void
imlib_context_get_color_hsva(float *hue, float *saturation, float *value,
                             int *alpha)
{
   int                 r, g, b;

   imlib_context_get_color(&r, &g, &b, alpha);
   __imlib_rgb_to_hsv(r, g, b, hue, saturation, value);
}

EAPI void
imlib_context_set_color_hlsa(float hue, float lightness, float saturation,
                             int alpha)
{
   int                 r, g, b;

   __imlib_hls_to_rgb(hue, lightness, saturation, &r, &g, &b);
   imlib_context_set_color(r, g, b, alpha);
}

EAPI void
imlib_context_get_color_hlsa(float *hue, float *lightness, float *saturation,
                             int *alpha)
{
   int                 r, g, b;

   imlib_context_get_color(&r, &g, &b, alpha);
   __imlib_rgb_to_hls(r, g, b, hue, lightness, saturation);
}

EAPI void
imlib_context_set_color_cmya(int cyan, int magenta, int yellow, int alpha)
{
   uint8_t             r, g, b, a;

   r = 255 - cyan;
   g = 255 - magenta;
   b = 255 - yellow;
   a = alpha;

   ctx->color.red = r;
   ctx->color.green = g;
   ctx->color.blue = b;
   ctx->color.alpha = a;

   ctx->pixel = PIXEL_ARGB(a, r, g, b);
}

EAPI void
imlib_context_get_color_cmya(int *cyan, int *magenta, int *yellow, int *alpha)
{
   *cyan = 255 - ctx->color.red;
   *magenta = 255 - ctx->color.green;
   *yellow = 255 - ctx->color.blue;
   *alpha = ctx->color.alpha;
}

EAPI void
imlib_context_set_color_range(Imlib_Color_Range color_range)
{
   ctx->color_range = color_range;
}

EAPI                Imlib_Color_Range
imlib_context_get_color_range(void)
{
   return ctx->color_range;
}

EAPI void
imlib_context_set_image_data_memory_function(Imlib_Image_Data_Memory_Function
                                             memory_function)
{
   ctx->image_data_memory_func = memory_function;
}

EAPI void
imlib_context_set_progress_function(Imlib_Progress_Function progress_function)
{
   ctx->progress_func = progress_function;
}

EAPI                Imlib_Image_Data_Memory_Function
imlib_context_get_image_data_memory_function(void)
{
   return ctx->image_data_memory_func;
}

EAPI                Imlib_Progress_Function
imlib_context_get_progress_function(void)
{
   return ctx->progress_func;
}

EAPI void
imlib_context_set_progress_granularity(char progress_granularity)
{
   ctx->progress_granularity = progress_granularity;
}

EAPI char
imlib_context_get_progress_granularity(void)
{
   return ctx->progress_granularity;
}

EAPI void
imlib_context_set_image(Imlib_Image image)
{
   ctx->image = image;
}

EAPI                Imlib_Image
imlib_context_get_image(void)
{
   return ctx->image;
}

/* imlib api */

EAPI int
imlib_get_cache_used(void)
{
   return __imlib_CurrentCacheSize();
}

EAPI int
imlib_get_cache_size(void)
{
   return __imlib_GetCacheSize();
}

EAPI void
imlib_set_cache_size(int bytes)
{
   __imlib_SetCacheSize(bytes);
}

EAPI int
imlib_get_color_usage(void)
{
#ifdef BUILD_X11
   return (int)_max_colors;
#else
   return 256;
#endif
}

EAPI void
imlib_set_color_usage(int max)
{
#ifdef BUILD_X11
   if (max < 2)
      max = 2;
   else if (max > 256)
      max = 256;
   _max_colors = max;
#endif
}

EAPI void
imlib_flush_loaders(void)
{
   __imlib_RemoveAllLoaders();
}

#ifdef BUILD_X11
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
#endif

EAPI                Imlib_Image
imlib_load_image(const char *file)
{
   Imlib_Image         im;
   ImlibLoadArgs       ila = { ILA0(ctx, 0, 0) };

   CHECK_PARAM_POINTER_RETURN("file", file, NULL);

   im = __imlib_LoadImage(file, &ila);

   return im;
}

static              Imlib_Image
_imlib_load_image_immediately(const char *file, int *err)
{
   Imlib_Image         im;
   ImlibLoadArgs       ila = { ILA0(ctx, 1, 0) };

   CHECK_PARAM_POINTER_RETURN("file", file, NULL);

   im = __imlib_LoadImage(file, &ila);
   *err = ila.err;

   return im;
}

EAPI                Imlib_Image
imlib_load_image_immediately(const char *file)
{
   int                 err;

   return _imlib_load_image_immediately(file, &err);
}

EAPI                Imlib_Image
imlib_load_image_without_cache(const char *file)
{
   Imlib_Image         im;
   ImlibLoadArgs       ila = { ILA0(ctx, 0, 1) };

   CHECK_PARAM_POINTER_RETURN("file", file, NULL);

   im = __imlib_LoadImage(file, &ila);

   return im;
}

EAPI                Imlib_Image
imlib_load_image_immediately_without_cache(const char *file)
{
   Imlib_Image         im;
   ImlibLoadArgs       ila = { ILA0(ctx, 1, 1) };

   CHECK_PARAM_POINTER_RETURN("file", file, NULL);

   im = __imlib_LoadImage(file, &ila);

   return im;
}

EAPI                Imlib_Image
imlib_load_image_with_error_return(const char *file,
                                   Imlib_Load_Error * error_return)
{
   Imlib_Image         im;
   int                 err = 0;

   im = _imlib_load_image_immediately(file, &err);
   if (error_return)
      *error_return = __imlib_ErrorFromErrno(err, 0);

   return im;
}

EAPI                Imlib_Image
imlib_load_image_with_errno_return(const char *file, int *error_return)
{
   Imlib_Image         im;
   int                 err = 0;

   im = _imlib_load_image_immediately(file, &err);
   if (error_return)
      *error_return = err;

   return im;
}

EAPI                Imlib_Image
imlib_load_image_fd(int fd, const char *file)
{
   Imlib_Image         im;
   ImlibLoadArgs       ila = { ILA0(ctx, 1, 1) };

   CHECK_PARAM_POINTER_RETURN("file", file, NULL);

   ila.fp = fdopen(fd, "rb");
   if (ila.fp)
     {
        im = __imlib_LoadImage(file, &ila);
        fclose(ila.fp);
     }
   else
     {
        im = NULL;
        close(fd);
     }

   return im;
}

EAPI                Imlib_Image
imlib_load_image_frame(const char *file, int frame)
{
   Imlib_Image         im;
   ImlibLoadArgs       ila = { ILA0(ctx, 1, 0),.frame = frame };

   CHECK_PARAM_POINTER_RETURN("file", file, NULL);

   im = __imlib_LoadImage(file, &ila);

   return im;
}

EAPI void
imlib_image_get_frame_info(Imlib_Frame_Info * info)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);

   info->frame_count = im->frame_count;
   info->frame_num = im->frame_num;
   info->canvas_w = im->canvas_w ? im->canvas_w : im->w;
   info->canvas_h = im->canvas_h ? im->canvas_h : im->h;
   info->frame_x = im->frame_x;
   info->frame_y = im->frame_y;
   info->frame_w = im->w;
   info->frame_h = im->h;
   info->frame_flags = im->frame_flags;
   info->frame_delay = im->frame_delay ? im->frame_delay : 100;
}

EAPI void
imlib_free_image(void)
{
   CHECK_PARAM_POINTER("image", ctx->image);
   __imlib_FreeImage(ctx->image);
   ctx->image = NULL;
}

EAPI void
imlib_free_image_and_decache(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   IM_FLAG_SET(im, F_INVALID);
   __imlib_FreeImage(im);
   ctx->image = NULL;
}

EAPI int
imlib_image_get_width(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, 0);
   CAST_IMAGE(im, ctx->image);
   return im->w;
}

EAPI int
imlib_image_get_height(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, 0);
   CAST_IMAGE(im, ctx->image);
   return im->h;
}

EAPI const char    *
imlib_image_get_filename(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, 0);
   CAST_IMAGE(im, ctx->image);
   /* strdup() the returned value if you want to alter it! */
   return im->file;
}

EAPI uint32_t      *
imlib_image_get_data(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, NULL);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return NULL;
   __imlib_DirtyImage(im);
   return im->data;
}

EAPI uint32_t      *
imlib_image_get_data_for_reading_only(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, NULL);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return NULL;
   return im->data;
}

EAPI void
imlib_image_put_back_data(uint32_t * data)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("data", data);
   CAST_IMAGE(im, ctx->image);
   __imlib_DirtyImage(im);
   data = NULL;
}

EAPI char
imlib_image_has_alpha(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, 0);
   CAST_IMAGE(im, ctx->image);
   if (IM_FLAG_ISSET(im, F_HAS_ALPHA))
      return 1;
   return 0;
}

EAPI void
imlib_image_set_changes_on_disk(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   IM_FLAG_SET(im, F_ALWAYS_CHECK_DISK);
}

EAPI void
imlib_image_get_border(Imlib_Border * border)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("border", border);
   CAST_IMAGE(im, ctx->image);
   border->left = im->border.left;
   border->right = im->border.right;
   border->top = im->border.top;
   border->bottom = im->border.bottom;
}

EAPI void
imlib_image_set_border(Imlib_Border * border)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("border", border);
   CAST_IMAGE(im, ctx->image);
   if ((im->border.left == border->left)
       && (im->border.right == border->right)
       && (im->border.top == border->top)
       && (im->border.bottom == border->bottom))
      return;
   im->border.left = MAX(0, border->left);
   im->border.right = MAX(0, border->right);
   im->border.top = MAX(0, border->top);
   im->border.bottom = MAX(0, border->bottom);
#ifdef BUILD_X11
   __imlib_DirtyPixmapsForImage(im);
#endif
}

EAPI void
imlib_image_set_format(const char *format)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("format", format);
   CAST_IMAGE(im, ctx->image);
   free(im->format);
   im->format = (format) ? strdup(format) : NULL;
   if (!IM_FLAG_ISSET(im, F_FORMAT_IRRELEVANT))
     {
        __imlib_DirtyImage(im);
     }
}

EAPI void
imlib_image_set_irrelevant_format(char irrelevant)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   IM_FLAG_UPDATE(im, F_FORMAT_IRRELEVANT, irrelevant);
}

EAPI char          *
imlib_image_format(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, NULL);
   CAST_IMAGE(im, ctx->image);
   return im->format;
}

EAPI void
imlib_image_set_has_alpha(char has_alpha)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   IM_FLAG_UPDATE(im, F_HAS_ALPHA, has_alpha);
}

#ifdef BUILD_X11
EAPI void
imlib_render_pixmaps_for_whole_image(Pixmap * pixmap_return,
                                     Pixmap * mask_return)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("pixmap_return", pixmap_return);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_CreatePixmapsForImage(ctx->display, ctx->drawable, ctx->visual,
                                 ctx->depth, ctx->colormap, im, pixmap_return,
                                 mask_return, 0, 0, im->w, im->h, im->w,
                                 im->h, 0, ctx->dither, ctx->dither_mask,
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
   if (__imlib_LoadImageData(im))
      return;
   __imlib_CreatePixmapsForImage(ctx->display, ctx->drawable, ctx->visual,
                                 ctx->depth, ctx->colormap, im, pixmap_return,
                                 mask_return, 0, 0, im->w, im->h, width,
                                 height, ctx->anti_alias, ctx->dither,
                                 ctx->dither_mask, ctx->mask_alpha_threshold,
                                 ctx->color_modifier);
}

EAPI void
imlib_free_pixmap_and_mask(Pixmap pixmap)
{
   __imlib_FreePixmap(ctx->display, pixmap);
}

EAPI void
imlib_render_image_on_drawable(int x, int y)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_RenderImage(ctx->display, im, ctx->drawable, ctx->mask,
                       ctx->visual, ctx->colormap, ctx->depth, 0, 0, im->w,
                       im->h, x, y, im->w, im->h, 0, ctx->dither, ctx->blend,
                       ctx->dither_mask, ctx->mask_alpha_threshold,
                       ctx->color_modifier, ctx->operation);
}

EAPI void
imlib_render_image_on_drawable_at_size(int x, int y, int width, int height)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_RenderImage(ctx->display, im, ctx->drawable, ctx->mask,
                       ctx->visual, ctx->colormap, ctx->depth, 0, 0, im->w,
                       im->h, x, y, width, height, ctx->anti_alias,
                       ctx->dither, ctx->blend, ctx->dither_mask,
                       ctx->mask_alpha_threshold, ctx->color_modifier,
                       ctx->operation);
}

EAPI void
imlib_render_image_part_on_drawable_at_size(int source_x, int source_y,
                                            int source_width,
                                            int source_height, int x, int y,
                                            int width, int height)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_RenderImage(ctx->display, im, ctx->drawable, 0, ctx->visual,
                       ctx->colormap, ctx->depth, source_x, source_y,
                       source_width, source_height, x, y, width, height,
                       ctx->anti_alias, ctx->dither, ctx->blend, 0,
                       0, ctx->color_modifier, ctx->operation);
}

EAPI                uint32_t
imlib_render_get_pixel_color(void)
{
   return __imlib_RenderGetPixel(ctx->display, ctx->drawable, ctx->visual,
                                 ctx->colormap, ctx->depth,
                                 (uint8_t) ctx->color.red,
                                 (uint8_t) ctx->color.green,
                                 (uint8_t) ctx->color.blue);
}

#endif

EAPI void
imlib_blend_image_onto_image(Imlib_Image source_image, char merge_alpha,
                             int source_x, int source_y, int source_width,
                             int source_height, int destination_x,
                             int destination_y, int destination_width,
                             int destination_height)
{
   ImlibImage         *im_src, *im_dst;
   int                 aa;

   CHECK_PARAM_POINTER("source_image", source_image);
   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im_src, source_image);
   CAST_IMAGE(im_dst, ctx->image);
   if (__imlib_LoadImageData(im_src))
      return;
   if (__imlib_LoadImageData(im_dst))
      return;
   __imlib_DirtyImage(im_dst);
   /* FIXME: hack to get around infinite loops for scaling down too far */
   aa = ctx->anti_alias;
   if ((abs(destination_width) < (source_width >> 7))
       || (abs(destination_height) < (source_height >> 7)))
      aa = 0;
   __imlib_BlendImageToImage(im_src, im_dst, aa, ctx->blend,
                             merge_alpha, source_x, source_y, source_width,
                             source_height, destination_x, destination_y,
                             destination_width, destination_height,
                             ctx->color_modifier, ctx->operation,
                             ctx->cliprect.x, ctx->cliprect.y,
                             ctx->cliprect.w, ctx->cliprect.h);
}

EAPI                Imlib_Image
imlib_create_image(int width, int height)
{
   uint32_t           *data;

   if (!IMAGE_DIMENSIONS_OK(width, height))
      return NULL;
   data = malloc(width * height * sizeof(uint32_t));
   if (data)
      return __imlib_CreateImage(width, height, data);
   return NULL;
}

EAPI                Imlib_Image
imlib_create_image_using_data(int width, int height, uint32_t * data)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("data", data, NULL);
   if (!IMAGE_DIMENSIONS_OK(width, height))
      return NULL;
   im = __imlib_CreateImage(width, height, data);
   if (im)
      IM_FLAG_SET(im, F_DONT_FREE_DATA);
   return im;
}

EAPI                Imlib_Image
   imlib_create_image_using_data_and_memory_function
   (int width, int height, uint32_t * data,
    Imlib_Image_Data_Memory_Function func) {
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("data", data, NULL);
   if (!IMAGE_DIMENSIONS_OK(width, height))
      return NULL;
   im = __imlib_CreateImage(width, height, data);
   if (im)
      im->data_memory_func = func;

   return im;
}

EAPI                Imlib_Image
imlib_create_image_using_copied_data(int width, int height, uint32_t * data)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("data", data, NULL);
   if (!IMAGE_DIMENSIONS_OK(width, height))
      return NULL;
   im = __imlib_CreateImage(width, height, NULL);
   if (!im)
      return NULL;
   im->data = malloc(width * height * sizeof(uint32_t));
   if (data)
     {
        memcpy(im->data, data, width * height * sizeof(uint32_t));
        return im;
     }
   else
      __imlib_FreeImage(im);
   return NULL;
}

#ifdef BUILD_X11
EAPI                Imlib_Image
imlib_create_image_from_drawable(Pixmap mask, int x, int y, int width,
                                 int height, char need_to_grab_x)
{
   ImlibImage         *im;
   char                domask = 0;

   if (!IMAGE_DIMENSIONS_OK(width, height))
      return NULL;
   if (mask)
     {
        domask = 1;
        if (mask == (Pixmap) 1)
           mask = None;
     }
   im = __imlib_CreateImage(width, height, NULL);
   if (!im)
      return NULL;
   im->data = malloc(width * height * sizeof(uint32_t));
   if (im->data &&
       __imlib_GrabDrawableToRGBA(im->data, 0, 0, width, height, ctx->display,
                                  ctx->drawable, mask, ctx->visual,
                                  ctx->colormap, ctx->depth, x, y, width,
                                  height, &domask, need_to_grab_x))
     {
        IM_FLAG_UPDATE(im, F_HAS_ALPHA, domask);
     }
   else
     {
        __imlib_FreeImage(im);
        im = NULL;
     }

   return im;
}

EAPI                Imlib_Image
imlib_create_image_from_ximage(XImage * image, XImage * mask, int x, int y,
                               int width, int height, char need_to_grab_x)
{
   ImlibImage         *im;

   if (!IMAGE_DIMENSIONS_OK(width, height))
      return NULL;
   im = __imlib_CreateImage(width, height, NULL);
   if (!im)
      return NULL;
   im->data = malloc(width * height * sizeof(uint32_t));
   if (!im->data)
     {
        __imlib_FreeImage(im);
        return NULL;
     }
   __imlib_GrabXImageToRGBA(im->data, 0, 0, width, height,
                            ctx->display, image, mask, ctx->visual,
                            ctx->depth, x, y, width, height, need_to_grab_x);
   return im;
}

EAPI                Imlib_Image
imlib_create_scaled_image_from_drawable(Pixmap mask, int source_x,
                                        int source_y, int source_width,
                                        int source_height,
                                        int destination_width,
                                        int destination_height,
                                        char need_to_grab_x,
                                        char get_mask_from_shape)
{
   ImlibImage         *im;
   char                domask;

   if (!IMAGE_DIMENSIONS_OK(source_width, source_height))
      return NULL;
   if (!IMAGE_DIMENSIONS_OK(destination_width, destination_height))
      return NULL;

   domask = mask != 0 || get_mask_from_shape;

   im = __imlib_CreateImage(destination_width, destination_height, NULL);
   if (!im)
      return NULL;
   im->data = malloc(destination_width * destination_height * sizeof(uint32_t));
   if (!im->data)
     {
        __imlib_FreeImage(im);
        return NULL;
     }

   __imlib_GrabDrawableScaledToRGBA(im->data, 0, 0,
                                    destination_width, destination_height,
                                    ctx->display, ctx->drawable, mask,
                                    ctx->visual, ctx->colormap, ctx->depth,
                                    source_x, source_y,
                                    source_width, source_height,
                                    &domask, need_to_grab_x);

   IM_FLAG_UPDATE(im, F_HAS_ALPHA, domask);

   return im;
}

EAPI char
imlib_copy_drawable_to_image(Pixmap mask, int x, int y, int width, int height,
                             int destination_x, int destination_y,
                             char need_to_grab_x)
{
   ImlibImage         *im;
   char                domask = 0;
   int                 pre_adj;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, 0);
   if (mask)
     {
        domask = 1;
        if (mask == (Pixmap) 1)
           mask = None;
     }
   CAST_IMAGE(im, ctx->image);

   if (__imlib_LoadImageData(im))
      return 0;

   pre_adj = 0;
   if (x < 0)
     {
        width += x;
        pre_adj = x;
        x = 0;
     }
   if (width < 0)
      width = 0;
   if (destination_x < 0)
     {
        width += destination_x;
        x -= destination_x - pre_adj;
        destination_x = 0;
     }
   if ((destination_x + width) >= im->w)
      width = im->w - destination_x;

   pre_adj = 0;
   if (y < 0)
     {
        height += y;
        pre_adj = y;
        y = 0;
     }
   if (height < 0)
      height = 0;
   if (destination_y < 0)
     {
        height += destination_y;
        y -= destination_y - pre_adj;
        destination_y = 0;
     }
   if ((destination_y + height) >= im->h)
      height = im->h - destination_y;

   if ((width <= 0) || (height <= 0))
      return 0;
   __imlib_DirtyImage(im);
   return __imlib_GrabDrawableToRGBA(im->data, destination_x, destination_y,
                                     im->w, im->h, ctx->display,
                                     ctx->drawable, mask, ctx->visual,
                                     ctx->colormap, ctx->depth, x, y, width,
                                     height, &domask, need_to_grab_x);
}
#endif

EAPI                Imlib_Image
imlib_clone_image(void)
{
   ImlibImage         *im, *im_old;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, NULL);
   CAST_IMAGE(im_old, ctx->image);
   if (__imlib_LoadImageData(im_old))
      return NULL;
   /* Note: below check should've ensured by original image allocation,
    * but better safe than sorry. */
   if (!IMAGE_DIMENSIONS_OK(im_old->w, im_old->h))
      return NULL;
   im = __imlib_CreateImage(im_old->w, im_old->h, NULL);
   if (!(im))
      return NULL;
   im->data = malloc(im->w * im->h * sizeof(uint32_t));
   if (!(im->data))
     {
        __imlib_FreeImage(im);
        return NULL;
     }
   memcpy(im->data, im_old->data, im->w * im->h * sizeof(uint32_t));
   im->flags = im_old->flags;
   IM_FLAG_SET(im, F_UNCACHEABLE);
   im->moddate = im_old->moddate;
   im->border = im_old->border;
   im->loader = im_old->loader;
   if (im_old->format)
      im->format = strdup(im_old->format);
   if (im_old->file)
      im->file = strdup(im_old->file);
   return im;
}

EAPI                Imlib_Image
imlib_create_cropped_image(int x, int y, int width, int height)
{
   ImlibImage         *im, *im_old;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, NULL);
   if (!IMAGE_DIMENSIONS_OK(abs(width), abs(height)))
      return NULL;
   CAST_IMAGE(im_old, ctx->image);
   if (__imlib_LoadImageData(im_old))
      return NULL;
   im = __imlib_CreateImage(abs(width), abs(height), NULL);
   im->data = malloc(abs(width * height) * sizeof(uint32_t));
   if (!(im->data))
     {
        __imlib_FreeImage(im);
        return NULL;
     }
   if (IM_FLAG_ISSET(im_old, F_HAS_ALPHA))
     {
        IM_FLAG_SET(im, F_HAS_ALPHA);
        __imlib_BlendImageToImage(im_old, im, 0, 0, 1, x, y, abs(width),
                                  abs(height), 0, 0, width, height, NULL,
                                  (ImlibOp) IMLIB_OP_COPY,
                                  ctx->cliprect.x, ctx->cliprect.y,
                                  ctx->cliprect.w, ctx->cliprect.h);
     }
   else
     {
        __imlib_BlendImageToImage(im_old, im, 0, 0, 0, x, y, abs(width),
                                  abs(height), 0, 0, width, height, NULL,
                                  (ImlibOp) IMLIB_OP_COPY,
                                  ctx->cliprect.x, ctx->cliprect.y,
                                  ctx->cliprect.w, ctx->cliprect.h);
     }
   return im;
}

EAPI                Imlib_Image
imlib_create_cropped_scaled_image(int source_x, int source_y,
                                  int source_width, int source_height,
                                  int destination_width, int destination_height)
{
   ImlibImage         *im, *im_old;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, NULL);
   if (!IMAGE_DIMENSIONS_OK(abs(destination_width), abs(destination_height)))
      return NULL;
   CAST_IMAGE(im_old, ctx->image);
   if (__imlib_LoadImageData(im_old))
      return NULL;
   im = __imlib_CreateImage(abs(destination_width), abs(destination_height),
                            NULL);
   im->data =
      malloc(abs(destination_width * destination_height) * sizeof(uint32_t));
   if (!(im->data))
     {
        __imlib_FreeImage(im);
        return NULL;
     }
   if (IM_FLAG_ISSET(im_old, F_HAS_ALPHA))
     {
        IM_FLAG_SET(im, F_HAS_ALPHA);
        __imlib_BlendImageToImage(im_old, im, ctx->anti_alias, 0, 1, source_x,
                                  source_y, source_width, source_height, 0, 0,
                                  destination_width, destination_height, NULL,
                                  (ImlibOp) IMLIB_OP_COPY,
                                  ctx->cliprect.x, ctx->cliprect.y,
                                  ctx->cliprect.w, ctx->cliprect.h);
     }
   else
     {
        __imlib_BlendImageToImage(im_old, im, ctx->anti_alias, 0, 0, source_x,
                                  source_y, source_width, source_height, 0, 0,
                                  destination_width, destination_height, NULL,
                                  (ImlibOp) IMLIB_OP_COPY,
                                  ctx->cliprect.x, ctx->cliprect.y,
                                  ctx->cliprect.w, ctx->cliprect.h);
     }
   return im;
}

EAPI                Imlib_Updates
imlib_updates_clone(Imlib_Updates updates)
{
   ImlibUpdate        *u;

   u = (ImlibUpdate *) updates;
   return __imlib_DupUpdates(u);
}

EAPI                Imlib_Updates
imlib_update_append_rect(Imlib_Updates updates, int x, int y, int w, int h)
{
   ImlibUpdate        *u;

   u = (ImlibUpdate *) updates;
   return __imlib_AddUpdate(u, x, y, w, h);
}

EAPI                Imlib_Updates
imlib_updates_merge(Imlib_Updates updates, int w, int h)
{
   ImlibUpdate        *u;

   u = (ImlibUpdate *) updates;
   return __imlib_MergeUpdate(u, w, h, 0);
}

EAPI                Imlib_Updates
imlib_updates_merge_for_rendering(Imlib_Updates updates, int w, int h)
{
   ImlibUpdate        *u;

   u = (ImlibUpdate *) updates;
   return __imlib_MergeUpdate(u, w, h, 3);
}

EAPI void
imlib_updates_free(Imlib_Updates updates)
{
   ImlibUpdate        *u;

   u = (ImlibUpdate *) updates;
   __imlib_FreeUpdates(u);
}

EAPI                Imlib_Updates
imlib_updates_get_next(Imlib_Updates updates)
{
   ImlibUpdate        *u;

   u = (ImlibUpdate *) updates;
   return u->next;
}

EAPI void
imlib_updates_get_coordinates(Imlib_Updates updates, int *x_return,
                              int *y_return, int *width_return,
                              int *height_return)
{
   ImlibUpdate        *u;

   CHECK_PARAM_POINTER("updates", updates);
   u = (ImlibUpdate *) updates;
   if (x_return)
      *x_return = u->x;
   if (y_return)
      *y_return = u->y;
   if (width_return)
      *width_return = u->w;
   if (height_return)
      *height_return = u->h;
}

EAPI void
imlib_updates_set_coordinates(Imlib_Updates updates, int x, int y, int width,
                              int height)
{
   ImlibUpdate        *u;

   CHECK_PARAM_POINTER("updates", updates);
   u = (ImlibUpdate *) updates;
   u->x = x;
   u->y = y;
   u->w = width;
   u->h = height;
}

#ifdef BUILD_X11
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
   if (__imlib_LoadImageData(im))
      return;
   ximcs = __imlib_GetXImageCacheCountMax(ctx->display);        /* Save */
   if (ximcs == 0)              /* Only if we don't set this up elsewhere */
      __imlib_SetXImageCacheCountMax(ctx->display, 10);
   for (; u; u = u->next)
     {
        __imlib_RenderImage(ctx->display, im, ctx->drawable, 0, ctx->visual,
                            ctx->colormap, ctx->depth, u->x, u->y, u->w, u->h,
                            x + u->x, y + u->y, u->w, u->h, 0, ctx->dither, 0,
                            0, 0, ctx->color_modifier, OP_COPY);
     }
   if (ximcs == 0)
      __imlib_SetXImageCacheCountMax(ctx->display, ximcs);
}
#endif

EAPI                Imlib_Updates
imlib_updates_init(void)
{
   return NULL;
}

EAPI                Imlib_Updates
imlib_updates_append_updates(Imlib_Updates updates,
                             Imlib_Updates appended_updates)
{
   ImlibUpdate        *u, *uu;

   u = (ImlibUpdate *) updates;
   uu = (ImlibUpdate *) appended_updates;
   if (!uu)
      return u;
   if (!u)
      return uu;
   while (u)
     {
        if (!(u->next))
          {
             u->next = uu;
             return updates;
          }
        u = u->next;
     }
   return u;
}

EAPI void
imlib_image_flip_horizontal(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_FlipImageHoriz(im);
}

EAPI void
imlib_image_flip_vertical(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_FlipImageVert(im);
}

EAPI void
imlib_image_flip_diagonal(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_FlipImageDiagonal(im, 0);
}

EAPI void
imlib_image_orientate(int orientation)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   switch (orientation)
     {
     default:
     case 0:
        break;
     case 1:
        __imlib_FlipImageDiagonal(im, 1);
        break;
     case 2:
        __imlib_FlipImageBoth(im);
        break;
     case 3:
        __imlib_FlipImageDiagonal(im, 2);
        break;
     case 4:
        __imlib_FlipImageHoriz(im);
        break;
     case 5:
        __imlib_FlipImageDiagonal(im, 3);
        break;
     case 6:
        __imlib_FlipImageVert(im);
        break;
     case 7:
        __imlib_FlipImageDiagonal(im, 0);
        break;
     }
}

EAPI void
imlib_image_blur(int radius)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_BlurImage(im, radius);
}

EAPI void
imlib_image_sharpen(int radius)
{
   ImlibImage         *im;

   CAST_IMAGE(im, ctx->image);
   CHECK_PARAM_POINTER("image", ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_SharpenImage(im, radius);
}

EAPI void
imlib_image_tile_horizontal(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_TileImageHoriz(im);
}

EAPI void
imlib_image_tile_vertical(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_TileImageVert(im);
}

EAPI void
imlib_image_tile(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_TileImageHoriz(im);
   __imlib_TileImageVert(im);
}

EAPI                Imlib_Font
imlib_load_font(const char *font_name)
{
   return __imlib_font_load_joined(font_name);
}

EAPI void
imlib_free_font(void)
{
   CHECK_PARAM_POINTER("font", ctx->font);
   __imlib_font_free(ctx->font);
   ctx->font = NULL;
}

EAPI int
imlib_insert_font_into_fallback_chain(Imlib_Font font, Imlib_Font fallback_font)
{
   CHECK_PARAM_POINTER_RETURN("font", font, 1);
   CHECK_PARAM_POINTER_RETURN("fallback_font", fallback_font, 1);
   return __imlib_font_insert_into_fallback_chain_imp(font, fallback_font);
}

EAPI void
imlib_remove_font_from_fallback_chain(Imlib_Font fallback_font)
{
   CHECK_PARAM_POINTER("fallback_font", fallback_font);
   __imlib_font_remove_from_fallback_chain_imp(fallback_font);
}

EAPI                Imlib_Font
imlib_get_prev_font_in_fallback_chain(Imlib_Font fn)
{
   CHECK_PARAM_POINTER_RETURN("fn", fn, 0);
   return ((ImlibFont *) fn)->fallback_prev;
}

EAPI                Imlib_Font
imlib_get_next_font_in_fallback_chain(Imlib_Font fn)
{
   CHECK_PARAM_POINTER_RETURN("fn", fn, 0);
   return ((ImlibFont *) fn)->fallback_next;
}

EAPI void
imlib_text_draw(int x, int y, const char *text)
{
   imlib_text_draw_with_return_metrics(x, y, text, NULL, NULL, NULL, NULL);
}

EAPI void
imlib_text_draw_with_return_metrics(int x, int y, const char *text,
                                    int *width_return, int *height_return,
                                    int *horizontal_advance_return,
                                    int *vertical_advance_return)
{
   ImlibImage         *im;
   ImlibFont          *fn;
   int                 dir;

   CHECK_PARAM_POINTER("font", ctx->font);
   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("text", text);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   fn = (ImlibFont *) ctx->font;
   __imlib_DirtyImage(im);

   dir = ctx->direction;
   if (ctx->direction == IMLIB_TEXT_TO_ANGLE && ctx->angle == 0.0)
      dir = IMLIB_TEXT_TO_RIGHT;

   __imlib_render_str(im, fn, x, y, text, ctx->pixel, dir,
                      ctx->angle, width_return, height_return, 0,
                      horizontal_advance_return, vertical_advance_return,
                      ctx->operation,
                      ctx->cliprect.x, ctx->cliprect.y,
                      ctx->cliprect.w, ctx->cliprect.h);
}

EAPI void
imlib_get_text_size(const char *text, int *width_return, int *height_return)
{
   ImlibFont          *fn;
   int                 w, h;
   int                 dir;

   CHECK_PARAM_POINTER("font", ctx->font);
   CHECK_PARAM_POINTER("text", text);
   fn = (ImlibFont *) ctx->font;

   dir = ctx->direction;
   if (ctx->direction == IMLIB_TEXT_TO_ANGLE && ctx->angle == 0.0)
      dir = IMLIB_TEXT_TO_RIGHT;

   __imlib_font_query_size(fn, text, &w, &h);

   switch (dir)
     {
     case IMLIB_TEXT_TO_RIGHT:
     case IMLIB_TEXT_TO_LEFT:
        if (width_return)
           *width_return = w;
        if (height_return)
           *height_return = h;
        break;
     case IMLIB_TEXT_TO_DOWN:
     case IMLIB_TEXT_TO_UP:
        if (width_return)
           *width_return = h;
        if (height_return)
           *height_return = w;
        break;
     case IMLIB_TEXT_TO_ANGLE:
        if (width_return || height_return)
          {
             double              sa, ca;

             sa = sin(ctx->angle);
             ca = cos(ctx->angle);

             if (width_return)
               {
                  double              x1, x2, xt;

                  x1 = x2 = 0.0;
                  xt = ca * w;
                  if (xt < x1)
                     x1 = xt;
                  if (xt > x2)
                     x2 = xt;
                  xt = -(sa * h);
                  if (xt < x1)
                     x1 = xt;
                  if (xt > x2)
                     x2 = xt;
                  xt = ca * w - sa * h;
                  if (xt < x1)
                     x1 = xt;
                  if (xt > x2)
                     x2 = xt;
                  *width_return = (int)(x2 - x1);
               }
             if (height_return)
               {
                  double              y1, y2, yt;

                  y1 = y2 = 0.0;
                  yt = sa * w;
                  if (yt < y1)
                     y1 = yt;
                  if (yt > y2)
                     y2 = yt;
                  yt = ca * h;
                  if (yt < y1)
                     y1 = yt;
                  if (yt > y2)
                     y2 = yt;
                  yt = sa * w + ca * h;
                  if (yt < y1)
                     y1 = yt;
                  if (yt > y2)
                     y2 = yt;
                  *height_return = (int)(y2 - y1);
               }
          }
        break;
     default:
        break;
     }
}

EAPI void
imlib_get_text_advance(const char *text, int *horizontal_advance_return,
                       int *vertical_advance_return)
{
   ImlibFont          *fn;
   int                 w, h;

   CHECK_PARAM_POINTER("font", ctx->font);
   CHECK_PARAM_POINTER("text", text);
   fn = (ImlibFont *) ctx->font;
   __imlib_font_query_advance(fn, text, &w, &h);
   if (horizontal_advance_return)
      *horizontal_advance_return = w;
   if (vertical_advance_return)
      *vertical_advance_return = h;
}

EAPI int
imlib_get_text_inset(const char *text)
{
   ImlibFont          *fn;

   CHECK_PARAM_POINTER_RETURN("font", ctx->font, 0);
   CHECK_PARAM_POINTER_RETURN("text", text, 0);
   fn = (ImlibFont *) ctx->font;
   return __imlib_font_query_inset(fn, text);
}

EAPI void
imlib_add_path_to_font_path(const char *path)
{
   CHECK_PARAM_POINTER("path", path);
   if (!__imlib_font_path_exists(path))
      __imlib_font_add_font_path(path);
}

EAPI void
imlib_remove_path_from_font_path(const char *path)
{
   CHECK_PARAM_POINTER("path", path);
   __imlib_font_del_font_path(path);
}

EAPI char         **
imlib_list_font_path(int *number_return)
{
   CHECK_PARAM_POINTER_RETURN("number_return", number_return, NULL);
   return __imlib_font_list_font_path(number_return);
}

EAPI int
imlib_text_get_index_and_location(const char *text, int x, int y,
                                  int *char_x_return, int *char_y_return,
                                  int *char_width_return,
                                  int *char_height_return)
{
   ImlibFont          *fn;
   int                 w, h, cx, cy, cw, ch, cp, xx, yy;
   int                 dir;

   CHECK_PARAM_POINTER_RETURN("font", ctx->font, -1);
   CHECK_PARAM_POINTER_RETURN("text", text, -1);
   fn = (ImlibFont *) ctx->font;

   dir = ctx->direction;
   if (ctx->direction == IMLIB_TEXT_TO_ANGLE && ctx->angle == 0.0)
      dir = IMLIB_TEXT_TO_RIGHT;

   imlib_get_text_size(text, &w, &h);

   switch (dir)
     {
     case IMLIB_TEXT_TO_RIGHT:
        xx = x;
        yy = y;
        break;
     case IMLIB_TEXT_TO_LEFT:
        xx = w - x;
        yy = h - y;
        break;
     case IMLIB_TEXT_TO_DOWN:
        xx = y;
        yy = w - x;
        break;
     case IMLIB_TEXT_TO_UP:
        xx = h - y;
        yy = x;
        break;
     default:
        return -1;
     }

   cp = __imlib_font_query_text_at_pos(fn, text, xx, yy, &cx, &cy, &cw, &ch);

   switch (dir)
     {
     case IMLIB_TEXT_TO_RIGHT:
        if (char_x_return)
           *char_x_return = cx;
        if (char_y_return)
           *char_y_return = cy;
        if (char_width_return)
           *char_width_return = cw;
        if (char_height_return)
           *char_height_return = ch;
        return cp;
        break;
     case IMLIB_TEXT_TO_LEFT:
        cx = 1 + w - cx - cw;
        if (char_x_return)
           *char_x_return = cx;
        if (char_y_return)
           *char_y_return = cy;
        if (char_width_return)
           *char_width_return = cw;
        if (char_height_return)
           *char_height_return = ch;
        return cp;
        break;
     case IMLIB_TEXT_TO_DOWN:
        if (char_x_return)
           *char_x_return = cy;
        if (char_y_return)
           *char_y_return = cx;
        if (char_width_return)
           *char_width_return = ch;
        if (char_height_return)
           *char_height_return = cw;
        return cp;
        break;
     case IMLIB_TEXT_TO_UP:
        cy = 1 + h - cy - ch;
        if (char_x_return)
           *char_x_return = cy;
        if (char_y_return)
           *char_y_return = cx;
        if (char_width_return)
           *char_width_return = ch;
        if (char_height_return)
           *char_height_return = cw;
        return cp;
        break;
     default:
        return -1;
        break;
     }
   return -1;
}

EAPI void
imlib_text_get_location_at_index(const char *text, int index,
                                 int *char_x_return, int *char_y_return,
                                 int *char_width_return,
                                 int *char_height_return)
{
   ImlibFont          *fn;
   int                 cx, cy, cw, ch, w, h;

   CHECK_PARAM_POINTER("font", ctx->font);
   CHECK_PARAM_POINTER("text", text);
   fn = (ImlibFont *) ctx->font;

   __imlib_font_query_char_coords(fn, text, index, &cx, &cy, &cw, &ch);

   w = h = 0;
   imlib_get_text_size(text, &w, &h);

   switch (ctx->direction)
     {
     case IMLIB_TEXT_TO_RIGHT:
        if (char_x_return)
           *char_x_return = cx;
        if (char_y_return)
           *char_y_return = cy;
        if (char_width_return)
           *char_width_return = cw;
        if (char_height_return)
           *char_height_return = ch;
        return;
        break;
     case IMLIB_TEXT_TO_LEFT:
        cx = 1 + w - cx - cw;
        if (char_x_return)
           *char_x_return = cx;
        if (char_y_return)
           *char_y_return = cy;
        if (char_width_return)
           *char_width_return = cw;
        if (char_height_return)
           *char_height_return = ch;
        return;
        break;
     case IMLIB_TEXT_TO_DOWN:
        if (char_x_return)
           *char_x_return = cy;
        if (char_y_return)
           *char_y_return = cx;
        if (char_width_return)
           *char_width_return = ch;
        if (char_height_return)
           *char_height_return = cw;
        return;
        break;
     case IMLIB_TEXT_TO_UP:
        cy = 1 + h - cy - ch;
        if (char_x_return)
           *char_x_return = cy;
        if (char_y_return)
           *char_y_return = cx;
        if (char_width_return)
           *char_width_return = ch;
        if (char_height_return)
           *char_height_return = cw;
        return;
        break;
     default:
        return;
        break;
     }
}

EAPI char         **
imlib_list_fonts(int *number_return)
{
   CHECK_PARAM_POINTER_RETURN("number_return", number_return, NULL);
   return __imlib_font_list_fonts(number_return);
}

EAPI void
imlib_free_font_list(char **font_list, int number)
{
   __imlib_FileFreeDirList(font_list, number);
}

EAPI int
imlib_get_font_cache_size(void)
{
   return __imlib_font_cache_get();
}

EAPI void
imlib_set_font_cache_size(int bytes)
{
   __imlib_font_cache_set(bytes);
}

EAPI void
imlib_flush_font_cache(void)
{
   __imlib_font_flush();
}

EAPI int
imlib_get_font_ascent(void)
{
   CHECK_PARAM_POINTER_RETURN("font", ctx->font, 0);
   return __imlib_font_ascent_get(ctx->font);
}

EAPI int
imlib_get_font_descent(void)
{
   CHECK_PARAM_POINTER_RETURN("font", ctx->font, 0);
   return __imlib_font_descent_get(ctx->font);
}

EAPI int
imlib_get_maximum_font_ascent(void)
{
   CHECK_PARAM_POINTER_RETURN("font", ctx->font, 0);
   return __imlib_font_max_ascent_get(ctx->font);
}

EAPI int
imlib_get_maximum_font_descent(void)
{
   CHECK_PARAM_POINTER_RETURN("font", ctx->font, 0);
   return __imlib_font_max_descent_get(ctx->font);
}

EAPI                Imlib_Color_Modifier
imlib_create_color_modifier(void)
{
   return __imlib_CreateCmod();
}

EAPI void
imlib_free_color_modifier(void)
{
   CHECK_PARAM_POINTER("color_modifier", ctx->color_modifier);
   __imlib_FreeCmod((ImlibColorModifier *) ctx->color_modifier);
   ctx->color_modifier = NULL;
}

EAPI void
imlib_modify_color_modifier_gamma(double gamma_value)
{
   CHECK_PARAM_POINTER("color_modifier", ctx->color_modifier);
   __imlib_CmodModGamma((ImlibColorModifier *) ctx->color_modifier,
                        gamma_value);
}

EAPI void
imlib_modify_color_modifier_brightness(double brightness_value)
{
   CHECK_PARAM_POINTER("color_modifier", ctx->color_modifier);
   __imlib_CmodModBrightness((ImlibColorModifier *) ctx->color_modifier,
                             brightness_value);
}

EAPI void
imlib_modify_color_modifier_contrast(double contrast_value)
{
   CHECK_PARAM_POINTER("color_modifier", ctx->color_modifier);
   __imlib_CmodModContrast((ImlibColorModifier *) ctx->color_modifier,
                           contrast_value);
}

EAPI void
imlib_set_color_modifier_tables(uint8_t * red_table, uint8_t * green_table,
                                uint8_t * blue_table, uint8_t * alpha_table)
{
   CHECK_PARAM_POINTER("color_modifier", ctx->color_modifier);
   __imlib_CmodSetTables((ImlibColorModifier *) ctx->color_modifier,
                         red_table, green_table, blue_table, alpha_table);
}

EAPI void
imlib_get_color_modifier_tables(uint8_t * red_table, uint8_t * green_table,
                                uint8_t * blue_table, uint8_t * alpha_table)
{
   CHECK_PARAM_POINTER("color_modifier", ctx->color_modifier);
   __imlib_CmodGetTables((ImlibColorModifier *) ctx->color_modifier,
                         red_table, green_table, blue_table, alpha_table);
}

EAPI void
imlib_reset_color_modifier(void)
{
   CHECK_PARAM_POINTER("color_modifier", ctx->color_modifier);
   __imlib_CmodReset((ImlibColorModifier *) ctx->color_modifier);
}

EAPI void
imlib_apply_color_modifier(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("color_modifier", ctx->color_modifier);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_DataCmodApply(im->data, im->w, im->h, 0, &(im->flags),
                         (ImlibColorModifier *) ctx->color_modifier);
}

EAPI void
imlib_apply_color_modifier_to_rectangle(int x, int y, int width, int height)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("color_modifier", ctx->color_modifier);
   CAST_IMAGE(im, ctx->image);
   if (x < 0)
     {
        width += x;
        x = 0;
     }
   if (width <= 0)
      return;
   if ((x + width) > im->w)
      width = (im->w - x);
   if (width <= 0)
      return;
   if (y < 0)
     {
        height += y;
        y = 0;
     }
   if (height <= 0)
      return;
   if ((y + height) > im->h)
      height = (im->h - y);
   if (height <= 0)
      return;
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_DataCmodApply(im->data + (y * im->w) + x, width, height,
                         im->w - width, &(im->flags),
                         (ImlibColorModifier *) ctx->color_modifier);
}

EAPI                Imlib_Updates
imlib_image_draw_pixel(int x, int y, char make_updates)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, NULL);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return NULL;
   __imlib_DirtyImage(im);
   return __imlib_Point_DrawToImage(x, y, ctx->pixel, im,
                                    ctx->cliprect.x,
                                    ctx->cliprect.y,
                                    ctx->cliprect.w,
                                    ctx->cliprect.h,
                                    ctx->operation, ctx->blend, make_updates);
}

EAPI                Imlib_Updates
imlib_image_draw_line(int x1, int y1, int x2, int y2, char make_updates)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, NULL);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return NULL;
   __imlib_DirtyImage(im);
   return __imlib_Line_DrawToImage(x1, y1, x2, y2, ctx->pixel,
                                   im, ctx->cliprect.x,
                                   ctx->cliprect.y,
                                   ctx->cliprect.w,
                                   ctx->cliprect.h,
                                   ctx->operation, ctx->blend,
                                   ctx->anti_alias, make_updates);
}

EAPI void
imlib_image_draw_rectangle(int x, int y, int width, int height)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_Rectangle_DrawToImage(x, y, width, height, ctx->pixel,
                                 im, ctx->cliprect.x, ctx->cliprect.y,
                                 ctx->cliprect.w, ctx->cliprect.h,
                                 ctx->operation, ctx->blend);
}

EAPI void
imlib_image_fill_rectangle(int x, int y, int width, int height)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_Rectangle_FillToImage(x, y, width, height, ctx->pixel,
                                 im, ctx->cliprect.x, ctx->cliprect.y,
                                 ctx->cliprect.w, ctx->cliprect.h,
                                 ctx->operation, ctx->blend);
}

EAPI void
imlib_image_copy_alpha_to_image(Imlib_Image image_source, int x, int y)
{
   ImlibImage         *im, *im2;

   CHECK_PARAM_POINTER("image_source", image_source);
   CHECK_PARAM_POINTER("image_destination", ctx->image);
   CAST_IMAGE(im, image_source);
   CAST_IMAGE(im2, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   if (__imlib_LoadImageData(im2))
      return;
   __imlib_DirtyImage(im);
   __imlib_copy_alpha_data(im, im2, 0, 0, im->w, im->h, x, y);
}

EAPI void
imlib_image_copy_alpha_rectangle_to_image(Imlib_Image image_source, int x,
                                          int y, int width, int height,
                                          int destination_x, int destination_y)
{
   ImlibImage         *im, *im2;

   CHECK_PARAM_POINTER("image_source", image_source);
   CHECK_PARAM_POINTER("image_destination", ctx->image);
   CAST_IMAGE(im, image_source);
   CAST_IMAGE(im2, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   if (__imlib_LoadImageData(im2))
      return;
   __imlib_DirtyImage(im);
   __imlib_copy_alpha_data(im, im2, x, y, width, height, destination_x,
                           destination_y);
}

EAPI void
imlib_image_scroll_rect(int x, int y, int width, int height, int delta_x,
                        int delta_y)
{
   ImlibImage         *im;
   int                 xx, yy, w, h, nx, ny;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   if (delta_x > 0)
     {
        xx = x;
        nx = x + delta_x;
        w = width - delta_x;
     }
   else
     {
        xx = x - delta_x;
        nx = x;
        w = width + delta_x;
     }
   if (delta_y > 0)
     {
        yy = y;
        ny = y + delta_y;
        h = height - delta_y;
     }
   else
     {
        yy = y - delta_y;
        ny = y;
        h = height + delta_y;
     }
   __imlib_DirtyImage(im);
   __imlib_copy_image_data(im, xx, yy, w, h, nx, ny);
}

EAPI void
imlib_image_copy_rect(int x, int y, int width, int height, int new_x, int new_y)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_copy_image_data(im, x, y, width, height, new_x, new_y);
}

EAPI                Imlib_Color_Range
imlib_create_color_range(void)
{
   return __imlib_CreateRange();
}

EAPI void
imlib_free_color_range(void)
{
   CHECK_PARAM_POINTER("color_range", ctx->color_range);
   __imlib_FreeRange((ImlibRange *) ctx->color_range);
   ctx->color_range = NULL;
}

EAPI void
imlib_add_color_to_color_range(int distance_away)
{
   CHECK_PARAM_POINTER("color_range", ctx->color_range);
   __imlib_AddRangeColor((ImlibRange *) ctx->color_range, ctx->color.red,
                         ctx->color.green, ctx->color.blue, ctx->color.alpha,
                         distance_away);
}

EAPI void
imlib_image_fill_color_range_rectangle(int x, int y, int width, int height,
                                       double angle)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("color_range", ctx->color_range);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_DrawGradient(im, x, y, width, height,
                        (ImlibRange *) ctx->color_range, angle,
                        ctx->operation,
                        ctx->cliprect.x, ctx->cliprect.y,
                        ctx->cliprect.w, ctx->cliprect.h);
}

EAPI void
imlib_image_fill_hsva_color_range_rectangle(int x, int y, int width, int height,
                                            double angle)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("color_range", ctx->color_range);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_DrawHsvaGradient(im, x, y, width, height,
                            (ImlibRange *) ctx->color_range, angle,
                            ctx->operation,
                            ctx->cliprect.x, ctx->cliprect.y,
                            ctx->cliprect.w, ctx->cliprect.h);
}

EAPI void
imlib_image_query_pixel(int x, int y, Imlib_Color * color_return)
{
   ImlibImage         *im;
   uint32_t           *p;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("color_return", color_return);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   if ((x < 0) || (x >= im->w) || (y < 0) || (y >= im->h))
     {
        color_return->red = 0;
        color_return->green = 0;
        color_return->blue = 0;
        color_return->alpha = 0;
        return;
     }
   p = im->data + (im->w * y) + x;
   color_return->red = ((*p) >> 16) & 0xff;
   color_return->green = ((*p) >> 8) & 0xff;
   color_return->blue = (*p) & 0xff;
   color_return->alpha = ((*p) >> 24) & 0xff;
}

EAPI void
imlib_image_query_pixel_hsva(int x, int y, float *hue, float *saturation,
                             float *value, int *alpha)
{
   ImlibImage         *im;
   uint32_t           *p;
   int                 r, g, b;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   if ((x < 0) || (x >= im->w) || (y < 0) || (y >= im->h))
     {
        *hue = 0;
        *saturation = 0;
        *value = 0;
        *alpha = 0;
        return;
     }
   p = im->data + (im->w * y) + x;
   r = ((*p) >> 16) & 0xff;
   g = ((*p) >> 8) & 0xff;
   b = (*p) & 0xff;
   *alpha = ((*p) >> 24) & 0xff;

   __imlib_rgb_to_hsv(r, g, b, hue, saturation, value);
}

EAPI void
imlib_image_query_pixel_hlsa(int x, int y, float *hue, float *lightness,
                             float *saturation, int *alpha)
{
   ImlibImage         *im;
   uint32_t           *p;
   int                 r, g, b;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   if ((x < 0) || (x >= im->w) || (y < 0) || (y >= im->h))
     {
        *hue = 0;
        *lightness = 0;
        *saturation = 0;
        *alpha = 0;
        return;
     }
   p = im->data + (im->w * y) + x;
   r = ((*p) >> 16) & 0xff;
   g = ((*p) >> 8) & 0xff;
   b = (*p) & 0xff;
   *alpha = ((*p) >> 24) & 0xff;

   __imlib_rgb_to_hls(r, g, b, hue, lightness, saturation);
}

EAPI void
imlib_image_query_pixel_cmya(int x, int y, int *cyan, int *magenta, int *yellow,
                             int *alpha)
{
   ImlibImage         *im;
   uint32_t           *p;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   if ((x < 0) || (x >= im->w) || (y < 0) || (y >= im->h))
     {
        *cyan = 0;
        *magenta = 0;
        *yellow = 0;
        *alpha = 0;
        return;
     }
   p = im->data + (im->w * y) + x;
   *cyan = 255 - (((*p) >> 16) & 0xff);
   *magenta = 255 - (((*p) >> 8) & 0xff);
   *yellow = 255 - ((*p) & 0xff);
   *alpha = ((*p) >> 24) & 0xff;
}

EAPI void
imlib_image_attach_data_value(const char *key, void *data, int value,
                              Imlib_Internal_Data_Destructor_Function
                              destructor_function)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("key", key);
   CAST_IMAGE(im, ctx->image);
   __imlib_AttachTag(im, key, value, data,
                     (ImlibDataDestructorFunction) destructor_function);
}

EAPI void          *
imlib_image_get_attached_data(const char *key)
{
   ImlibImageTag      *t;
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, NULL);
   CHECK_PARAM_POINTER_RETURN("key", key, NULL);
   CAST_IMAGE(im, ctx->image);
   t = __imlib_GetTag(im, key);
   if (t)
      return t->data;
   return NULL;
}

EAPI int
imlib_image_get_attached_value(const char *key)
{
   ImlibImageTag      *t;
   ImlibImage         *im;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, 0);
   CHECK_PARAM_POINTER_RETURN("key", key, 0);
   CAST_IMAGE(im, ctx->image);
   t = __imlib_GetTag(im, key);
   if (t)
      return t->val;
   return 0;
}

EAPI void
imlib_image_remove_attached_data_value(const char *key)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("key", key);
   CAST_IMAGE(im, ctx->image);
   __imlib_RemoveTag(im, key);
}

EAPI void
imlib_image_remove_and_free_attached_data_value(const char *key)
{
   ImlibImageTag      *t;
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("key", key);
   CAST_IMAGE(im, ctx->image);
   t = __imlib_RemoveTag(im, key);
   __imlib_FreeTag(im, t);
}

static void
_imlib_save_image(const char *file, int *err)
{
   ImlibImage         *im;
   ImlibLoadArgs       ila = { ILA0(ctx, 0, 0) };

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("file", file);
   CAST_IMAGE(im, ctx->image);

   if (__imlib_LoadImageData(im))
      return;

   __imlib_SaveImage(im, file, &ila);
   *err = ila.err;
}

EAPI void
imlib_save_image(const char *file)
{
   int                 err;

   _imlib_save_image(file, &err);
}

EAPI void
imlib_save_image_with_error_return(const char *file,
                                   Imlib_Load_Error * error_return)
{
   int                 err = 0;

   _imlib_save_image(file, &err);

   if (error_return)
      *error_return = __imlib_ErrorFromErrno(err, 1);
}

EAPI void
imlib_save_image_with_errno_return(const char *file, int *error_return)
{
   int                 err = 0;

   _imlib_save_image(file, &err);

   if (error_return)
      *error_return = err;
}

EAPI                Imlib_Image
imlib_create_rotated_image(double angle)
{
   ImlibImage         *im, *im_old;
   int                 x, y, dx, dy, sz;
   double              x1, y1, d;

   CHECK_PARAM_POINTER_RETURN("image", ctx->image, NULL);
   CAST_IMAGE(im_old, ctx->image);
   if (__imlib_LoadImageData(im_old))
      return NULL;

   d = hypot((double)(im_old->w + 4), (double)(im_old->h + 4)) / sqrt(2.0);

   x1 = (double)(im_old->w) / 2.0 - sin(angle + atan(1.0)) * d;
   y1 = (double)(im_old->h) / 2.0 - cos(angle + atan(1.0)) * d;

   sz = (int)(d * sqrt(2.0));
   x = (int)(x1 * _ROTATE_PREC_MAX);
   y = (int)(y1 * _ROTATE_PREC_MAX);
   dx = (int)(cos(angle) * _ROTATE_PREC_MAX);
   dy = -(int)(sin(angle) * _ROTATE_PREC_MAX);

   if (!IMAGE_DIMENSIONS_OK(sz, sz))
      return NULL;

   im = __imlib_CreateImage(sz, sz, NULL);
   im->data = calloc(sz * sz, sizeof(uint32_t));
   if (!(im->data))
     {
        __imlib_FreeImage(im);
        return NULL;
     }

   if (ctx->anti_alias)
     {
        __imlib_RotateAA(im_old->data, im->data, im_old->w, im_old->w,
                         im_old->h, im->w, sz, sz, x, y, dx, dy, -dy, dx);
     }
   else
     {
        __imlib_RotateSample(im_old->data, im->data, im_old->w, im_old->w,
                             im_old->h, im->w, sz, sz, x, y, dx, dy, -dy, dx);
     }
   IM_FLAG_SET(im, F_HAS_ALPHA);

   return im;
}

void
imlib_rotate_image_from_buffer(double angle, Imlib_Image source_image)
{
   ImlibImage         *im, *im_old;
   int                 x, y, dx, dy, sz;
   double              x1, y1, d;

   // source image (to rotate)
   CHECK_PARAM_POINTER("source_image", source_image);
   CAST_IMAGE(im_old, source_image);

   // current context image
   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);

   if (__imlib_LoadImageData(im_old))
      return;

   d = hypot((double)(im_old->w + 4), (double)(im_old->h + 4)) / sqrt(2.0);

   x1 = (double)(im_old->w) / 2.0 - sin(angle + atan(1.0)) * d;
   y1 = (double)(im_old->h) / 2.0 - cos(angle + atan(1.0)) * d;

   sz = (int)(d * sqrt(2.0));
   x = (int)(x1 * _ROTATE_PREC_MAX);
   y = (int)(y1 * _ROTATE_PREC_MAX);
   dx = (int)(cos(angle) * _ROTATE_PREC_MAX);
   dy = -(int)(sin(angle) * _ROTATE_PREC_MAX);

   if ((im->w != im->h) || ((im->w < sz) && (im->h < sz)))
      return;                   // If size is wrong
   else
      sz = im->w;               // update sz with real width

#if 0                           /* Not necessary 'cause destination is context */
   im = __imlib_CreateImage(sz, sz, NULL);
   im->data = calloc(sz * sz, sizeof(uint32_t));
   if (!(im->data))
     {
        __imlib_FreeImage(im);
        return;
     }
#endif

   if (ctx->anti_alias)
     {
        __imlib_RotateAA(im_old->data, im->data, im_old->w, im_old->w,
                         im_old->h, im->w, sz, sz, x, y, dx, dy, -dy, dx);
     }
   else
     {
        __imlib_RotateSample(im_old->data, im->data, im_old->w, im_old->w,
                             im_old->h, im->w, sz, sz, x, y, dx, dy, -dy, dx);
     }
   IM_FLAG_SET(im, F_HAS_ALPHA);

   return;
}

EAPI void
imlib_blend_image_onto_image_at_angle(Imlib_Image source_image,
                                      char merge_alpha, int source_x,
                                      int source_y, int source_width,
                                      int source_height, int destination_x,
                                      int destination_y, int angle_x,
                                      int angle_y)
{
   ImlibImage         *im_src, *im_dst;

   CHECK_PARAM_POINTER("source_image", source_image);
   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im_src, source_image);
   CAST_IMAGE(im_dst, ctx->image);
   if (__imlib_LoadImageData(im_src))
      return;
   if (__imlib_LoadImageData(im_dst))
      return;
   __imlib_DirtyImage(im_dst);
   __imlib_BlendImageToImageSkewed(im_src, im_dst, ctx->anti_alias,
                                   ctx->blend, merge_alpha, source_x,
                                   source_y, source_width, source_height,
                                   destination_x, destination_y, angle_x,
                                   angle_y, 0, 0, ctx->color_modifier,
                                   ctx->operation,
                                   ctx->cliprect.x, ctx->cliprect.y,
                                   ctx->cliprect.w, ctx->cliprect.h);
}

EAPI void
imlib_blend_image_onto_image_skewed(Imlib_Image source_image,
                                    char merge_alpha, int source_x,
                                    int source_y, int source_width,
                                    int source_height, int destination_x,
                                    int destination_y, int h_angle_x,
                                    int h_angle_y, int v_angle_x, int v_angle_y)
{
   ImlibImage         *im_src, *im_dst;

   CHECK_PARAM_POINTER("source_image", source_image);
   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im_src, source_image);
   CAST_IMAGE(im_dst, ctx->image);
   if (__imlib_LoadImageData(im_src))
      return;
   if (__imlib_LoadImageData(im_dst))
      return;
   __imlib_DirtyImage(im_dst);
   __imlib_BlendImageToImageSkewed(im_src, im_dst, ctx->anti_alias,
                                   ctx->blend, merge_alpha, source_x,
                                   source_y, source_width, source_height,
                                   destination_x, destination_y, h_angle_x,
                                   h_angle_y, v_angle_x, v_angle_y,
                                   ctx->color_modifier, ctx->operation,
                                   ctx->cliprect.x, ctx->cliprect.y,
                                   ctx->cliprect.w, ctx->cliprect.h);
}

#ifdef BUILD_X11
EAPI void
imlib_render_image_on_drawable_skewed(int source_x, int source_y,
                                      int source_width, int source_height,
                                      int destination_x, int destination_y,
                                      int h_angle_x, int h_angle_y,
                                      int v_angle_x, int v_angle_y)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_RenderImageSkewed(ctx->display, im, ctx->drawable, ctx->mask,
                             ctx->visual, ctx->colormap, ctx->depth, source_x,
                             source_y, source_width, source_height,
                             destination_x, destination_y, h_angle_x,
                             h_angle_y, v_angle_x, v_angle_y, ctx->anti_alias,
                             ctx->dither, ctx->blend, ctx->dither_mask,
                             ctx->mask_alpha_threshold, ctx->color_modifier,
                             ctx->operation);
}

EAPI void
imlib_render_image_on_drawable_at_angle(int source_x, int source_y,
                                        int source_width, int source_height,
                                        int destination_x, int destination_y,
                                        int angle_x, int angle_y)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_RenderImageSkewed(ctx->display, im, ctx->drawable, ctx->mask,
                             ctx->visual, ctx->colormap, ctx->depth, source_x,
                             source_y, source_width, source_height,
                             destination_x, destination_y, angle_x, angle_y,
                             0, 0, ctx->anti_alias, ctx->dither, ctx->blend,
                             ctx->dither_mask, ctx->mask_alpha_threshold,
                             ctx->color_modifier, ctx->operation);
}
#endif

EAPI void
imlib_image_filter(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CHECK_PARAM_POINTER("filter", ctx->filter);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_FilterImage(im, (ImlibFilter *) ctx->filter);
}

EAPI                Imlib_Filter
imlib_create_filter(int initsize)
{
   return __imlib_CreateFilter(initsize);
}

EAPI void
imlib_free_filter(void)
{
   CHECK_PARAM_POINTER("filter", ctx->filter);
   __imlib_FreeFilter((ImlibFilter *) ctx->filter);
   ctx->filter = NULL;
}

EAPI void
imlib_context_set_filter(Imlib_Filter filter)
{
   ctx->filter = filter;
}

EAPI                Imlib_Filter
imlib_context_get_filter(void)
{
   return ctx->filter;
}

EAPI void
imlib_filter_set(int xoff, int yoff, int a, int r, int g, int b)
{
   ImlibFilter        *fil;

   CHECK_PARAM_POINTER("filter", ctx->filter);
   fil = (ImlibFilter *) ctx->filter;
   __imlib_FilterSetColor(&fil->alpha, xoff, yoff, a, 0, 0, 0);
   __imlib_FilterSetColor(&fil->red, xoff, yoff, 0, r, 0, 0);
   __imlib_FilterSetColor(&fil->green, xoff, yoff, 0, 0, g, 0);
   __imlib_FilterSetColor(&fil->blue, xoff, yoff, 0, 0, 0, b);
}

EAPI void
imlib_filter_set_alpha(int xoff, int yoff, int a, int r, int g, int b)
{
   ImlibFilter        *fil;

   CHECK_PARAM_POINTER("filter", ctx->filter);
   fil = (ImlibFilter *) ctx->filter;
   __imlib_FilterSetColor(&fil->alpha, xoff, yoff, a, r, g, b);
}

EAPI void
imlib_filter_set_red(int xoff, int yoff, int a, int r, int g, int b)
{
   ImlibFilter        *fil;

   CHECK_PARAM_POINTER("filter", ctx->filter);
   fil = (ImlibFilter *) ctx->filter;
   __imlib_FilterSetColor(&fil->red, xoff, yoff, a, r, g, b);
}

EAPI void
imlib_filter_set_green(int xoff, int yoff, int a, int r, int g, int b)
{
   ImlibFilter        *fil;

   CHECK_PARAM_POINTER("filter", ctx->filter);
   fil = (ImlibFilter *) ctx->filter;
   __imlib_FilterSetColor(&fil->green, xoff, yoff, a, r, g, b);
}

EAPI void
imlib_filter_set_blue(int xoff, int yoff, int a, int r, int g, int b)
{
   ImlibFilter        *fil;

   CHECK_PARAM_POINTER("filter", ctx->filter);
   fil = (ImlibFilter *) ctx->filter;
   __imlib_FilterSetColor(&fil->blue, xoff, yoff, a, r, g, b);
}

EAPI void
imlib_filter_constants(int a, int r, int g, int b)
{
   CHECK_PARAM_POINTER("filter", ctx->filter);
   __imlib_FilterConstants((ImlibFilter *) ctx->filter, a, r, g, b);
}

EAPI void
imlib_filter_divisors(int a, int r, int g, int b)
{
   CHECK_PARAM_POINTER("filter", ctx->filter);
   __imlib_FilterDivisors((ImlibFilter *) ctx->filter, a, r, g, b);
}

EAPI void
imlib_apply_filter(const char *script, ...)
{
   va_list             param_list;
   ImlibImage         *im;

   __imlib_dynamic_filters_init();
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   va_start(param_list, script);
   __imlib_script_parse(im, script, param_list);
   va_end(param_list);
}

EAPI                ImlibPolygon
imlib_polygon_new(void)
{
   return __imlib_polygon_new();
}

EAPI void
imlib_polygon_add_point(ImlibPolygon poly, int x, int y)
{
   CHECK_PARAM_POINTER("polygon", poly);
   __imlib_polygon_add_point((ImlibPoly *) poly, x, y);
}

EAPI void
imlib_polygon_free(ImlibPolygon poly)
{
   CHECK_PARAM_POINTER("polygon", poly);
   __imlib_polygon_free((ImlibPoly *) poly);
}

EAPI void
imlib_image_draw_polygon(ImlibPolygon poly, unsigned char closed)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_Polygon_DrawToImage((ImlibPoly *) poly, closed, ctx->pixel,
                               im, ctx->cliprect.x, ctx->cliprect.y,
                               ctx->cliprect.w, ctx->cliprect.h,
                               ctx->operation, ctx->blend, ctx->anti_alias);
}

EAPI void
imlib_image_fill_polygon(ImlibPolygon poly)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_Polygon_FillToImage((ImlibPoly *) poly, ctx->pixel,
                               im, ctx->cliprect.x, ctx->cliprect.y,
                               ctx->cliprect.w, ctx->cliprect.h,
                               ctx->operation, ctx->blend, ctx->anti_alias);
}

EAPI void
imlib_polygon_get_bounds(ImlibPolygon poly, int *px1, int *py1, int *px2,
                         int *py2)
{
   CHECK_PARAM_POINTER("polygon", poly);
   __imlib_polygon_get_bounds((ImlibPoly *) poly, px1, py1, px2, py2);
}

EAPI void
imlib_image_draw_ellipse(int xc, int yc, int a, int b)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_Ellipse_DrawToImage(xc, yc, a, b, ctx->pixel,
                               im, ctx->cliprect.x, ctx->cliprect.y,
                               ctx->cliprect.w, ctx->cliprect.h,
                               ctx->operation, ctx->blend, ctx->anti_alias);
}

EAPI void
imlib_image_fill_ellipse(int xc, int yc, int a, int b)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   __imlib_Ellipse_FillToImage(xc, yc, a, b, ctx->pixel,
                               im, ctx->cliprect.x, ctx->cliprect.y,
                               ctx->cliprect.w, ctx->cliprect.h,
                               ctx->operation, ctx->blend, ctx->anti_alias);
}

EAPI unsigned char
imlib_polygon_contains_point(ImlibPolygon poly, int x, int y)
{
   CHECK_PARAM_POINTER_RETURN("polygon", poly, 0);
   return __imlib_polygon_contains_point((ImlibPoly *) poly, x, y);
}

EAPI void
imlib_image_clear(void)
{
   ImlibImage         *im;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   memset(im->data, 0, im->w * im->h * sizeof(uint32_t));
}

EAPI void
imlib_image_clear_color(int r, int g, int b, int a)
{
   ImlibImage         *im;
   int                 i, max;
   uint32_t            col;

   CHECK_PARAM_POINTER("image", ctx->image);
   CAST_IMAGE(im, ctx->image);
   if (__imlib_LoadImageData(im))
      return;
   __imlib_DirtyImage(im);
   max = im->w * im->h;
   col = PIXEL_ARGB(a, r, g, b);
   for (i = 0; i < max; i++)
      im->data[i] = col;
}

EAPI const char    *
imlib_strerror(int err)
{
   const char         *str;

   if (err >= 0)
     {
        str = strerror(err);
     }
   else
     {
        switch (err)
          {
          default:             /* Should not happen */
             str = "Imlib2: Unknown error";
             break;
          case IMLIB_ERR_INTERNAL:     /* Should not happen */
             str = "Imlib2: Internal error";
             break;
          case IMLIB_ERR_NO_LOADER:
             str = "Imlib2: No loader for file format";
             break;
          case IMLIB_ERR_NO_SAVER:
             str = "Imlib2: No saver for file format";
             break;
          case IMLIB_ERR_BAD_IMAGE:
             str = "Imlib2: Invalid image file";
             break;
          case IMLIB_ERR_BAD_FRAME:
             str = "Imlib2: Requested frame not in image";
             break;
          }
     }

   return str;
}
