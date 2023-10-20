#ifndef __IMLIB_API_H
#define __IMLIB_API_H 1
/** @file
 * Imlib2 library API
 */
#define IMLIB2_VERSION_MAJOR 1
#define IMLIB2_VERSION_MINOR 11
#define IMLIB2_VERSION_MICRO 0

#define IMLIB2_VERSION_(maj, min, mic) (10000 * (maj) + 100 * (min) + (mic))
#define IMLIB2_VERSION IMLIB2_VERSION_(IMLIB2_VERSION_MAJOR, IMLIB2_VERSION_MINOR, IMLIB2_VERSION_MICRO)

#ifdef EAPI
#undef EAPI
#endif
#ifdef WIN32
#ifdef BUILDING_DLL
#define EAPI __declspec(dllexport)
#else
#define EAPI __declspec(dllimport)
#endif
#else
#ifdef __GNUC__
#if __GNUC__ >= 4
#define EAPI __attribute__ ((visibility("default")))
#else
#define EAPI
#endif
#else
#define EAPI
#endif
#endif

#ifndef IMLIB2_DEPRECATED
#ifdef __GNUC__
#define IMLIB2_DEPRECATED __attribute__((deprecated))
#else
#define IMLIB2_DEPRECATED
#endif
#endif

#include <stddef.h>
#include <stdint.h>
#ifndef X_DISPLAY_MISSING
#include <X11/Xlib.h>
#endif

/* opaque data types */
typedef void       *Imlib_Context;
typedef void       *Imlib_Image;
typedef void       *Imlib_Color_Modifier;
typedef void       *Imlib_Updates;
typedef void       *Imlib_Font;
typedef void       *Imlib_Color_Range;
typedef void       *Imlib_Filter;
typedef void       *ImlibPolygon;

/* blending operations */
typedef enum {
   IMLIB_OP_COPY,
   IMLIB_OP_ADD,
   IMLIB_OP_SUBTRACT,
   IMLIB_OP_RESHADE
} Imlib_Operation;

typedef enum {
   IMLIB_TEXT_TO_RIGHT = 0,
   IMLIB_TEXT_TO_LEFT = 1,
   IMLIB_TEXT_TO_DOWN = 2,
   IMLIB_TEXT_TO_UP = 3,
   IMLIB_TEXT_TO_ANGLE = 4
} Imlib_Text_Direction;

#define IMLIB_ERR_INTERNAL      -1      /* Internal error (should not happen) */
#define IMLIB_ERR_NO_LOADER     -2      /* No loader for file format */
#define IMLIB_ERR_NO_SAVER      -3      /* No saver for file format */
#define IMLIB_ERR_BAD_IMAGE     -4      /* Invalid image file */
#define IMLIB_ERR_BAD_FRAME     -5      /* Requested frame not in image */

typedef struct {
   int                 left, right, top, bottom;
} Imlib_Border;

typedef struct {
   int                 alpha, red, green, blue;
} Imlib_Color;

/* Progressive loading callback */
typedef int         (*Imlib_Progress_Function)(Imlib_Image im, char percent,
                                               int update_x, int update_y,
                                               int update_w, int update_h);

/* Custom attached data destructor callback */
typedef void        (*Imlib_Data_Destructor_Function)(Imlib_Image im,
                                                      void *data);

/* Custom image data memory management function */
typedef void       *(*Imlib_Image_Data_Memory_Function)(void *, size_t size);

