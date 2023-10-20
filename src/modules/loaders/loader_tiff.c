/* To do: */
/* o Need code to handle tiff with different orientations */

#include "loader_common.h"
#include <setjmp.h>
#include <stdarg.h>
#include <tiffio.h>

/* This is a wrapper data structure for TIFFRGBAImage, so that data can be */
/* passed into the callbacks. More elegent, I think, than a bunch of globals */

struct TIFFRGBAImage_Extra {
   TIFFRGBAImage       rgba;
   tileContigRoutine   put_contig;
   tileSeparateRoutine put_separate;
   ImlibImage         *image;
   ImlibProgressFunction progress;
   char                pper;
   char                progress_granularity;
   uint32              num_pixels;
   uint32              py;
};

typedef struct TIFFRGBAImage_Extra TIFFRGBAImage_Extra;

static void         raster(TIFFRGBAImage_Extra * img, uint32 * raster, uint32 x,
                           uint32 y, uint32 w, uint32 h);

static void
put_contig_and_raster(TIFFRGBAImage * img, uint32 * rast,
                      uint32 x, uint32 y, uint32 w, uint32 h,
                      int32 fromskew, int32 toskew, unsigned char *cp)
{
   (*(((TIFFRGBAImage_Extra *) img)->put_contig)) (img, rast, x, y, w, h,
                                                   fromskew, toskew, cp);
   raster((TIFFRGBAImage_Extra *) img, rast, x, y, w, h);
}

static void
put_separate_and_raster(TIFFRGBAImage * img, uint32 * rast,
                        uint32 x, uint32 y, uint32 w, uint32 h,
                        int32 fromskew, int32 toskew,
                        unsigned char *r, unsigned char *g, unsigned char *b,
                        unsigned char *a)
{
   (*(((TIFFRGBAImage_Extra *) img)->put_separate))
      (img, rast, x, y, w, h, fromskew, toskew, r, g, b, a);
   raster((TIFFRGBAImage_Extra *) img, rast, x, y, w, h);
}

#define PIM(_x, _y) buffer + ((_x) + image_width * (_y))

static void
raster(TIFFRGBAImage_Extra * img, uint32 * rast,
       uint32 x, uint32 y, uint32 w, uint32 h)
{
   uint32              image_width, image_height;
   uint32             *pixel, pixel_value;
   uint32              i, j, k;
   DATA32             *buffer_pixel, *buffer = img->image->data;
   int                 alpha_premult;
   int                 a, r, g, b;

   image_width = img->image->w;
   image_height = img->image->h;

#if 0
   printf("%s: x,y=%d,%d wxh=%dx%d (image %dx%d)\n", __func__,
          x, y, w, h, image_width, image_height);
#endif

   /* rast seems to point to the beginning of the last strip processed */
   /* so you need use negative offsets. Bizzare. Someone please check this */
   /* I don't understand why, but that seems to be what's going on. */
   /* libtiff needs better docs! */

   if (img->rgba.alpha == EXTRASAMPLE_UNASSALPHA)
      alpha_premult = 1;
   switch (img->rgba.orientation)
     {
     default:
     case ORIENTATION_TOPLEFT:
     case ORIENTATION_TOPRIGHT:
        for (j = 0; j < h; j++)
          {
             pixel = rast - j * image_width;

             for (i = 0; i < w; i++)
               {
                  pixel_value = *pixel++;
                  a = TIFFGetA(pixel_value);
                  r = TIFFGetR(pixel_value);
                  g = TIFFGetG(pixel_value);
                  b = TIFFGetB(pixel_value);
                  if ((a > 0) && (a < 255) && (alpha_premult))
                    {
                       r = (r * 255) / a;
                       g = (g * 255) / a;
                       b = (b * 255) / a;
                    }
                  k = x + i;
                  if (img->rgba.orientation == ORIENTATION_TOPRIGHT)
                     k = image_width - 1 - k;
                  buffer_pixel = PIM(k, image_height - 1 - (y - j));
                  *buffer_pixel = (a << 24) | (r << 16) | (g << 8) | b;
               }
          }
        break;
     case ORIENTATION_BOTRIGHT:
     case ORIENTATION_BOTLEFT:
        for (j = 0; j < h; j++)
          {
             pixel = rast + j * image_width;

             for (i = 0; i < w; i++)
               {
                  pixel_value = *pixel++;
                  a = TIFFGetA(pixel_value);
                  r = TIFFGetR(pixel_value);
                  g = TIFFGetG(pixel_value);
                  b = TIFFGetB(pixel_value);
                  if ((a > 0) && (a < 255) && (alpha_premult))
                    {
                       r = (r * 255) / a;
                       g = (g * 255) / a;
                       b = (b * 255) / a;
                    }
                  k = x + i;
                  if (img->rgba.orientation == ORIENTATION_BOTRIGHT)
                     k = image_width - 1 - k;
                  buffer_pixel = PIM(k, image_height - 1 - (y + j));
                  *buffer_pixel = (a << 24) | (r << 16) | (g << 8) | b;
               }
          }
        break;

     case ORIENTATION_LEFTTOP:
     case ORIENTATION_RIGHTTOP:
        for (i = 0; i < h; i++)
          {
             pixel = rast - i * image_height;

             for (j = 0; j < w; j++)
               {
                  pixel_value = *pixel++;
                  a = TIFFGetA(pixel_value);
                  r = TIFFGetR(pixel_value);
                  g = TIFFGetG(pixel_value);
                  b = TIFFGetB(pixel_value);
                  if ((a > 0) && (a < 255) && (alpha_premult))
                    {
                       r = (r * 255) / a;
                       g = (g * 255) / a;
                       b = (b * 255) / a;
                    }
                  k = y - i;
                  if (img->rgba.orientation == ORIENTATION_LEFTTOP)
                     k = image_width - 1 - k;
                  buffer_pixel = PIM(k, x + j);
                  *buffer_pixel = (a << 24) | (r << 16) | (g << 8) | b;
               }
          }
        break;
     case ORIENTATION_RIGHTBOT:
     case ORIENTATION_LEFTBOT:
        for (i = 0; i < h; i++)
          {
             pixel = rast + i * image_height;

             for (j = 0; j < w; j++)
               {
                  pixel_value = *pixel++;
                  a = TIFFGetA(pixel_value);
                  r = TIFFGetR(pixel_value);
                  g = TIFFGetG(pixel_value);
                  b = TIFFGetB(pixel_value);
                  if ((a > 0) && (a < 255) && (alpha_premult))
                    {
                       r = (r * 255) / a;
                       g = (g * 255) / a;
                       b = (b * 255) / a;
                    }
                  k = y + i;
                  if (img->rgba.orientation == ORIENTATION_RIGHTBOT)
                     k = image_width - 1 - k;
                  buffer_pixel = PIM(k, image_height - 1 - (x + j));
                  *buffer_pixel = (a << 24) | (r << 16) | (g << 8) | b;
               }
          }
        break;
     }

   if (img->progress)
     {
        char                per;

        switch (img->rgba.orientation)
          {
          default:
          case ORIENTATION_TOPLEFT:
             if (w >= image_width)
               {
                  uint32              real_y = (image_height - 1) - y;

                  per = (char)(((real_y + h - 1) * 100) / image_height);

                  if (((per - img->pper) >= img->progress_granularity) ||
                      (real_y + h) >= image_height)
                    {
                       img->progress(img->image, per, 0, img->py, w,
                                     (real_y + h) - img->py);
                       img->py = real_y + h;
                       img->pper = per;
                    }
               }
             else
               {
                  /* for tile based images, we just progress each tile because */
                  /* of laziness. Couldn't think of a good way to do this */
                  y = image_height - 1 - y;
                  goto progress_a;
               }
             break;
          case ORIENTATION_TOPRIGHT:
             y = image_height - 1 - y;
             goto progress_a;
          case ORIENTATION_BOTRIGHT:
             y = image_height - y - h;
             goto progress_a;
          case ORIENTATION_BOTLEFT:
             y = image_height - y - h;
             goto progress_a;
           progress_a:
             per = (char)((w * h * 100) / img->num_pixels);
             img->pper += per;
             img->progress(img->image, img->pper, x, y, w, h);
             break;

          case ORIENTATION_LEFTTOP:
             y = image_width - 1 - y;
             goto progress_b;
          case ORIENTATION_RIGHTTOP:
             y = y + 1 - h;
             goto progress_b;
          case ORIENTATION_RIGHTBOT:
             y = image_width - y - h;
             goto progress_b;
          case ORIENTATION_LEFTBOT:
             goto progress_b;
           progress_b:
             per = (char)((w * h * 100) / img->num_pixels);
             img->pper += per;
             img->progress(img->image, img->pper, y, x, h, w);
             break;
          }
     }
}