/* *INDENT-OFF* */
#ifdef __cplusplus
extern "C" {
#endif
/* *INDENT-ON* */

/** Return Imlib2 version */
EAPI int            imlib_version(void);

/*--------------------------------
 * Context handling
 */

/** Create new context */
EAPI Imlib_Context  imlib_context_new(void);

/** Free context */
EAPI void           imlib_context_free(Imlib_Context context);

/** Push context */
EAPI void           imlib_context_push(Imlib_Context context);

/** Pop context */
EAPI void           imlib_context_pop(void);

/** Get context handle */
EAPI Imlib_Context  imlib_context_get(void);

/*--------------------------------
 * Context operations
 */

#ifndef X_DISPLAY_MISSING
/**
 * Set the X display to use for rendering of images to drawables
 *
 * You do not need to set this if you do not intend to
 * render an image to an X drawable. If you do you will need to set
 * this. If you change displays just set this to the new display
 * pointer. Do not use a Display pointer if you have closed that
 * display already - also note that if you close a display connection
 * and continue to render using Imlib2 without setting the display
 * pointer to NULL or something new, crashes may occur.
 *
 * @param display       The Display to use
 */
EAPI void           imlib_context_set_display(Display * display);

/**
 * Return the current display used for Imlib2's display context
 *
 * @return The current display
 */
EAPI Display       *imlib_context_get_display(void);

/**
 * Tell Imlib2 that the current display connection has been closed
 *
 * Call when (and only when) you close a display connection but continue
 * using Imlib2 on a different connection.
 */
EAPI void           imlib_context_disconnect_display(void);

/**
 * Set the visual to use when rendering images to drawables
 *
 * You need to set this for anything to render to a drawable or produce pixmaps
 * (this can be the default visual).
 *
 * @param visual        The Visual to use
 */
EAPI void           imlib_context_set_visual(Visual * visual);

/**
 * Return the current visual
 *
 * @return The current visual
 */
EAPI Visual        *imlib_context_get_visual(void);

/**
 * Set the colormap to use when rendering to drawables and allocating
 * colors
 *
 * You must set this to render any images or produce pixmaps
 * (this can be the default colormap).
 *
 * @param colormap      The Colormap to use
 */
EAPI void           imlib_context_set_colormap(Colormap colormap);

/**
 * Return the current Colormap
 *
 * @return The current colormap
 */
EAPI Colormap       imlib_context_get_colormap(void);

/**
 * Set the X drawable to which images will be rendered
 *
 * This may be either a pixmap or a window. You must set this to render anything.
 *
 * @param drawable      An X Drawable
 */
EAPI void           imlib_context_set_drawable(Drawable drawable);

/**
 * Return the current Drawable
 *
 * @return The current drawable
 */
EAPI Drawable       imlib_context_get_drawable(void);

/**
 * Set the 1-bit deep pixmap to be drawn to when generating a mask pixmap
 *
 * This is only useful if the image you are rendering has alpha.
 * Set this to 0 to not render a pixmap mask.
 *
 * @param mask          A pixmap with depth 1
 */
EAPI void           imlib_context_set_mask(Pixmap mask);

/**
 * Return the current mask pixmap
 *
 * @return The current pixmap
 */
EAPI Pixmap         imlib_context_get_mask(void);

/**
 * Set mask dithering mode
 *
 * When rendering to a mask or producing pixmap masks from images
 * dither_mask selects if the mask is to be dithered or not.
 * Passing in 1 for dither_mask means the mask pixmap will be dithered,
 * 0 means it will not be dithered.
 *
 * @param dither_mask   The dither mask flag
 */
EAPI void           imlib_context_set_dither_mask(char dither_mask);

/**
 * Return the current mask dithering mode
 *
 * @return The current dither mask flag
 */
EAPI char           imlib_context_get_dither_mask(void);

/**
 * Set mask alpha threshold
 *
 * When rendering to a mask or producing pixmap masks from images
 * mask_alpha_threshold selects the alpha threshold above which mask bits are set.
 * The default mask alpha threshold is 128, meaning that a mask bit will be set
 * if the pixel alpha is >= 128.
 *
 * @param mask_alpha_threshold The mask alpha threshold
 */
EAPI void           imlib_context_set_mask_alpha_threshold(int
                                                           mask_alpha_threshold);

/**
 * Return the current mask alpha threshold
 *
 * @return The current mask mask alpha threshold
 */
EAPI int            imlib_context_get_mask_alpha_threshold(void);

#endif /* X_DISPLAY_MISSING */

/**
 * Set "anti-aliasing" mode when scaling images
 *
 * This isn't quite correct since it's actually super and sub pixel
 * sampling that it turns on and off, but anti-aliasing is used for
 * having "smooth" edges to lines and shapes and this means when
 * images are scaled they will keep their smooth appearance.
 * Passing in 1 turns this on and 0 turns it off.
 *
 * @param anti_alias    The anti-alias flag
 */
EAPI void           imlib_context_set_anti_alias(char anti_alias);

/**
 * Return the current "anti-aliasing" mode
 *
 * @return The current anti alias flag
 */
EAPI char           imlib_context_get_anti_alias(void);

/**
 * Set dithering mode
 *
 * Sets the dithering flag for rendering to a drawable or when pixmaps
 * are produced.
 * This affects the color image appearance. Dithering slows down rendering but
 * produces considerably better results.
 * This option has no effect for rendering in 24 bit and up, but in 16 bit and
 * lower it will dither, producing smooth gradients and much better quality
 * images.
 * Setting dither to 1 enables dithering and 0 disables dithering.
 *
 * @param dither        The dithering flag
 */
EAPI void           imlib_context_set_dither(char dither);

/**
 * Return the current dithering mode
 *
 * @return The current dithering flag
 */
EAPI char           imlib_context_get_dither(void);

/**
 * Set blending mode
 *
 * Select whether to blend or not during rendering operations.
 * Setting blend to 1 enables blending and 0 disables blending.
 *
 * @param blend         The blending flag
 */
EAPI void           imlib_context_set_blend(char blend);

/**
 * Return current blending mode
 *
 * @return The current blending flag
 */
EAPI char           imlib_context_get_blend(void);

/**
 * Set the color modifier
 *
 * The color modifier is used when rendering pixmaps or images to a drawable
 * or images onto other images.
 *
 * Color modifiers are lookup tables that map the values in the red, green,
 * blue and alpha channels to other values in the same channel when rendering,
 * allowing for fades, color correction etc. to be done whilst
 * rendering.
 * Pass in NULL as the color_modifier to disable the color modifier for
 * rendering.
 *
 * @param color_modifier The color modifier
 */
EAPI void           imlib_context_set_color_modifier(Imlib_Color_Modifier
                                                     color_modifier);

/**
 * Return the current color modifier
 *
 * @return The current color modifier
 */
EAPI Imlib_Color_Modifier imlib_context_get_color_modifier(void);

/**
 * Set drawing operation
 *
 * When Imlib2 draws an image onto another or an image onto a drawable
 * it is able to do more than just blend the result on using the given
 * alpha channel of the image. It is also able to do saturating
 * additive, subtractive and a combination of the both (called reshade)
 * rendering.
 * The default mode is IMLIB_OP_COPY. you can also set it to
 * IMLIB_OP_ADD, IMLIB_OP_SUBTRACT or IMLIB_OP_RESHADE.
 * IMLIB_OP_COPY performs basic alpha blending:
 * DST = (SRC * A) + (DST * (1 - A)).
 * IMLIB_OP_ADD does DST = DST + (SRC * A).
 * IMLIB_OP_SUBTRACT does DST = DST - (SRC * A)
 * IMLIB_OP_RESHADE does DST = DST + (((SRC - * 0.5) / 2) * A).
 *
 * @param operation     The drawing operation
 */
EAPI void           imlib_context_set_operation(Imlib_Operation operation);

/**
 * Return the current operation mode
 *
 * @return The current operation mode
 */
EAPI Imlib_Operation imlib_context_get_operation(void);

/**
 * Set the font to use when rendering text
 *
 * The font should be loaded first with imlib_load_font().
 *
 * @param font          The font
 */
EAPI void           imlib_context_set_font(Imlib_Font font);

/**
 * Return the current font
 *
 * @return The current font
 */
EAPI Imlib_Font     imlib_context_get_font(void);

/**
 * Set the text drawing direction
 *
 * Sets the direction in which to draw text in terms of simple 90
 * degree orientations or an arbitrary angle. The direction can be one
 * of IMLIB_TEXT_TO_RIGHT, IMLIB_TEXT_TO_LEFT, IMLIB_TEXT_TO_DOWN,
 * IMLIB_TEXT_TO_UP or IMLIB_TEXT_TO_ANGLE. The default is
 * IMLIB_TEXT_TO_RIGHT. If you use IMLIB_TEXT_TO_ANGLE, you will also
 * have to set the angle with imlib_context_set_angle().
 *
 * @param direction     The text direction
 */
EAPI void           imlib_context_set_direction(Imlib_Text_Direction direction);

/**
 * Return the current direction to render text in
 *
 * @return The current text rendering direction
 */
EAPI Imlib_Text_Direction imlib_context_get_direction(void);

/**
 * Set the text drawing angle
 *
 * Sets the angle at which text strings will be drawn if the text
 * direction has been set to IMLIB_TEXT_TO_ANGLE with
 * imlib_context_set_direction().
 *
 * @param angle         Angle of the text strings
 */
EAPI void           imlib_context_set_angle(double angle);

/**
 * Return the current angle used to render text in
 *
 * Used if the direction is IMLIB_TEXT_TO_ANGLE.
 *
 * @return The current text rendering angle
 */
EAPI double         imlib_context_get_angle(void);

/**
 * Set the drawing color in RGBA space
 *
 * Sets the color with which text and lines are drawn when
 * being rendered onto an image. Values for @p red, @p green, @p blue
 * and @p alpha are between 0 and 255
 * - any other values have undefined results.
 *
 * @param red           Red channel of the color
 * @param green         Green channel of the color
 * @param blue          Blue channel of the color
 * @param alpha         Alpha channel of the color
 */
EAPI void           imlib_context_set_color(int red, int green, int blue,
                                            int alpha);

/**
 * Return the current drawing color in RGBA space
 *
 * @param red           Red channel of the current color
 * @param green         Green channel of the current color
 * @param blue          Blue channel of the current color
 * @param alpha         Alpha channel of the current color
 */
EAPI void           imlib_context_get_color(int *red, int *green, int *blue,
                                            int *alpha);

/**
 * Set the drawing color in HSVA space
 *
 * Values for @p hue are between 0 and 360, values for @p saturation and @p
 * value between 0 and 1, and values for @p alpha are between 0 and 255
 * - any other values have undefined results.
 *
 * @param hue           Hue channel of the color
 * @param saturation    Saturation channel of the color
 * @param value         Value channel of the color
 * @param alpha         Alpha channel of the color
 */
EAPI void           imlib_context_set_color_hsva(float hue, float saturation,
                                                 float value, int alpha);

/**
 * Return the current drawing color in HSVA space
 *
 * @param hue           Hue channel of the current color
 * @param saturation    Saturation channel of the current color
 * @param value         Value channel of the current color
 * @param alpha         Alpha channel of the current color
 */
EAPI void           imlib_context_get_color_hsva(float *hue, float *saturation,
                                                 float *value, int *alpha);

/**
 * Set the drawing color in HLSA space
 *
 * Values for @p hue are between 0 and 360, values for @p lightness and @p
 * saturation between 0 and 1, and values for @p alpha are between 0 and 255
 * - any other values have undefined results.
 *
 * @param hue           Hue channel of the color
 * @param lightness     Lightness channel of the color
 * @param saturation    Saturation channel of the color
 * @param alpha         Alpha channel of the color
 */
EAPI void           imlib_context_set_color_hlsa(float hue, float lightness,
                                                 float saturation, int alpha);

/**
 * Return the current drawing color in HLSA space
 *
 * @param hue           Hue channel of the current color
 * @param lightness     Lightness channel of the current color
 * @param saturation    Saturation channel of the current color
 * @param alpha         Alpha channel of the current color
 */
EAPI void           imlib_context_get_color_hlsa(float *hue, float *lightness,
                                                 float *saturation, int *alpha);

/**
 * Set the drawing color in CMYA space
 *
 * Values for @p cyan, @p magenta, @p yellow and @p alpha are between 0 and 255
 * - any other values have undefined results.
 *
 * @param cyan          Cyan channel of the color
 * @param magenta       Magenta channel of the color
 * @param yellow        Yellow channel of the color
 * @param alpha         Alpha channel of the color
 */
EAPI void           imlib_context_set_color_cmya(int cyan, int magenta,
                                                 int yellow, int alpha);

/**
 * Return the current drawing color in CMYA space
 *
 * @param cyan          Cyan channel of the current color
 * @param magenta       Magenta channel of the current color
 * @param yellow        Yellow channel of the current color
 * @param alpha         Alpha channel of the current color
 */
EAPI void           imlib_context_get_color_cmya(int *cyan, int *magenta,
                                                 int *yellow, int *alpha);

/**
 * Return the current drawing color as a color struct
 *
 * Do NOT free this pointer.
 *
 * @return The current color
 */
EAPI Imlib_Color   *imlib_context_get_imlib_color(void);

/**
 * Set the color range to use for rendering gradients
 *
 * @param color_range   Color range
 */
EAPI void           imlib_context_set_color_range(Imlib_Color_Range
                                                  color_range);

/**
 * Return the current color range being used for gradients
 *
 * @return The current color range
 */
EAPI Imlib_Color_Range imlib_context_get_color_range(void);

/**
 * Set the image data memory management function
 *
 * @param memory_function The image data memory management function
 */
EAPI void           imlib_context_set_image_data_memory_function
   (Imlib_Image_Data_Memory_Function memory_function);

/**
 * Return the current image data memeory management function
 *
 * @return The image data memory management function
 */
EAPI                Imlib_Image_Data_Memory_Function
imlib_context_get_image_data_memory_function(void);

/**
 * Set the progress function to be called back whilst loading images
 *
 * Set this to the function to be called, or set it to NULL to
 * disable progress callbacks.
 *
 * @param progress_function The progress function
 */
EAPI void           imlib_context_set_progress_function(Imlib_Progress_Function
                                                        progress_function);

/**
 * Return the current progress function
 *
 * @return The current progress function
 */
EAPI Imlib_Progress_Function imlib_context_get_progress_function(void);

/**
 * Set progress callback granularity
 *
 * This hints as to how often to call the progress callback. 0 means
 * as often as possible. 1 means whenever 15 more of the image has been
 * decoded, 10 means every 10% of the image decoding, 50 means every
 * 50% and 100 means only call at the end.
 * Values outside of the range 0-100 are undefined.
 *
 * @param progress_granularity The progress granularity
 */
EAPI void           imlib_context_set_progress_granularity(char
                                                           progress_granularity);

/**
 * Return the current progress granularity
 *
 * @return The current progress granularity
 */
EAPI char           imlib_context_get_progress_granularity(void);

/**
 * Set the image Imlib2 will be using with its function calls
 *
 * @param image         The image
 */
EAPI void           imlib_context_set_image(Imlib_Image image);

/**
 * Return the current context image
 *
 * @return The current image
 */
EAPI Imlib_Image    imlib_context_get_image(void);

/**
 * Set the clip rectangle
 *
 * @param x             The top left x coordinate of the rectangle
 * @param y             The top left y coordinate of the rectangle
 * @param w             The width of the rectangle
 * @param h             The height of the rectangle
 */
EAPI void           imlib_context_set_cliprect(int x, int y, int w, int h);

/**
 * Return the current clip rectangle
 *
 * @param x             The top left x coordinate of the rectangle
 * @param y             The top left y coordinate of the rectangle
 * @param w             The width of the rectangle
 * @param h             The height of the rectangle
 */
EAPI void           imlib_context_get_cliprect(int *x, int *y, int *w, int *h);

/**
 * Return the current size of the image cache in bytes
 *
 * The cache is a unified cache used for image data AND pixmaps.
 *
 * @return The current image cache memory usage
 */
EAPI int            imlib_get_cache_used(void);

/**
 * Return the current maximum size of the image cache in bytes
 *
 * The cache is a unified cache used for image data AND pixmaps.
 *
 * @return The current image cache max size
 */
EAPI int            imlib_get_cache_size(void);

/**
 * Set the cache size
 *
 * The size is in bytes. Setting the cache size to 0 effectively flushes
 * the cache and keeps the cache size at 0 until set to another value.
 * Whenever you set the cache size Imlib2 will flush as many old images
 * and pixmap from the cache as needed until the current cache usage is
 * less than or equal to the cache size.
 *
 * @param bytes         Image cache max size
 */
EAPI void           imlib_set_cache_size(int bytes);

#ifndef X_DISPLAY_MISSING
/**
 * Get the maximum number of colors Imlib2 is allowed to allocate
 *
 * The default is 256.
 *
 * @return The current maximum number of colors
 */
EAPI int            imlib_get_color_usage(void);

/**
 * Set the maximum number of colors Imlib2 is allowed to allocate
 *
 * The default is 256. This has no effect in depths greater than 8 bit.
 *
 * @param max           Maximum number of colors
 */
EAPI void           imlib_set_color_usage(int max);

/**
 * Convenience function that returns the depth of a visual
 *
 * @param display       The display
 * @param visual        The visual
 */
EAPI int            imlib_get_visual_depth(Display * display, Visual * visual);

/**
 * Return the visual that Imlib2 thinks will give you the best quality output
 *
 * If not NULL @p depth_return will return the depth of the visual.
 *
 * @param display       The display
 * @param screen        The screen
 * @param depth_return  The depth of the returned visual
 *
 * @return The best visual
 */
EAPI Visual        *imlib_get_best_visual(Display * display, int screen,
                                          int *depth_return);
#endif /* X_DISPLAY_MISSING */

/**
 * Flush loader cache
 *
 * If you want Imlib2 to forcibly flush any cached loaders it has and
 * re-load them from disk (this is useful if the program just
 * installed a new loader and does not want to wait till Imlib2 deems
 * it an optimal time to rescan the loaders)
 */
EAPI void           imlib_flush_loaders(void);

/**
 * Get error code from previous imlib function call
 *
 * The error code is set in function calls that involve direct or indirect
 * image loading or saving.
 *
 * @return error code
 *          0: Success,
 *   positive: Regular errnos,
 *   negative: IMLIB_ERR_... values, see above
 */
EAPI int            imlib_get_error(void);

/**
 * Load an image from file (header only)
 *
 * Please see the section \ref loading for more detail.
 *
 * @param file          File name
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_load_image(const char *file);

/**
 * Load an image from file "immediately" (header and data)
 *
 * This forces the image data to be decoded at load time instead of decoding
 * being deferred until it is needed.
 *
 * @param file          File name
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_load_image_immediately(const char *file);

/**
 * Load an image without looking in the cache first
 *
 * @param file          File name
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_load_image_without_cache(const char *file);

/**
 * Load an image "immediately" without looking in the cache first
 *
 * @param file          File name
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_load_image_immediately_without_cache(const char
                                                               *file);

/**
 * Load an image from file with error return
 *
 * The image is loaded without deferred image data decoding.
 *
 * On error @p error_return is set to the detail of the error.
 * error values:
 *          0: Success,
 *   positive: Regular errnos,
 *   negative: IMLIB_ERR_... values, see above
 *
 * @param file          File name
 * @param error_return  The returned error
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_load_image_with_errno_return(const char *file,
                                                       int *error_return);

/**
 * Read an image from file descriptor
 *
 * The file name @p file is only used to guess the file format.
 * The image is loaded without deferred image data decoding and without
 * looking in the cache.
 *
 * @p fd must be mmap'able (so it cannot be a pipe).
 * @p fd will be closed after calling this function.
 *
 * @param fd            Image file descriptor
 * @param file          File name
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_load_image_fd(int fd, const char *file);

/**
 * Read an image from memory
 *
 * The file name @p file is only used to guess the file format.
 * The image is loaded without deferred image data decoding and without
 * looking in the cache.
 *
 * @param file          File name
 * @param data          Image data
 * @param size          Image data size
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_load_image_mem(const char *file,
                                         const void *data, size_t size);

/**
 * Free the current image
 */
EAPI void           imlib_free_image(void);

/**
 * Free the current image AND remove it from the cache
 */
EAPI void           imlib_free_image_and_decache(void);

/*--------------------------------
 * Query/modify image parameters
 */

/**
 * Get the width in pixels of the current image
 */
EAPI int            imlib_image_get_width(void);

/**
 * Get the height in pixels of the current image
 */
EAPI int            imlib_image_get_height(void);

/**
 * Get the filename for the current image
 *
 * The pointer returned is only valid as long as no operations cause
 * the filename of the image to change. Saving the file with a
 * different name would cause this. It is suggested you duplicate the
 * string if you wish to continue to use the string for later
 * processing.
 * Do not free the string pointer returned by this function.
 *
 * @return The current image filename
 */
EAPI const char    *imlib_image_get_filename(void);

/**
 * Get a pointer to the image data for the current image
 *
 * When you get this pointer it is assumed you are planning on writing
 * to the data, thus once you do this the image can no longer be used for
 * caching - in fact all images cached from this one will also be affected
 * when you put the data back. If this matters it is suggested you clone
 * the image first before playing with the image data.
 * The image data is returned in the format of a uint32_t (32 bits) per
 * pixel in a linear array ordered from the top left of the image to the
 * bottom right going from left to right each line.
 * Each pixel has the upper 8 bits as the alpha channel and the lower 8 bits
 * are the blue channel - so a pixel's bits are ARGB (from
 * most to least significant, 8 bits per channel).
 * You must put the data back at some point.
 *
 * @return A pointer to the image data
 */
EAPI uint32_t      *imlib_image_get_data(void);

/**
 * Get a pointer to the image data for the current image (read-only)
 *
 * Functions the same way as imlib_image_get_data(), but returns a
 * pointer expecting the program to NOT write to the data returned (it
 * is for inspection purposes only). Writing to this data has undefined
 * results.
 * The data does not need to be put back.
 *
 * @return A pointer to the image data
 */
EAPI uint32_t      *imlib_image_get_data_for_reading_only(void);

/**
 * Put back @p data obtained by imlib_image_get_data().
 *
 * @p data must be returned by imlib_image_get_data().
 *
 * @param data          The pointer to the image data
 */
EAPI void           imlib_image_put_back_data(uint32_t * data);

/**
 * Return whether or not the current image has an alpa channel
 *
 * Returns 1 if the current context image has an alpha channel, or 0
 * if it does not.
 *
 * @return Current alpha channel flag
 */
EAPI char           imlib_image_has_alpha(void);

/**
 * Force timestamp check on next load
 *
 * By default Imlib2 will not check the timestamp of an image on disk
 * and compare it with the image in its cache - this is to minimize
 * disk activity when using the cache. Call this function and it will
 * flag the current context image as being liable to change on disk
 * and Imlib2 will check the timestamp of the image file on disk and
 * compare it with the cached image when it next needs to use this
 * image in the cache.
 */
EAPI void           imlib_image_set_changes_on_disk(void);

/**
 * Get image border
 *
 * The border is the area at the edge of the image that does not scale
 * with the rest of the image when resized - the borders remain constant
 * in size. This is useful for scaling bevels at the edge of images
 * differently to the image center.
 *
 * @param border        Returns the image border
 */
EAPI void           imlib_image_get_border(Imlib_Border * border);

/**
 * Set image border
 *
 * @param border        Image border
 */
EAPI void           imlib_image_set_border(Imlib_Border * border);

/**
 * Set image format
 *
 * This formt is used when you wish to save an image in a different
 * format that it was loaded in, or if the image currently has no
 * file format associated with it.
 *
 * @param format        Image format
 */
EAPI void           imlib_image_set_format(const char *format);

/**
 * Set if the format value of the current image is irrelevant for
 * caching purposes
 *
 * By default it is. Pass irrelevant as 1 to make it
 * irrelevant and 0 to make it relevant for caching.
 *
 * @param irrelevant    Irrelevant format flag
 */
EAPI void           imlib_image_set_irrelevant_format(char irrelevant);

/**
 * Return the current image's format
 *
 * Do not free this string. Duplicate it if you need it for later use.
 *
 * @return Current image format
 */
EAPI char          *imlib_image_format(void);

/**
 * Set the alpha flag for the current image
 *
 * Set @p has_alpha to 1 to enable the alpha channel or 0 to disable it.
 *
 * @param has_alpha     Alpha flag
 */
EAPI void           imlib_image_set_has_alpha(char has_alpha);

/**
 * Get image pixel value at given location
 *
 * Fills the @p color_return color structure with the color of the pixel
 * in the current image that is at the (@p x, @p y) location.
 *
 * @param x             The x coordinate of the pixel
 * @param y             The y coordinate of the pixel
 * @param color_return  The returned color
 */
EAPI void           imlib_image_query_pixel(int x, int y,
                                            Imlib_Color * color_return);

/**
 * Get image pixel value at given location - HSVA space
 *
 * @param x             The x coordinate of the pixel
 * @param y             The y coordinate of the pixel
 * @param hue           The returned hue channel
 * @param saturation    The returned saturation channel
 * @param value         The returned value channel
 * @param alpha         The returned alpha channel
 */
EAPI void           imlib_image_query_pixel_hsva(int x, int y,
                                                 float *hue, float *saturation,
                                                 float *value, int *alpha);

/**
 * Get image pixel value at given location - HLSA space
 *
 * @param x             The x coordinate of the pixel
 * @param y             The y coordinate of the pixel
 * @param hue           The returned hue channel
 * @param lightness     The returned lightness channel
 * @param saturation    The returned saturation channel
 * @param alpha         The returned alpha channel
 */
EAPI void           imlib_image_query_pixel_hlsa(int x, int y,
                                                 float *hue, float *lightness,
                                                 float *saturation, int *alpha);

/**
 * Get image pixel value at given location - CMYA space
 *
 * @param x             The x coordinate of the pixel
 * @param y             The y coordinate of the pixel
 * @param cyan          The returned cyan channel
 * @param magenta       The returned magenta channel
 * @param yellow        The returned yellow channel
 * @param alpha         The returned alpha channel
 */
EAPI void           imlib_image_query_pixel_cmya(int x, int y,
                                                 int *cyan, int *magenta,
                                                 int *yellow, int *alpha);

/*--------------------------------
 * Rendering functions
 */

#ifndef X_DISPLAY_MISSING
/**
 * Create pixmap/mask of the current image
 *
 * The mask is generated if the image has an alpha value.
 *
 * The pixmaps must be freed using imlib_free_pixmap_and_mask().
 *
 * @param pixmap_return The returned pixmap
 * @param mask_return   The returned mask
 */
EAPI void           imlib_render_pixmaps_for_whole_image(Pixmap * pixmap_return,
                                                         Pixmap * mask_return);

/**
 * Create pixmap/mask of the current image at given size
 *
 * Works just like imlib_render_pixmaps_for_whole_image(), but will
 * scale the output result to the width @p width and height @p height
 * specified.
 * Scaling is done before depth conversion so pixels used for dithering don't
 * grow large.
 *
 * @param pixmap_return The returned pixmap
 * @param mask_return   The returned mask
 * @param width         Width of the pixmap
 * @param height        Height of the pixmap
 */
EAPI void           imlib_render_pixmaps_for_whole_image_at_size(Pixmap *
                                                                 pixmap_return,
                                                                 Pixmap *
                                                                 mask_return,
                                                                 int width,
                                                                 int height);

/**
 * Free @p pixmap and any associated mask
 *
 * The pixmap will remain cached until the image the pixmap was generated from
 * is dirtied or decached, or the cache is flushed.
 *
 * @param pixmap        The pixmap
 */
EAPI void           imlib_free_pixmap_and_mask(Pixmap pixmap);

/**
 * Render image onto drawable
 *
 * Renders the current image onto the current drawable at the (@p x, @p y)
 * pixel location specified without scaling.
 *
 * @param x             X coordinate of the pixel
 * @param y             Y coordinate of the pixel
 */
EAPI void           imlib_render_image_on_drawable(int x, int y);

/**
 * Render image onto drawable at given size
 *
 * Renders the current image onto the current drawable at the (@p x, @p y)
 * location specified AND scale the image to the width @p width and height
 * @p height.
 *
 * @param x             X coordinate of the pixel
 * @param y             Y coordinate of the pixel
 * @param width         Width of the rendered image
 * @param height        Height of the rendered image
 */
EAPI void           imlib_render_image_on_drawable_at_size(int x, int y,
                                                           int width,
                                                           int height);

/**
 * Render part of image onto drawable at given size
 *
 * Renders the source (@p src_x, @p src_y, @p src_width, @p src_height)
 * pixel rectangle from the current image onto the current drawable at the
 * (@p dst_x, @p dst_y) location scaled to the width @p dst_width and
 * height @p dst_height.
 *
 * @param src_x         X coordinate of the source image
 * @param src_y         Y coordinate of the source image
 * @param src_width     Width of the source image
 * @param src_height    Height of the source image
 * @param dst_x         X coordinate of the destination image
 * @param dst_y         Y coordinate of the destination image
 * @param dst_width     Width of the destination image
 * @param dst_height    Height of the destination image
 */
EAPI void           imlib_render_image_part_on_drawable_at_size(int src_x,
                                                                int src_y,
                                                                int src_width,
                                                                int src_height,
                                                                int dst_x,
                                                                int dst_y,
                                                                int dst_width,
                                                                int dst_height);

/**
 * Return X11 pixel value of current color
 */
EAPI uint32_t       imlib_render_get_pixel_color(void);

#endif /* X_DISPLAY_MISSING */

/**
 * Blend part of image onto image
 *
 * Blends the source rectangle (@p src_x, @p src_y, @p src_width, @p src_height)
 * from @p src_image onto the current image at the destination (@p dst_x, @p dst_y)
 * location scaled to the width @p dst_width and height @p * dst_height.
 * If @p merge_alpha is set to 1 it will also modify the destination image
 * alpha channel, otherwise the destination alpha channel is left untouched.
 *
 * @param src_image     The source image
 * @param merge_alpha   Alpha flag
 * @param src_x         X coordinate of the source image
 * @param src_y         Y coordinate of the source image
 * @param src_width     Width of the source image
 * @param src_height    Height of the source image
 * @param dst_x         X coordinate of the destination image
 * @param dst_y         Y coordinate of the destination image
 * @param dst_width     Width of the destination image
 * @param dst_height    Height of the destination image
 */
EAPI void           imlib_blend_image_onto_image(Imlib_Image src_image,
                                                 char merge_alpha,
                                                 int src_x, int src_y,
                                                 int src_width, int src_height,
                                                 int dst_x, int dst_y,
                                                 int dst_width, int dst_height);

/*--------------------------------
 * Image creation functions
 */

/**
 * Create a new blank image
 *
 * Creates a new blank image of size @p width and @p height. The contents of
 * this image at creation time are undefined (they could be garbage
 * memory). You are free to do whatever you like with this image. It
 * is not cached.
 *
 * @param width         The width of the image
 * @param height        The height of the image
 *
 * @return A new blank image on success or NULL on failure
 */
EAPI Imlib_Image    imlib_create_image(int width, int height);

/**
 * Clone image
 *
 * Creates an exact duplicate of the current image.
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_clone_image(void);

/**
 * Create cropped image
 *
 * Creates a duplicate of a (@p x, @p y, @p width, @p height) rectangle in the
 * current image.
 *
 * @param x             The top left x coordinate of the rectangle
 * @param y             The top left y coordinate of the rectangle
 * @param width         The width of the rectangle
 * @param height        The height of the rectangle
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_create_cropped_image(int x, int y, int width,
                                               int height);

/**
 * Create cropped and scaled image
 *
 * Works the same as imlib_create_cropped_image() but will scale the
 * new image to the new destination @p dst_width and @p dst_height
 * whilst cropping.
 *
 * @param src_x         The top left x coordinate of the source rectangle
 * @param src_y         The top left y coordinate of the source rectangle
 * @param src_width     The width of the source rectangle
 * @param src_height    The height of the source rectangle
 * @param dst_width     The width of the destination image
 * @param dst_height    The height of the destination image
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_create_cropped_scaled_image(int src_x, int src_y,
                                                      int src_width,
                                                      int src_height,
                                                      int dst_width,
                                                      int dst_height);

/**
 * Create a new image using given pixel data
 *
 * Creates an image from the image data specified with the width @p width and
 * the height @p height specified.
 * The image data @p data must be in the same format as imlib_image_get_data()
 * would return.
 * You are responsible for freeing this image data once the image is freed.
 * This is useful for when you already have static
 * buffers of the same format Imlib2 uses (many video grabbing devices
 * use such a format) and wish to use Imlib2 to render the results
 * onto another image, or X drawable.
 * On success an image handle is returned - otherwise NULL is returned.
 *
 * @param width         The width of the image
 * @param height        The height of the image
 * @param data          The data
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_create_image_using_data(int width, int height,
                                                  uint32_t * data);

/**
 * Create a new image using given pixel data and memory management function
 *
 * Creates an image from the image data specified with the width @p width and
 * the height @p height specified.
 * The image data @p data must be in the same format as imlib_image_get_data()
 * would return.
 * The memory management function @p func is responsible for freeing this image
 * data once the image is freed.
 *
 * @param width         The width of the image
 * @param height        The height of the image
 * @param data          The data
 * @param func          The memory management function
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_create_image_using_data_and_memory_function
   (int width, int height, uint32_t * data,
    Imlib_Image_Data_Memory_Function func);

/**
 * Create a new image using given pixel data
 *
 * Works the same way as imlib_create_image_using_data() but Imlib2
 * copies the image data to the image structure. You may now do
 * whatever you wish with the original data as it will not be needed
 * anymore.
 *
 * @param width         The width of the image
 * @param height        The height of the image
 * @param data          The data
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_create_image_using_copied_data(int width, int height,
                                                         uint32_t * data);

#ifndef X_DISPLAY_MISSING
/**
 * Create image from drawable
 *
 * Return an image (using the mask @p mask to determine the alpha channel)
 * from the current drawable.
 * If @p mask is 0 it will not create a useful alpha channel in the image.
 * If @p mask is 1 the mask will be set to the shape mask of the drawable.
 * It will create an image from the (@p x, @p y, @p width , @p height)
 * rectangle in the drawable.
 * If @p need_to_grab_x is 1 the X Server s grabbed to avoid possible
 * race conditions. Set to 1 unless the server is already grabbed.
 * If you have not already grabbed the server you MUST set this to 1.
 *
 * @param mask          A mask
 * @param x             The top left x coordinate of the rectangle
 * @param y             The top left y coordinate of the rectangle
 * @param width         The width of the rectangle
 * @param height        The height of the rectangle
 * @param need_to_grab_x Grab flag
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_create_image_from_drawable(Pixmap mask,
                                                     int x, int y,
                                                     int width, int height,
                                                     char need_to_grab_x);

/**
 * Create image from XImage
 *
 * @param image         An XImage
 * @param mask          An XImage mask
 * @param x             The top left x coordinate of the rectangle
 * @param y             The top left y coordinate of the rectangle
 * @param width         The width of the rectangle
 * @param height        The height of the rectangle
 * @param need_to_grab_x Grab flag
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_create_image_from_ximage(XImage * image,
                                                   XImage * mask,
                                                   int x, int y,
                                                   int width, int height,
                                                   char need_to_grab_x);

/**
 * Create image from drawable, scaled
 *
 * Creates an image from the current drawable (optionally using the
 * @p mask pixmap specified to determine alpha transparency) and scale
 * the grabbed data first before converting to an actual image (to
 * minimize reads from the frame buffer which can be slow).
 * The source (@p src_x, @p src_y, @p src_width, @p src_height)
 * rectangle will be grabbed, scaled to the destination @p dst_width
 * and @p dst_height, then converted to an image.
 * If @p need_to_grab_x is 1 the X Server s grabbed to avoid possible
 * race conditions. Set to 1 unless the server is already grabbed.
 * If @p get_mask_from_shape and the current drawable is a window its shape
 * is used for determining the alpha channel.
 *
 * @param mask          A mask
 * @param src_x         The top left x coordinate of the rectangle
 * @param src_y         The top left y coordinate of the rectangle
 * @param src_width     The width of the rectangle
 * @param src_height    The height of the rectangle
 * @param dst_width     The width of the returned image
 * @param dst_height    The height of the returned image
 * @param need_to_grab_x Grab flag
 * @param get_mask_from_shape A char
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_create_scaled_image_from_drawable(Pixmap mask,
                                                            int src_x,
                                                            int src_y,
                                                            int src_width,
                                                            int src_height,
                                                            int dst_width,
                                                            int dst_height,
                                                            char need_to_grab_x,
                                                            char get_mask_from_shape);

/**
 * Copy part of drawable to image
 *
 * Grabs a section of the current drawable (optionally using the pixmap @p mask
 * provided as a corresponding mask for that drawable - if @p mask is 0
 * this is not used).
 * If @p mask is 1 the mask will be set to the shape mask of the drawable.
 * It grabs the (@p src_x, @p src_y, @p src_width, @p src_height) rectangle
 * and places it at the destination (@p dst_x, @p dst_y) location in the
 * current image.
 * If @p need_to_grab_x is 1 the X Server s grabbed to avoid possible
 * race conditions. Set to 1 unless the server is already grabbed.
 *
 * @param mask          A mask
 * @param src_x         The top left x coordinate of the rectangle
 * @param src_y         The top left y coordinate of the rectangle
 * @param src_width     The width of the rectangle
 * @param src_height    The height of the rectangle
 * @param dst_x         The x coordinate of the new location
 * @param dst_y         The x coordinate of the new location
 * @param need_to_grab_x Grab flag
 *
 * @return 1 on success, 0 otherwise
 */
EAPI char           imlib_copy_drawable_to_image(Pixmap mask,
                                                 int src_x, int src_y,
                                                 int src_width, int src_height,
                                                 int dst_x, int dst_y,
                                                 char need_to_grab_x);

/**
 * Return the current number of cached XImages
 *
 * @return The current number of cached XImages
 */
EAPI int            imlib_get_ximage_cache_count_used(void);

/**
 * Return the maximum number of XImages to cache
 *
 * @return The current XImage cache max count
 */
EAPI int            imlib_get_ximage_cache_count_max(void);

/**
 * Set the maximum number of XImages to cache
 *
 * Setting the cache size to 0 effectively flushes the cache and keeps
 * the cached XImage count at 0 until set to another value.
 * Whenever you set the max count Imlib2 will flush as many old XImages
 * from the cache as possible until the current cached XImage count is
 * less than or equal to the cache max count.
 *
 * @param count         XImage cache max count
 */
EAPI void           imlib_set_ximage_cache_count_max(int count);

/**
 * Return the current XImage cache memory usage
 *
 * @return The current XImage cache memory usage
 */
EAPI int            imlib_get_ximage_cache_size_used(void);

/**
 * Return the XImage cache maximum size
 *
 * @return The current XImage cache max size
 */
EAPI int            imlib_get_ximage_cache_size_max(void);

/**
 * Set the XImage cache maximum size
 *
 * Setting the cache size to 0 effectively flushes the cache and keeps
 * the cache size at 0 until set to another value.
 * Whenever you set the max size Imlib2 will flush as many old XImages
 * from the cache as possible until the current XImage cache usage is
 * less than or equal to the cache max size.
 *
 * @param bytes         XImage cache max size in bytes
 */
EAPI void           imlib_set_ximage_cache_size_max(int bytes);

#endif /* X_DISPLAY_MISSING */

/*--------------------------------
 * Updates - lists of rectangles for storing required update draws
 */

/**
 * Duplicate update list
 *
 * Creates a duplicate of an updates list.
 *
 * @param updates       An updates list
 *
 * @return Duplicate of @p updates
 */
EAPI Imlib_Updates  imlib_updates_clone(Imlib_Updates updates);

/**
 * Append rectangle to updates list
 *
 * Appends an update rectangle to the updates list passed in (if the
 * updates is NULL it will create a new updates list) and returns a
 * handle to the modified updates list (the handle may be modified so
 * only use the new updates handle returned).
 *
 * @param updates       An updates list
 * @param x             The top left x coordinate of the rectangle
 * @param y             The top left y coordinate of the rectangle
 * @param w             The width of the rectangle
 * @param h             The height of the rectangle
 *
 * @return The updates handle
 */
EAPI Imlib_Updates  imlib_update_append_rect(Imlib_Updates updates,
                                             int x, int y, int w, int h);

/**
 * Merge updates list
 *
 * Takes an updates list, and modifies it by merging overlapped
 * rectangles and lots of tiny rectangles into larger rectangles to
 * minimize the number of rectangles in the list for optimized
 * redrawing.
 * The new updates handle is now valid and the old one passed in is not.
 *
 * @param updates       An updates list
 * @param w             The width of the rectangle
 * @param h             The height of the rectangle
 *
 * @return The updates handle
 */
EAPI Imlib_Updates  imlib_updates_merge(Imlib_Updates updates, int w, int h);

/**
 * Merge updates list
 *
 * Works almost exactly as imlib_updates_merge() but is more lenient
 * on the spacing between update rectangles - if they are very close it
 * amalgamates 2 smaller rectangles into 1 larger one.
 *
 * @param updates       An updates list
 * @param w             The width of the rectangle
 * @param h             The height of the rectangle
 *
 * @return The updates handle
 */
EAPI Imlib_Updates  imlib_updates_merge_for_rendering(Imlib_Updates updates,
                                                      int w, int h);

/**
 * Free updates list
 *
 * @param updates       An updates list
 */
EAPI void           imlib_updates_free(Imlib_Updates updates);

/**
 * Get the next update in an updates list
 *
 * @param updates       An updates list
 *
 * @return The next updates
 */
EAPI Imlib_Updates  imlib_updates_get_next(Imlib_Updates updates);

/**
 * Return the coordinates of first update in update list
 *
 * @param updates       An updates list
 * @param x_return      The top left x coordinate of the update
 * @param y_return      The top left y coordinate of the update
 * @param width_return  The width of the update
 * @param height_return The height of the update
 */
EAPI void           imlib_updates_get_coordinates(Imlib_Updates updates,
                                                  int *x_return,
                                                  int *y_return,
                                                  int *width_return,
                                                  int *height_return);

/**
 * Modify the coordinates of the first update in update list
 *
 * @param updates       An updates list
 * @param x             The top left x coordinate of the update
 * @param y             The top left y coordinate of the update
 * @param width         The width of the update
 * @param height        The height of the update
 */
EAPI void           imlib_updates_set_coordinates(Imlib_Updates updates,
                                                  int x, int y,
                                                  int width, int height);

#ifdef BUILD_X11
/**
 * Render updates on drawable
 *
 * Given an updates list (preferable already merged for rendering)
 * this will render the corresponding parts of the image to the current
 * drawable at an offset of @p x, @p y.
 *
 * @param updates       An updates list
 * @param x             The top left x coordinate of the update
 * @param y             The top left y coordinate of the update
 */
EAPI void           imlib_render_image_updates_on_drawable(Imlib_Updates
                                                           updates,
                                                           int x, int y);
#endif

/**
 * Return empty updates list
 *
 * Initializes an updates list before you add any updates to it or
 * merge it for rendering etc.
 *
 * @return The initialized updates list
 */
EAPI Imlib_Updates  imlib_updates_init(void);

/**
 * Append update list to update list
 *
 * Appends @p appended_updates to the updates list @p updates and
 * returns the new list.
 *
 * @param updates       An updates list
 * @param appended_updates The updates list to append
 *
 * @return The new updates list
 */
EAPI Imlib_Updates  imlib_updates_append_updates(Imlib_Updates updates,
                                                 Imlib_Updates
                                                 appended_updates);

/*--------------------------------
 * Image modification
 */

/**
 * Flip/mirror the current image horizontally
 */
EAPI void           imlib_image_flip_horizontal(void);

/**
 * Flip/mirror the current image vertically
 */
EAPI void           imlib_image_flip_vertical(void);

/**
 * Flip/mirror the current image diagonally
 *
 * Good for quick and dirty 90 degree rotations if used before or after a
 * horizontal or vertical flip.
 */
EAPI void           imlib_image_flip_diagonal(void);

/**
 * Rotate the current image
 *
 * Performs 90 degree rotations on the current image.
 * 0 does not rotate, 1 rotates clockwise by 90 degree, 2,
 * rotates clockwise by 180 degrees, 3 rotates clockwise by 270
 * degrees.
 *
 * @param orientation   The orientation
 */
EAPI void           imlib_image_orientate(int orientation);

/**
 * Blur the current image
 *
 * A @p radius value of 0 has no effect, 1 and above determine the blur matrix
 * radius that determine how much to blur the image.
 *
 * @param radius        The radius
 */
EAPI void           imlib_image_blur(int radius);

/**
 * Sharpen the current image
 *
 * The @p radius value affects how much to sharpen by.
 *
 * @param radius        The radius
 */
EAPI void           imlib_image_sharpen(int radius);

/**
 * Modify current image for horizontal tiling
 *
 * Modifies an image so it will tile seamlessly horizontally if used
 * as a tile (i.e. drawn multiple times horizontally).
 */
EAPI void           imlib_image_tile_horizontal(void);

/**
 * Modify current image for vertical tiling
 *
 * Modifies an image so it will tile seamlessly vertically if used as
 * a tile (i.e. drawn multiple times vertically).
 */
EAPI void           imlib_image_tile_vertical(void);

/**
 * Modify current image for horizontal and vertical tiling
 *
 * Modifies an image so it will tile seamlessly horizontally and
 * vertically if used as a tile (i.e. drawn multiple times horizontally
 * and vertically).
 */
EAPI void           imlib_image_tile(void);

/**
 * Copy image alpha
 *
 * Copies the alpha channel of the source image @p image_source to the
 * (@p x, @p y) coordinates of the current image, replacing the alpha
 * channel there.
 *
 * @param image_source  An image
 * @param x             The x coordinate
 * @param y             The y coordinate
 */
EAPI void           imlib_image_copy_alpha_to_image(Imlib_Image image_source,
                                                    int x, int y);

/**
 * Copy partial image alpha
 *
 * Copies the source (@p src_x, @p src_y, @p src_width, @p src_height) rectangle
 * alpha channel from the source image @p image_source and replaces the
 * alpha channel on the destination image at the (@p dst_x, @p dst_y)
 * coordinates.
 *
 * @param image_source  An image
 * @param src_x         The top left x coordinate of the rectangle
 * @param src_y         The top left y coordinate of the rectangle
 * @param src_width     The width of the rectangle
 * @param src_height    The height of the rectangle
 * @param dst_x         The top left x coordinate of the destination rectangle
 * @param dst_y         The top left x coordinate of the destination rectangle
 */
EAPI void           imlib_image_copy_alpha_rectangle_to_image(Imlib_Image
                                                              image_source,
                                                              int src_x,
                                                              int src_y,
                                                              int src_width,
                                                              int src_height,
                                                              int dst_x,
                                                              int dst_y);

/**
 * Scroll image
 *
 * Scrolls a rectangle of size @p width, @p height at the (@p x, @p y)
 * location within the current image by the @p delta_x, @p delta_y distance.
 *
 * @param x             The top left x coordinate of the rectangle
 * @param y             The top left y coordinate of the rectangle
 * @param width         The width of the rectangle
 * @param height        The height of the rectangle
 * @param delta_x       Distance along the x coordinates
 * @param delta_y       Distance along the y coordinates
 */
EAPI void           imlib_image_scroll_rect(int x, int y, int width, int height,
                                            int delta_x, int delta_y);

/**
 * Copy image part within image
 *
 * Copies a rectangle of size @p width, @p height at the (@p x, @p y) location
 * specified in the current image to a new location (@p new_x, @p new_y) in the same
 * image.
 *
 * @param x             The top left x coordinate of the rectangle
 * @param y             The top left y coordinate of the rectangle
 * @param width         The width of the rectangle
 * @param height        The height of the rectangle
 * @param new_x         The top left x coordinate of the new location
 * @param new_y         The top left y coordinate of the new location
 */
EAPI void           imlib_image_copy_rect(int x, int y, int width, int height,
                                          int new_x, int new_y);

/*--------------------------------
 * Fonts and text
 */

/**
 * Load a truetype font
 *
 * Loads a truetype font from the first directory in the font path that
 * contains that font. The font name @p font_name format is "font_name/size". For
 * example. If there is a font file called blum.ttf somewhere in the
 * font path you might use "blum/20" to load a 20 pixel sized font of
 * blum. If the font cannot be found NULL is returned.
 *
 * @param font_name     The font name with the size
 *
 * @return NULL if no font found
 */
EAPI Imlib_Font     imlib_load_font(const char *font_name);

/**
 * Unload current font
 */
EAPI void           imlib_free_font(void);

/**
 * Add font to fallback chain
 *
 * This arranges for the given fallback font to be used if a glyph does not
 * exist in the given font when text is being rendered.
 * Fonts can be arranged in an aribitrarily long chain and attempts will be
 * made in order on the chain.
 * Cycles in the chain are not possible since the given fallback font is
 * removed from any chain it's already in.
 * A fallback font may be a member of only one chain. Adding it as the
 * fallback font to another font will remove it from it's first fallback chain.
 *
 * @param font          A previously loaded font
 * @param fallback_font A previously loaded font to be chained to the given font
 *
 * @return 0 on success
 */
EAPI int            imlib_insert_font_into_fallback_chain(Imlib_Font font,
                                                          Imlib_Font
                                                          fallback_font);

/**
 * Remove font from fallback chain
 *
 * This removes the given font from any fallback chain it may be in.
 *
 * @param fallback_font A font previously added to a fallback chain
 */
EAPI void           imlib_remove_font_from_fallback_chain(Imlib_Font
                                                          fallback_font);

/** Return previous font in font fallback chain */
EAPI Imlib_Font     imlib_get_prev_font_in_fallback_chain(Imlib_Font fn);

/** Return next font in font fallback chain */
EAPI Imlib_Font     imlib_get_next_font_in_fallback_chain(Imlib_Font fn);

/**
 * Draw text
 *
 * Draws the null-byte terminated string @p text using the current font on
 * the current image at the (@p x, @p y) location (@p x, @p y denoting the
 * top left corner of the font string)
 *
 * @param x             The x coordinate of the top left corner
 * @param y             The y coordinate of the top left corner
 * @param text          A null-byte terminated string
 */
EAPI void           imlib_text_draw(int x, int y, const char *text);

/**
 * Draw text and return metrics
 *
 * Works just like imlib_text_draw() but also returns the width and
 * height of the string drawn, and @p horizontal_advance_return returns
 * the number of pixels you should advance horizontally to draw another
 * string (useful if you are drawing a line of text word by word) and
 * @p vertical_advance_return does the same for the vertical direction
 * (i.e. drawing text line by line).
 *
 * @param x             The x coordinate of the top left corner
 * @param y             The y coordinate of the top left corner
 * @param text          A null-byte terminated string
 * @param width_return  The width of the string
 * @param height_return The height of the string
 * @param horizontal_advance_return  Horizontal offset
 * @param vertical_advance_return    Vertical offset
 */
EAPI void           imlib_text_draw_with_return_metrics(int x, int y,
                                                        const char *text,
                                                        int *width_return,
                                                        int *height_return,
                                                        int *horizontal_advance_return,
                                                        int *vertical_advance_return);

/**
 * Get text size
 *
 * Gets the width and height in pixels the @p text string would use up
 * if drawn with the current font.
 *
 * @param text          A string
 * @param width_return  The width of the text
 * @param height_return The height of the text
 */
EAPI void           imlib_get_text_size(const char *text,
                                        int *width_return, int *height_return);

/**
 * Get text advances
 *
 * Gets the advance horizontally and vertically in pixels the next
 * text string would need to be placed at for the current font. The
 * advances are not adjusted for rotation so you will have to translate
 * the advances (which are calculated as if the text was drawn
 * horizontally from left to right) depending on the text orientation.
 *
 * @param text          A string
 * @param horizontal_advance_return Horizontal offset
 * @param vertical_advance_return Vertical offset
 */
EAPI void           imlib_get_text_advance(const char *text,
                                           int *horizontal_advance_return,
                                           int *vertical_advance_return);

/**
 * Get text inset
 *
 * Returns the inset of the first character of @p text
 * in using the current font and returns that value in pixels.
 *
 * @param text          A string
 *
 * @return The inset value of @p text
 */
EAPI int            imlib_get_text_inset(const char *text);

/**
 * Add font path
 *
 * Adds the directory @p path to the end of the current list of
 * directories to scan for fonts.
 *
 * @param path          A directory path
 */
EAPI void           imlib_add_path_to_font_path(const char *path);

/**
 * Remove font path
 *
 * Removes a font path from the font path list
 *
 * @param path          A directory path
 */
EAPI void           imlib_remove_path_from_font_path(const char *path);

/**
 * Return font path list
 *
 * Returns a list of strings that are the directories in the font
 * path. Do not free this list or change it in any way. If you add or
 * delete members of the font path this list will be invalid. If you
 * intend to use this list later duplicate it for your own use. The
 * number of elements in the array of strings is put into
 * @p number_return.
 *
 * @param number_return Number of paths in the list
 *
 * @return A list of strings
 */
EAPI char         **imlib_list_font_path(int *number_return);

/**
 * Return character number in text
 *
 * Returns the character number in the string @p text using the current
 * font at the (@p x, @p y) pixel location which is an offset relative to the
 * top left of that string. -1 is returned if there is no character
 * there. If there is a character, @p char_x_return, @p char_y_return,
 * @p char_width_return and @p char_height_return (respectively the
 * character x, y, width and height)  are also filled in.
 *
 * @param text          A string
 * @param x             The x offset
 * @param y             The y offset
 * @param char_x_return The x coordinate of the character
 * @param char_y_return The x coordinate of the character
 * @param char_width_return  The width of the character
 * @param char_height_return The height of the character
 *
 * @return Character number, -1 if no character found
 */
EAPI int            imlib_text_get_index_and_location(const char *text,
                                                      int x, int y,
                                                      int *char_x_return,
                                                      int *char_y_return,
                                                      int *char_width_return,
                                                      int *char_height_return);

/**
 * Get geometry of character in text
 *
 * Gets the geometry of the character at index @p index in the @p text
 * string using the current font.
 *
 * @param text          A string
 * @param index         The character index
 * @param char_x_return The x coordinate of the character
 * @param char_y_return The y coordinate of the character
 * @param char_width_return  The width of the character
 * @param char_height_return The height of the character
 */
EAPI void           imlib_text_get_location_at_index(const char *text,
                                                     int index,
                                                     int *char_x_return,
                                                     int *char_y_return,
                                                     int *char_width_return,
                                                     int *char_height_return);

/**
 * Return a list of fonts imlib2 can find in its font path
 *
 * @param number_return Number of fonts in the list
 *
 * @return A list of fonts
 */
EAPI char         **imlib_list_fonts(int *number_return);

/**
 * Free the font list returned by imlib_list_fonts()
 *
 * @param font_list     The font list
 * @param number        Number of fonts in the list
 */
EAPI void           imlib_free_font_list(char **font_list, int number);

/**
 * Return the font cache size
 *
 * @return The font cache size in bytes
 */
EAPI int            imlib_get_font_cache_size(void);

/**
 * Set the font cache size
 *
 * Whenever you set the font cache size Imlib2 will flush fonts from the
 * cache until the memory used by fonts is less than or equal to the font
 * cache size.
 * Setting the size to 0 effectively frees all speculatively cached fonts.
 *
 * @param bytes         The font cache size in bytes
 */
EAPI void           imlib_set_font_cache_size(int bytes);

/**
 * Flush all speculatively cached fonts from the font cache
 */
EAPI void           imlib_flush_font_cache(void);

/**
 * Return the current font's ascent
 *
 * @return The font's ascent value in pixels
 */
EAPI int            imlib_get_font_ascent(void);

/**
 * Return the current font's descent
 *
 * @return The font's descent value in pixels
 */
EAPI int            imlib_get_font_descent(void);

/**
 * Return the current font's maximum ascent
 *
 * @return The font's maximum ascent
 */
EAPI int            imlib_get_maximum_font_ascent(void);

/**
 * Return the current font's maximum descent
 *
 * @return The font's maximum descent
 */
EAPI int            imlib_get_maximum_font_descent(void);

/*--------------------------------
 * Color modifiers
 */

/**
 * Create empty color modifier
 *
 * Creates a new empty color modifier and returns a
 * valid handle on success. NULL is returned on failure.
 *
 * @return Color modifier handle (NULL on failure)
 */
EAPI Imlib_Color_Modifier imlib_create_color_modifier(void);

/**
 * Free the current color modifier
 */
EAPI void           imlib_free_color_modifier(void);

/**
 * Modify current color modifier gamma
 *
 * Modifies the current color modifier by adjusting the gamma by the
 * value specified @p gamma_value. The color modifier is modified not set, so calling
 * this repeatedly has cumulative effects. A gamma of 1.0 is normal
 * linear, 2.0 brightens and 0.5 darkens etc. Negative values are not
 * allowed.
 *
 * @param gamma_value   Value of gamma
 */
EAPI void           imlib_modify_color_modifier_gamma(double gamma_value);

/**
 * Modify current color modifier brightness
 *
 * Modifies the current color modifier by adjusting the brightness by
 * the value @p brightness_value. The color modifier is modified not set, so
 * calling this repeatedly has cumulative effects. brightness values
 * of 0 do not affect anything. -1.0 will make things completely black
 * and 1.0 will make things all white. Values in-between vary
 * brightness linearly.
 *
 * @param brightness_value Value of brightness
 */
EAPI void           imlib_modify_color_modifier_brightness(double
                                                           brightness_value);

/**
 * Modify current color modifier contrast
 *
 * Modifies the current color modifier by adjusting the contrast by
 * the value @p contrast_value. The color modifier is modified not set, so
 * calling this repeatedly has cumulative effects. Contrast of 1.0 does
 * nothing. 0.0 will merge to gray, 2.0 will double contrast etc.
 *
 * @param contrast_value Value of contrast
 */
EAPI void           imlib_modify_color_modifier_contrast(double
                                                         contrast_value);

/**
 * Set current color modifier tables
 *
 * Explicitly copies the mapping tables from the table pointers passed
 * into this function into those of the current color modifier. Tables
 * are 256 entry arrays of uint8_t which are a mapping of that channel
 * value to a new channel value. A normal mapping would be linear (v[0]
 * = 0, v[10] = 10, v[50] = 50, v[200] = 200, v[255] = 255).
 *
 * @param red_table     A 256 element uint8_t array
 * @param green_table   A 256 element uint8_t array
 * @param blue_table    A 256 element uint8_t array
 * @param alpha_table   A 256 element uint8_t array
 */
EAPI void           imlib_set_color_modifier_tables(uint8_t * red_table,
                                                    uint8_t * green_table,
                                                    uint8_t * blue_table,
                                                    uint8_t * alpha_table);

/**
 * Get current color modifier tables
 *
 * Copies the table values from the current color modifier into the
 * pointers to mapping tables specified. They must have 256 entries and
 * be uint8_t format.
 *
 * @param red_table     A 256 element uint8_t array
 * @param green_table   A 256 element uint8_t array
 * @param blue_table    A 256 element uint8_t array
 * @param alpha_table   A 256 element uint8_t array
 */
EAPI void           imlib_get_color_modifier_tables(uint8_t * red_table,
                                                    uint8_t * green_table,
                                                    uint8_t * blue_table,
                                                    uint8_t * alpha_table);

/**
 * Reset current color modifier tables
 *
 * Resets the current color modifier to have linear mapping tables
 */
EAPI void           imlib_reset_color_modifier(void);

/**
 * Apply color modifier to image
 *
 * Uses the current color modifier mapping tables to modify the current image.
 */
EAPI void           imlib_apply_color_modifier(void);

/**
 * Apply color modifier to image part
 *
 * Works the same way as imlib_apply_color_modifier() but only modifies
 * a selected rectangle in the current image.
 * @param x             The x coordinate of the left edge of the rectangle
 *
 * @param y             The y coordinate of the top edge of the rectangle
 * @param width         The width of the rectangle
 * @param height        The height of the rectangle
 */
EAPI void           imlib_apply_color_modifier_to_rectangle(int x, int y,
                                                            int width,
                                                            int height);

/*--------------------------------
 * Drawing on images
 */

/**
 * Draw a pixel
 *
 * Draws a pixel using the urrent color on the current image at
 * coordinate (@p x1, @p y1).
 * If @p make_updates is 1 it will also return an update you can use for an
 * updates list, otherwise it returns NULL.
 *
 * @param x             The x coordinate of the first point
 * @param y             The y coordinate of the first point
 * @param make_updates  Make updates flag
 *
 * @return An updates list
 */
EAPI Imlib_Updates  imlib_image_draw_pixel(int x, int y, char make_updates);

/**
 * Draw a line
 *
 * Draws a line using the current color on the current image from
 * coordinates (@p x1, @p y1) to (@p x2, @p y2).
 * If @p make_updates is 1 it will also return an update you can use for an
 * updates list, otherwise it returns NULL.
 *
 * @param x1            The x coordinate of the first point
 * @param y1            The y coordinate of the first point
 * @param x2            The x coordinate of the second point
 * @param y2            The y coordinate of the second point
 * @param make_updates: a char
 *
 * @return An updates list
 */
EAPI Imlib_Updates  imlib_image_draw_line(int x1, int y1, int x2, int y2,
                                          char make_updates);

/**
 * Draw a rectangle (no fill)
 *
 * Draws the outline of a rectangle on the current image at the (@p x,
 * @p y)
 * coordinates with a size of @p width and @p height pixels, using the
 * current color.
 *
 * @param x             The top left x coordinate of the rectangle
 * @param y             The top left y coordinate of the rectangle
 * @param width         The width of the rectangle
 * @param height        The height of the rectangle
 */
EAPI void           imlib_image_draw_rectangle(int x, int y,
                                               int width, int height);

/**
 * Draw rectangle (filled)
 *
 * Draws a filled rectangle on the current image at the (@p x, @p y)
 * coordinates with a size of @p width and @p height pixels, using the
 * current color.
 *
 * @param x             The top left x coordinate of the rectangle
 * @param y             The top left y coordinate of the rectangle
 * @param width         The width of the rectangle
 * @param height        The height of the rectangle
 */
EAPI void           imlib_image_fill_rectangle(int x, int y,
                                               int width, int height);

/*--------------------------------
 * Polygons
 */

/**
 * Create empty polygon object
 *
 * @return a new polygon object with no points set
 */
EAPI ImlibPolygon   imlib_polygon_new(void);

/**
 * Free a polygon object.
 *
 * @param poly          A polygon
 */
EAPI void           imlib_polygon_free(ImlibPolygon poly);

/**
 * Add point to polygon
 *
 * Adds the point (@p x, @p y) to the polygon @p poly. The point will be added
 * to the end of the polygon's internal point list. The points are
 * drawn in order, from the first to the last.
 *
 * @param poly          A polygon
 * @param x             The X coordinate
 * @param y             The Y coordinate
 */
EAPI void           imlib_polygon_add_point(ImlibPolygon poly, int x, int y);

/**
 * Draw a polygon (no fill)
 *
 * Draws the polygon @p poly onto the current context image. Points which have
 * been added to the polygon are drawn in sequence, first to last. The
 * final point will be joined with the first point if @p closed is
 * non-zero.
 *
 * @param poly          A polygon
 * @param closed        Closed polygon flag
 */
EAPI void           imlib_image_draw_polygon(ImlibPolygon poly,
                                             unsigned char closed);

/**
 * Draw a polygon (filled)
 *
 * Fills the area defined by the polygon @p polyon the current context image
 * with the current context color.
 *
 * @param poly          A polygon
 */
EAPI void           imlib_image_fill_polygon(ImlibPolygon poly);

/**
 * Get polygon bounds
 *
 * Calculates the bounding area of the polygon @p poly. (@p px1, @p py1) defines the
 * upper left corner of the bounding box and (@p px2, @p py2) defines it's
 * lower right corner.
 *
 * @param poly          A polygon
 * @param px1           X coordinate of the upper left corner
 * @param py1           Y coordinate of the upper left corner
 * @param px2           X coordinate of the lower right corner
 * @param py2           Y coordinate of the lower right corner
 */
EAPI void           imlib_polygon_get_bounds(ImlibPolygon poly,
                                             int *px1, int *py1,
                                             int *px2, int *py2);

/**
 * Check if polygon contains point
 *
 * Returns non-zero if the point (@p x, @p y) is within the area defined by
 * the polygon @p poly.
 *
 * @param poly          A polygon
 * @param x             The X coordinate
 * @param y             The Y coordinate
 *
 * @return non-zero if point is inside polygon
 */
EAPI unsigned char  imlib_polygon_contains_point(ImlibPolygon poly,
                                                 int x, int y);

/*--------------------------------
 * Ellipses
 */

/**
 * Draw an ellipse (no fill)
 *
 * Draws an ellipse on the current context image. The ellipse is
 * defined as (@p x-@p xc)^2/@p a^2 + (@p y-@p yc)^2/@p b^2 = 1. This means that the
 * point (@p xc, @p yc) marks the center of the ellipse, @p a defines the
 * horizontal amplitude of the ellipse, and @p b defines the vertical
 * amplitude.
 *
 * @param xc            X coordinate of the center of the ellipse
 * @param yc            Y coordinate of the center of the ellipse
 * @param a             The horizontal amplitude of the ellipse
 * @param b             The vertical amplitude of the ellipse
 */
EAPI void           imlib_image_draw_ellipse(int xc, int yc, int a, int b);

/**
 * Draw an ellipse (filled)
 *
 * Fills an ellipse on the current context image using the current
 * context color. The ellipse is
 * defined as (@p x-@p xc)^2/@p a^2 + (@p y-@p yc)^2/@p b^2 = 1. This means that the
 * point (@p xc, @p yc) marks the center of the ellipse, @p a defines the
 * horizontal amplitude of the ellipse, and @p b defines the vertical
 * amplitude.
 *
 * @param xc            X coordinate of the center of the ellipse
 * @param yc            Y coordinate of the center of the ellipse
 * @param a             The horizontal amplitude of the ellipse
 * @param b             The vertical amplitude of the ellipse
 */
EAPI void           imlib_image_fill_ellipse(int xc, int yc, int a, int b);

/*--------------------------------
 * Color ranges
 */

/**
 * Create empty color range
 *
 * @return Color range handle (NULL on error)
 */
EAPI Imlib_Color_Range imlib_create_color_range(void);

/**
 * Free the current color range
 */
EAPI void           imlib_free_color_range(void);

/**
 * Add color to color range
 *
 * Adds the current color to the current color range at a @p distance_away
 * distance from the previous color in the range (if it's the first
 * color in the range this is irrelevant).
 *
 * @param distance_away Distance from the previous color
 */
EAPI void           imlib_add_color_to_color_range(int distance_away);

/**
 * Fill image rectangle with color range
 *
 * Fills a rectangle of width @p width and height @p height at the (@p x, @p y) location
 * specified in the current image with a linear gradient of the
 * current color range at an angle of @p angle degrees with 0 degrees
 * being vertical from top to bottom going clockwise from there.
 *
 * @param x             The x coordinate of the left edge of the rectangle
 * @param y             The y coordinate of the top edge of the rectangle
 * @param width         The width of the rectangle
 * @param height        The height of the rectangle
 * @param angle         Angle of gradient
 */
EAPI void           imlib_image_fill_color_range_rectangle(int x, int y,
                                                           int width,
                                                           int height,
                                                           double angle);

/**
 * Fill image rectangle with HSVA color range
 *
 * Fills a rectangle of width @p width and height @p height at the (@p
 * x, @p y) location
 * specified in the current image with a linear gradient in HSVA color
 * space of the current color range at an angle of @p angle degrees with
 * 0 degrees being vertical from top to bottom going clockwise from
 * there.
 *
 * @param x             The x coordinate of the left edge of the rectangle
 * @param y             The y coordinate of the top edge of the rectangle
 * @param width         The width of the rectangle
 * @param height        The height of the rectangle
 * @param angle         Angle of gradient
 */
EAPI void           imlib_image_fill_hsva_color_range_rectangle(int x, int y,
                                                                int width,
                                                                int height,
                                                                double
                                                                angle);

/*--------------------------------
 * Image data attachment
 */

/**
 * Attach data to image
 *
 * Attaches data to the current image with the string key of @p key, and
 * the data of @p data and an integer of @p value.
 * The * @p destructor_function function, if not NULL is called when this
 * image is freed so the destructor can free @p data, if this is needed.
 *
 * @param key           The key
 * @param data          A pointer
 * @param value         A value
 * @param destructor_function An Imlib internal function
 */
EAPI void           imlib_image_attach_data_value(const char *key,
                                                  void *data, int value,
                                                  Imlib_Data_Destructor_Function
                                                  destructor_function);

/**
 * Get attached image data
 *
 * Gets the data attached to the current image with the key @p key
 * specified.
 *
 * @param key           The key
 *
 * @return The attached data as a pointer, or NULL if none
 */
EAPI void          *imlib_image_get_attached_data(const char *key);

/**
 * Get attached image data value
 *
 * Returns the value attached to the current image with the specified
 * key @p key. If none could be found 0 is returned.
 *
 * @param key           The key
 *
 * @return The attached value as an integer, or 0 if none
 */
EAPI int            imlib_image_get_attached_value(const char *key);

/**
 * Detach image data
 *
 * Detaches the data & value attached with the specified key @p key from the
 * current image.
 *
 * @param key           The key
 */
EAPI void           imlib_image_remove_attached_data_value(const char *key);

/**
 * Detach image data
 *
 * Removes the data and value attached to the current image with the
 * specified key @p key and also calls the destructor function that was
 * supplied when attaching it (see imlib_image_attach_data_value()).
 *
 * @param key           The key
 */
EAPI void           imlib_image_remove_and_free_attached_data_value(const char
                                                                    *key);

/*--------------------------------
 * Image saving
 */

/**
 * Save image to file
 *
 * Saves the current image in the format specified by the current
 * image's format setting to the filename @p file.
 * If the image's format is not set, the format is derived from @p file.
 *
 * @param file          The file name
 */
EAPI void           imlib_save_image(const char *file);

/**
 * Save image to file with error return
 *
 * Works the same way imlib_save_image() works, but will set the
 * @p error_return to an error value if the save fails.
 * error values:
 *          0: Success,
 *   positive: Regular errnos,
 *   negative: IMLIB_ERR_... values, see above
 *
 * @param file          The file name
 * @param error_return  The returned error
 */
EAPI void           imlib_save_image_with_errno_return(const char *file,
                                                       int *error_return);

/**
 * Save image to file descriptor
 *
 * Saves the current image in the format specified by the current
 * image's format setting to the file given by @p fd.
 * The file name @p file is used only to derive the file format if the
 * image's format is not set.
 *
 * @p fd will be closed after calling this function.
 *
 * @param fd            Image file descriptor
 * @param file          The file name
 */
EAPI void           imlib_save_image_fd(int fd, const char *file);

/*--------------------------------
 * Image rotation/skewing
 */

/**
 * Create rotated image
 *
 * Creates a new copy of the current image, but rotated by @p angle radians.
 *
 * @param angle         An angle in radians
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_create_rotated_image(double angle);

/**
 * Rotate image onto image
 *
 * Rotate @ src_image onto current image.
 *
 * @param src_image     The source image
 * @param angle         An angle in radians
 */
EAPI void           imlib_rotate_image_from_buffer(double angle,
                                                   Imlib_Image src_image);

/**
 * Blend image onto image at angle
 *
 * Works just like imlib_blend_image_onto_image_skewed() except you
 * cannot skew an image (@p v_angle_x and @p v_angle_y are 0).
 *
 * @param src_image     The image source
 * @param merge_alpha   A char
 * @param src_x         The source x coordinate
 * @param src_y         The source y coordinate
 * @param src_width     The source width
 * @param src_height    The source height
 * @param dst_x         The destination x coordinate
 * @param dst_y         The destination y coordinate
 * @param angle_x       An angle
 * @param angle_y       An angle
 */
EAPI void           imlib_blend_image_onto_image_at_angle(Imlib_Image
                                                          src_image,
                                                          char merge_alpha,
                                                          int src_x,
                                                          int src_y,
                                                          int src_width,
                                                          int src_height,
                                                          int dst_x,
                                                          int dst_y,
                                                          int angle_x,
                                                          int angle_y);

/**
 * Blend image onto image at angle, skewed
 *
 * Blends the source rectangle (@p src_x, @p src_y, @p src_width, * @p src_height)
 * from the @p src_image onto the current image at the destination
 * (@p dst_x, @p dst_y) location.
 * It will be rotated and scaled so that the upper right corner will be
 * positioned @p h_angle_x pixels to the right (or left, if negative) and
 * @p h_angle_y pixels down (from (@p dst_x, @p dst_y).
 * If @p v_angle_x and @p v_angle_y are not 0, the image will also be
 * skewed so that the lower left corner will be positioned @p v_angle_x
 * pixels to the right and @p v_angle_y pixels down. The at_angle versions
 * simply have the @p v_angle_x and @p v_angle_y set to 0 so the rotation
 * doesn't get skewed, and the render_..._on_drawable ones seem obvious
 * enough; they do the same on a drawable.
 *
 * Example:
 * @code
 * imlib_blend_image_onto_image_skewed(..., 0, 0, 100, 0, 0, 100);
 * @endcode
 * will simply scale the image to be 100x100.
 * @code
 * imlib_blend_image_onto_image_skewed(..., 0, 0, 0, 100, 100, 0);
 * @endcode
 * will scale the image to be 100x100, and flip it diagonally.
 * @code
 * imlib_blend_image_onto_image_skewed(..., 100, 0, 0, 100, -100, 0);
 * @endcode
 * will scale the image and rotate it 90 degrees clockwise.
 * @code
 * imlib_blend_image_onto_image_skewed(..., 50, 0, 50, 50, -50, 50);
 * @endcode
 * will rotate the image 45 degrees clockwise, and will scale it so
 * its corners are at (50,0)-(100,50)-(50,100)-(0,50) i.e. it fits
 * into the 100x100 square, so it's scaled down to 70.7% (sqrt(2)/2).
 * @code
 * imlib_blend_image_onto_image_skewed(..., 50, 50, 100 * cos(a), 100 * sin(a), 0);
 * @endcode
 * will rotate the image `a' degrees, with its upper left corner at (50,50).
 *
 * @param src_image     The source image
 * @param merge_alpha   A char
 * @param src_x         The source x coordinate
 * @param src_y         The source y coordinate
 * @param src_width     The source width
 * @param src_height    The source height
 * @param dst_x         The destination x coordinate
 * @param dst_y         The destination y coordinate
 * @param h_angle_x     An angle
 * @param h_angle_y     An angle
 * @param v_angle_x     An angle
 * @param v_angle_y     An angle
 */
EAPI void           imlib_blend_image_onto_image_skewed(Imlib_Image
                                                        src_image,
                                                        char merge_alpha,
                                                        int src_x,
                                                        int src_y,
                                                        int src_width,
                                                        int src_height,
                                                        int dst_x,
                                                        int dst_y,
                                                        int h_angle_x,
                                                        int h_angle_y,
                                                        int v_angle_x,
                                                        int v_angle_y);

#ifndef X_DISPLAY_MISSING
/**
 * Blend image onto drawable at angle, skewed
 *
 * Works just like imlib_blend_image_onto_image_skewed(), except it
 * blends the image onto the current drawable instead of the current
 * image.
 *
 * @param src_x         The source x coordinate
 * @param src_y         The source y coordinate
 * @param src_width     The source width
 * @param src_height    The source height
 * @param dst_x         The destination x coordinate
 * @param dst_y         The destination y coordinate
 * @param h_angle_x     An angle
 * @param h_angle_y     An angle
 * @param v_angle_x     An angle
 * @param v_angle_y     An angle
 */
EAPI void           imlib_render_image_on_drawable_skewed(int src_x,
                                                          int src_y,
                                                          int src_width,
                                                          int src_height,
                                                          int dst_x,
                                                          int dst_y,
                                                          int h_angle_x,
                                                          int h_angle_y,
                                                          int v_angle_x,
                                                          int v_angle_y);

/**
 * Blend image onto drawable at angle
 *
 * Works just like imlib_render_image_on_drawable_skewed() except you
 * cannot skew an image (@p v_angle_x and @p v_angle_y are 0).
 *
 * @param src_x         The source x coordinate
 * @param src_y         The source y coordinate
 * @param src_width     The source width
 * @param src_height    The source height
 * @param dst_x         The destination x coordinate
 * @param dst_y         The destination y coordinate
 * @param angle_x       An angle
 * @param angle_y       An angle
 */
EAPI void           imlib_render_image_on_drawable_at_angle(int src_x,
                                                            int src_y,
                                                            int src_width,
                                                            int src_height,
                                                            int dst_x,
                                                            int dst_y,
                                                            int angle_x,
                                                            int angle_y);

#endif /* X_DISPLAY_MISSING */

/*--------------------------------
 * Image filters
 */

/**
 * Filter current image with current filter
 */
EAPI void           imlib_image_filter(void);

/**
 * Create image filter object
 */
EAPI Imlib_Filter   imlib_create_filter(int initsize);

/**
 * Set the current image filter
 *
 * Set this to NULL to disable filters.
 *
 * @param filter        The filter
 */
EAPI void           imlib_context_set_filter(Imlib_Filter filter);

/**
 * Get the current image filter
 *
 * @return Current filter
 */
EAPI Imlib_Filter   imlib_context_get_filter(void);

/**
 * Free the current image filter
 */
EAPI void           imlib_free_filter(void);

EAPI void           imlib_filter_set(int xoff, int yoff,
                                     int a, int r, int g, int b);
EAPI void           imlib_filter_set_alpha(int xoff, int yoff,
                                           int a, int r, int g, int b);
EAPI void           imlib_filter_set_red(int xoff, int yoff,
                                         int a, int r, int g, int b);
EAPI void           imlib_filter_set_green(int xoff, int yoff,
                                           int a, int r, int g, int b);
EAPI void           imlib_filter_set_blue(int xoff, int yoff,
                                          int a, int r, int g, int b);
EAPI void           imlib_filter_constants(int a, int r, int g, int b);
EAPI void           imlib_filter_divisors(int a, int r, int g, int b);

EAPI void           imlib_apply_filter(const char *script, ...);

/**
 * Clear current image
 *
 * Set all pixels values to 0.
 */
EAPI void           imlib_image_clear(void);

/**
 * Set all current image pixels to given color
 */
EAPI void           imlib_image_clear_color(int r, int g, int b, int a);

/*--------------------------------
 * Multiframe image loading
 */

typedef struct {
   int                 frame_count;     /* Number of frames in image      */
   int                 frame_num;       /* Current frame (1..frame_count) */
   int                 canvas_w, canvas_h;      /* Canvas size  */
   int                 frame_x, frame_y;        /* Frame origin */
   int                 frame_w, frame_h;        /* Frame size   */
   int                 frame_flags;     /* Frame info flags */
   int                 frame_delay;     /* Frame delay (ms) */
   int                 loop_count;      /* Number of animation loops */
} Imlib_Frame_Info;

/* frame info flags */
#define IMLIB_IMAGE_ANIMATED      (1 << 0)      /* Frames are an animated sequence    */
#define IMLIB_FRAME_BLEND         (1 << 1)      /* Blend current onto previous frame  */
#define IMLIB_FRAME_DISPOSE_CLEAR (1 << 2)      /* Clear before rendering next frame  */
#define IMLIB_FRAME_DISPOSE_PREV  (1 << 3)      /* Revert before rendering next frame */

/**
 * Load image frame
 *
 * Loads the specified frame within the image.
 * On success an image handle is returned, otherwise NULL is returned
 * (e.g. if the requested frame does not exist).
 * The image is loaded immediately.
 *
 * @param file          Image file
 * @param frame         Frame number
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_load_image_frame(const char *file, int frame);

/**
 * Load image frame form memory
 *
 * Loads the specified frame within the image from memory.
 * The file name @p file is only used to guess the file format.
 * On success an image handle is returned, otherwise NULL is returned
 * (e.g. if the requested frame does not exist).
 * The image is loaded immediately.
 *
 * @param file          File name
 * @param frame         Frame number
 * @param data          Image data
 * @param size          Image data size
 *
 * @return Image handle (NULL on failure)
 */
EAPI Imlib_Image    imlib_load_image_frame_mem(const char *file, int frame,
                                               const void *data, size_t size);

/**
 * Get information about current image frame
 *
 * @param info          Imlib_Frame_Info struct returning the information
 */
EAPI void           imlib_image_get_frame_info(Imlib_Frame_Info * info);

/**
 * Return string describing error code
 *
 * @p err must be a value returned by imlib_load/save_image_with_errno_return().
 * The returned string must not be modified or freed.
 *
 * @param err           The error code
 *
 * @return String describing the error code
 */
EAPI const char    *imlib_strerror(int err);

/*--------------------------------
 * Deprecated functionality
 */

/* Old data types, no longer used by imlib2, likely to be removed */
#ifndef DATA64
#define DATA64 uint64_t
#define DATA32 uint32_t
#define DATA16 uint16_t
#define DATA8  uint8_t
#endif

/* *INDENT-OFF* */

typedef enum {
   IMLIB_LOAD_ERROR_NONE,
   IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST,
   IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY,
   IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ,
   IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT,
   IMLIB_LOAD_ERROR_PATH_TOO_LONG,
   IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT,
   IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY,
   IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE,
   IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS,
   IMLIB_LOAD_ERROR_OUT_OF_MEMORY,
   IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS,
   IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_WRITE,
   IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE,
   IMLIB_LOAD_ERROR_UNKNOWN,
   IMLIB_LOAD_ERROR_IMAGE_READ,
   IMLIB_LOAD_ERROR_IMAGE_FRAME
} Imlib_Load_Error;

/**
 * Load an image from file with error return
 *
 * The image is loaded without deferred image data decoding.
 *
 * On error @p error_return is set to the detail of the error.
 *
 * @param file          File name
 * @param error_return  The returned error
 *
 * @return Image handle (NULL on failure)
 *
 * @deprecated Use imlib_load_image_with_errno_return()
 */
/* IMLIB2_DEPRECATED */
EAPI Imlib_Image    imlib_load_image_with_error_return(const char *file,
                                                       Imlib_Load_Error *
                                                       error_return);

/**
 * Save image to file with error return
 *
 * Works the same way imlib_save_image() works, but will set the
 * @p error_return to an error value if the save fails.
 *
 * @param file          The file name
 * @param error_return  The returned error
 *
 * @deprecated Use imlib_save_image_with_errno_return()
 */
/* IMLIB2_DEPRECATED */
EAPI void           imlib_save_image_with_error_return(const char *file,
                                                       Imlib_Load_Error *
                                                       error_return);

/** Deprecated function - hasn't done anything useful ever
 * @deprecated Useless - don't use */
IMLIB2_DEPRECATED
EAPI void           imlib_image_set_irrelevant_border(char irrelevant);

/** Deprecated function - hasn't done anything useful ever
 * @deprecated Useless - don't use */
IMLIB2_DEPRECATED
EAPI void           imlib_image_set_irrelevant_alpha(char irrelevant);

/* Encodings known to Imlib2 (so far) */
typedef enum {
   IMLIB_TTF_ENCODING_ISO_8859_1,
   IMLIB_TTF_ENCODING_ISO_8859_2,
   IMLIB_TTF_ENCODING_ISO_8859_3,
   IMLIB_TTF_ENCODING_ISO_8859_4,
   IMLIB_TTF_ENCODING_ISO_8859_5
} Imlib_TTF_Encoding;

/** Deprecated function - hasn't done anything useful in ~20 years
 * @deprecated Useless - don't use */
IMLIB2_DEPRECATED
EAPI void           imlib_context_set_TTF_encoding(Imlib_TTF_Encoding encoding);

/** Deprecated function - hasn't done anything useful in ~20 years
 * @deprecated Useless - don't use */
IMLIB2_DEPRECATED
EAPI Imlib_TTF_Encoding imlib_context_get_TTF_encoding(void);

#ifdef __cplusplus
}
#endif
/* *INDENT-ON* */
#endif