char
load(ImlibImage * im, ImlibProgressFunction progress,
     char progress_granularity, char immediate_load)
{
   TIFF               *tif = NULL;
   FILE               *file;
   int                 fd, ok;
   uint16              magic_number;
   TIFFRGBAImage_Extra rgba_image;
   uint32             *rast = NULL;
   uint32              num_pixels;
   char                txt[1024];

   ok = 0;

   if (im->data)
      return 0;

   file = fopen(im->real_file, "rb");
   if (!file)
      return 0;

   fread(&magic_number, sizeof(uint16), 1, file);
   /* Apparently rewind(f) isn't sufficient */
   fseek(file, (long)0, SEEK_SET);

   if ((magic_number != TIFF_BIGENDIAN) /* Checks if actually tiff file */
       && (magic_number != TIFF_LITTLEENDIAN))
     {
        fclose(file);
        return 0;
     }

   fd = fileno(file);
   fd = dup(fd);
   lseek(fd, (long)0, SEEK_SET);
   fclose(file);

   tif = TIFFFdOpen(fd, im->real_file, "r");
   if (!tif)
      return 0;

   strcpy(txt, "Cannot be processed by libtiff");
   if (!TIFFRGBAImageOK(tif, txt))
      goto quit1;
   strcpy(txt, "Cannot begin reading tiff");
   if (!TIFFRGBAImageBegin((TIFFRGBAImage *) & rgba_image, tif, 1, txt))
      goto quit1;

   rgba_image.image = im;
   switch (rgba_image.rgba.orientation)
     {
     default:
     case ORIENTATION_TOPLEFT:
     case ORIENTATION_TOPRIGHT:
     case ORIENTATION_BOTRIGHT:
     case ORIENTATION_BOTLEFT:
        im->w = rgba_image.rgba.width;
        im->h = rgba_image.rgba.height;
        break;
     case ORIENTATION_LEFTTOP:
     case ORIENTATION_RIGHTTOP:
     case ORIENTATION_RIGHTBOT:
     case ORIENTATION_LEFTBOT:
        im->w = rgba_image.rgba.height;
        im->h = rgba_image.rgba.width;
        break;
     }
   if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
      goto quit2;
   rgba_image.num_pixels = num_pixels = im->w * im->h;
   if (rgba_image.rgba.alpha != EXTRASAMPLE_UNSPECIFIED)
      SET_FLAG(im->flags, F_HAS_ALPHA);
   else
      UNSET_FLAG(im->flags, F_HAS_ALPHA);
   if (!im->format)
      im->format = strdup("tiff");

   if ((im->loader) || (immediate_load) || (progress))
     {
        rgba_image.progress = progress;
        rgba_image.pper = rgba_image.py = 0;
        rgba_image.progress_granularity = progress_granularity;
        rast = (uint32 *) _TIFFmalloc(sizeof(uint32) * num_pixels);
        im->data = (DATA32 *) malloc(sizeof(DATA32) * num_pixels);

        if ((!rast) || (!im->data))     /* Error checking */
          {
             fprintf(stderr, "imlib2-tiffloader: Out of memory\n");

             if (rast)
                _TIFFfree(rast);
             if (im->data)
               {
                  free(im->data);
                  im->data = NULL;
               }
             goto quit2;
          }

        if (!rgba_image.rgba.put.any)
          {
             fprintf(stderr, "imlib2-tiffloader: No put function");

             _TIFFfree(rast);
             free(im->data);
             im->data = NULL;
             goto quit2;
          }

        if (rgba_image.rgba.isContig)
          {
             rgba_image.put_contig = rgba_image.rgba.put.contig;
             rgba_image.rgba.put.contig = put_contig_and_raster;
          }
        else
          {
             rgba_image.put_separate = rgba_image.rgba.put.separate;
             rgba_image.rgba.put.separate = put_separate_and_raster;
          }

        if (!TIFFRGBAImageGet((TIFFRGBAImage *) & rgba_image, rast,
                              rgba_image.rgba.width, rgba_image.rgba.height))
          {
             _TIFFfree(rast);
             free(im->data);
             im->data = NULL;
             goto quit2;
          }

        _TIFFfree(rast);
     }

   ok = 1;
 quit2:
   TIFFRGBAImageEnd((TIFFRGBAImage *) & rgba_image);
 quit1:
   TIFFClose(tif);

   return ok;
}

/* this seems to work, except the magic number isn't written. I'm guessing */
/* this is a problem in libtiff */

char
save(ImlibImage * im, ImlibProgressFunction progress, char progress_granularity)
{
   TIFF               *tif = NULL;
   uint8              *buf = NULL;
   DATA32              pixel, *data = im->data;
   double              alpha_factor;
   int                 x, y;
   uint8               r, g, b, a = 0;
   int                 has_alpha = IMAGE_HAS_ALPHA(im);
   int                 i = 0, pl = 0;
   char                pper = 0;

   /* By default uses patent-free use COMPRESSION_DEFLATE,
    * another lossless compression technique */
   ImlibImageTag      *tag;
   int                 compression_type = COMPRESSION_DEFLATE;

   if (!im->data)
      return 0;

   tif = TIFFOpen(im->real_file, "w");

   if (!tif)
      return 0;

   /* None of the TIFFSetFields are checked for errors, but since they */
   /* shouldn't fail, this shouldn't be a problem */

   TIFFSetField(tif, TIFFTAG_IMAGELENGTH, im->h);
   TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, im->w);
   TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
   TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
   TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
   TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_NONE);

   /* look for tags attached to image to get extra parameters like quality */
   /* settings etc. - this is the "api" to hint for extra information for */
   /* saver modules */

   /* compression */
   tag = __imlib_GetTag(im, "compression_type");
   if (tag)
     {
        compression_type = tag->val;
        switch (compression_type)
          {
          case COMPRESSION_NONE:
             break;
          case COMPRESSION_CCITTRLE:
             break;
          case COMPRESSION_CCITTFAX3:
             break;
          case COMPRESSION_CCITTFAX4:
             break;
          case COMPRESSION_LZW:
             break;
          case COMPRESSION_OJPEG:
             break;
          case COMPRESSION_JPEG:
             break;
          case COMPRESSION_NEXT:
             break;
          case COMPRESSION_CCITTRLEW:
             break;
          case COMPRESSION_PACKBITS:
             break;
          case COMPRESSION_THUNDERSCAN:
             break;
          case COMPRESSION_IT8CTPAD:
             break;
          case COMPRESSION_IT8LW:
             break;
          case COMPRESSION_IT8MP:
             break;
          case COMPRESSION_IT8BL:
             break;
          case COMPRESSION_PIXARFILM:
             break;
          case COMPRESSION_PIXARLOG:
             break;
          case COMPRESSION_DEFLATE:
             break;
          case COMPRESSION_ADOBE_DEFLATE:
             break;
          case COMPRESSION_DCS:
             break;
          case COMPRESSION_JBIG:
             break;
          case COMPRESSION_SGILOG:
             break;
          case COMPRESSION_SGILOG24:
             break;
          default:
             compression_type = COMPRESSION_DEFLATE;
          }

     }
   TIFFSetField(tif, TIFFTAG_COMPRESSION, compression_type);

   if (has_alpha)
     {
        uint16              extras[] = { EXTRASAMPLE_ASSOCALPHA };
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);
        TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, 1, extras);
     }
   else
     {
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
     }
   TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
   TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, 0));

   buf = (uint8 *) _TIFFmalloc(TIFFScanlineSize(tif));

   if (!buf)
     {
        TIFFClose(tif);
        return 0;
     }

   for (y = 0; y < im->h; y++)
     {
        i = 0;
        for (x = 0; x < im->w; x++)
          {
             pixel = data[(y * im->w) + x];

             r = (pixel >> 16) & 0xff;
             g = (pixel >> 8) & 0xff;
             b = pixel & 0xff;
             if (has_alpha)
               {
                  /* TIFF makes you pre-mutiply the rgb components by alpha */
                  a = (pixel >> 24) & 0xff;
                  alpha_factor = ((double)a / 255.0);
                  r *= alpha_factor;
                  g *= alpha_factor;
                  b *= alpha_factor;
               }

             /* This might be endian dependent */
             buf[i++] = r;
             buf[i++] = g;
             buf[i++] = b;
             if (has_alpha)
                buf[i++] = a;
          }

        if (!TIFFWriteScanline(tif, buf, y, 0))
          {
             _TIFFfree(buf);
             TIFFClose(tif);
             return 0;
          }

        if (progress)
          {
             char                per;
             int                 l;

             per = (char)((100 * y) / im->h);
             if ((per - pper) >= progress_granularity)
               {
                  l = y - pl;
                  (*progress) (im, per, 0, (y - l), im->w, l);
                  pper = per;
                  pl = y;
               }
          }
     }

   _TIFFfree(buf);
   TIFFClose(tif);

   return 1;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "tiff", "tif" };
   int                 i;

   l->num_formats = sizeof(list_formats) / sizeof(char *);
   l->formats = malloc(sizeof(char *) * l->num_formats);

   for (i = 0; i < l->num_formats; i++)
      l->formats[i] = strdup(list_formats[i]);
}
