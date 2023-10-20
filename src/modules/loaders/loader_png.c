#include "loader_common.h"

#include <png.h>
#include <sys/mman.h>

/* this is a quick sample png loader module... nice and small isn't it? */

/* PNG stuff */
#define PNG_BYTES_TO_CHECK 4

typedef struct {
   unsigned char     **lines;
} ImLib_PNG_data;

static void
comment_free(ImlibImage * im, void *data)
{
   free(data);
}

int
load2(ImlibImage * im, int load_data)
{
   int                 rc, ok;
   void               *fdata;
   png_uint_32         w32, h32;
   char                hasa;
   png_structp         png_ptr = NULL;
   png_infop           info_ptr = NULL;
   int                 bit_depth, color_type, interlace_type;
   ImLib_PNG_data      pdata;
   int                 i;

   /* read header */
   rc = LOAD_FAIL;
   pdata.lines = NULL;

   if (im->fsize < PNG_BYTES_TO_CHECK)
      return rc;

   fdata =
      mmap(NULL, PNG_BYTES_TO_CHECK, PROT_READ, MAP_SHARED, fileno(im->fp), 0);
   if (fdata == MAP_FAILED)
      return rc;

   ok = png_sig_cmp(fdata, 0, PNG_BYTES_TO_CHECK) == 0;

   munmap(fdata, PNG_BYTES_TO_CHECK);

   if (!ok)
      return rc;

   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (!png_ptr)
      goto quit;

   info_ptr = png_create_info_struct(png_ptr);
   if (!info_ptr)
      goto quit;

   if (setjmp(png_jmpbuf(png_ptr)))
     {
        rc = LOAD_FAIL;
        goto quit;
     }

   png_init_io(png_ptr, im->fp);
   png_read_info(png_ptr, info_ptr);
   png_get_IHDR(png_ptr, info_ptr, (png_uint_32 *) (&w32),
                (png_uint_32 *) (&h32), &bit_depth, &color_type,
                &interlace_type, NULL, NULL);
   if (!IMAGE_DIMENSIONS_OK(w32, h32))
      goto quit;

   im->w = (int)w32;
   im->h = (int)h32;

   hasa = 0;
   if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
      hasa = 1;
   if (color_type == PNG_COLOR_TYPE_RGB_ALPHA)
      hasa = 1;
   if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
      hasa = 1;
   UPDATE_FLAG(im->flags, F_HAS_ALPHA, hasa);

   if (!load_data)
     {
        rc = LOAD_SUCCESS;
        goto quit;
     }

   /* Load data */

   /* Prep for transformations...  ultimately we want ARGB */
   /* expand palette -> RGB if necessary */
   if (color_type == PNG_COLOR_TYPE_PALETTE)
      png_set_palette_to_rgb(png_ptr);
   /* expand gray (w/reduced bits) -> 8-bit RGB if necessary */
   if ((color_type == PNG_COLOR_TYPE_GRAY) ||
       (color_type == PNG_COLOR_TYPE_GRAY_ALPHA))
     {
        png_set_gray_to_rgb(png_ptr);
        if (bit_depth < 8)
           png_set_expand_gray_1_2_4_to_8(png_ptr);
     }
   /* expand transparency entry -> alpha channel if present */
   if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
      png_set_tRNS_to_alpha(png_ptr);
   /* reduce 16bit color -> 8bit color if necessary */
   if (bit_depth > 8)
      png_set_strip_16(png_ptr);
   /* pack all pixels to byte boundaries */
   png_set_packing(png_ptr);

/* note from raster:                                                         */
/* thanks to mustapha for helping debug this on PPC Linux remotely by        */
/* sending across screenshots all the time and me figuring out from them     */
/* what the hell was up with the colors                                      */
/* now png loading should work on big-endian machines nicely                 */
#ifdef WORDS_BIGENDIAN
   png_set_swap_alpha(png_ptr);
   if (!hasa)
      png_set_filler(png_ptr, 0xff, PNG_FILLER_BEFORE);
#else
   png_set_bgr(png_ptr);
   if (!hasa)
      png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
#endif

   if (!__imlib_AllocateData(im))
      goto quit;

   pdata.lines = malloc(im->h * sizeof(unsigned char *));
   if (!pdata.lines)
      goto quit;

   for (i = 0; i < im->h; i++)
      pdata.lines[i] = (unsigned char *)(im->data + i * im->w);

   if (im->lc)
     {
        int                 y, pass, n_passes, nrows = 1;

        n_passes = png_set_interlace_handling(png_ptr);
        for (pass = 0; pass < n_passes; pass++)
          {
             __imlib_LoadProgressSetPass(im, pass, n_passes);

             for (y = 0; y < im->h; y += nrows)
               {
                  png_read_rows(png_ptr, &pdata.lines[y], NULL, nrows);

                  if (__imlib_LoadProgressRows(im, y, nrows))
                    {
                       rc = LOAD_BREAK;
                       goto quit1;
                    }
               }
          }
     }
   else
     {
        png_read_image(png_ptr, pdata.lines);
     }

   rc = LOAD_SUCCESS;

#ifdef PNG_TEXT_SUPPORTED
   {
      png_textp           text;
      int                 num;

      num = 0;
      png_get_text(png_ptr, info_ptr, &text, &num);
      for (i = 0; i < num; i++)
        {
           if (!strcmp(text[i].key, "Imlib2-Comment"))
              __imlib_AttachTag(im, "comment", 0, strdup(text[i].text),
                                comment_free);
        }
   }
#endif

 quit1:
   png_read_end(png_ptr, info_ptr);
 quit:
   free(pdata.lines);
   png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
   if (rc <= 0)
      __imlib_FreeData(im);

   return rc;
}

char
save(ImlibImage * im, ImlibProgressFunction progress, char progress_granularity)
{
   int                 rc;
   FILE               *f;
   png_structp         png_ptr;
   png_infop           info_ptr;
   DATA32             *ptr;
   int                 x, y, j, interlace;
   png_bytep           row_ptr, data;
   png_color_8         sig_bit;
   ImlibImageTag      *tag;
   int                 quality = 75, compression = 3;
   int                 pass, n_passes = 1;
   int                 has_alpha;

   f = fopen(im->real_file, "wb");
   if (!f)
      return LOAD_FAIL;

   rc = LOAD_FAIL;
   info_ptr = NULL;
   data = NULL;

   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (!png_ptr)
      goto quit;

   info_ptr = png_create_info_struct(png_ptr);
   if (!info_ptr)
      goto quit;

   if (setjmp(png_jmpbuf(png_ptr)))
      goto quit;

   /* check whether we should use interlacing */
   interlace = PNG_INTERLACE_NONE;
   if ((tag = __imlib_GetTag(im, "interlacing")) && tag->val)
     {
#ifdef PNG_WRITE_INTERLACING_SUPPORTED
        interlace = PNG_INTERLACE_ADAM7;
#endif
     }

   png_init_io(png_ptr, f);
   has_alpha = IMAGE_HAS_ALPHA(im);
   if (has_alpha)
     {
        png_set_IHDR(png_ptr, info_ptr, im->w, im->h, 8,
                     PNG_COLOR_TYPE_RGB_ALPHA, interlace,
                     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
#ifdef WORDS_BIGENDIAN
        png_set_swap_alpha(png_ptr);
#else
        png_set_bgr(png_ptr);
#endif
     }
   else
     {
        png_set_IHDR(png_ptr, info_ptr, im->w, im->h, 8, PNG_COLOR_TYPE_RGB,
                     interlace, PNG_COMPRESSION_TYPE_BASE,
                     PNG_FILTER_TYPE_BASE);
        data = malloc(im->w * 3 * sizeof(png_byte));
     }
   sig_bit.red = 8;
   sig_bit.green = 8;
   sig_bit.blue = 8;
   sig_bit.alpha = 8;
   png_set_sBIT(png_ptr, info_ptr, &sig_bit);
   /* quality */
   tag = __imlib_GetTag(im, "quality");
   if (tag)
     {
        quality = tag->val;
        if (quality < 1)
           quality = 1;
        if (quality > 99)
           quality = 99;
     }
   /* convert to compression */
   quality = quality / 10;
   compression = 9 - quality;
   /* compression */
   tag = __imlib_GetTag(im, "compression");
   if (tag)
      compression = tag->val;
   if (compression < 0)
      compression = 0;
   if (compression > 9)
      compression = 9;
   tag = __imlib_GetTag(im, "comment");
   if (tag)
     {
#ifdef PNG_TEXT_SUPPORTED
        png_text            text;

        text.key = (char *)"Imlib2-Comment";
        text.text = tag->data;
        text.compression = PNG_TEXT_COMPRESSION_zTXt;
        png_set_text(png_ptr, info_ptr, &(text), 1);
#endif
     }
   png_set_compression_level(png_ptr, compression);
   png_write_info(png_ptr, info_ptr);
   png_set_shift(png_ptr, &sig_bit);
   png_set_packing(png_ptr);

#ifdef PNG_WRITE_INTERLACING_SUPPORTED
   n_passes = png_set_interlace_handling(png_ptr);
#endif

   for (pass = 0; pass < n_passes; pass++)
     {
        ptr = im->data;

        if (im->lc)
           __imlib_LoadProgressSetPass(im, pass, n_passes);

        for (y = 0; y < im->h; y++)
          {
             if (has_alpha)
                row_ptr = (png_bytep) ptr;
             else
               {
                  for (j = 0, x = 0; x < im->w; x++)
                    {
                       DATA32              pixel = ptr[x];

                       data[j++] = PIXEL_R(pixel);
                       data[j++] = PIXEL_G(pixel);
                       data[j++] = PIXEL_B(pixel);
                    }
                  row_ptr = (png_bytep) data;
               }
             png_write_rows(png_ptr, &row_ptr, 1);

             if (im->lc && __imlib_LoadProgressRows(im, y, 1))
               {
                  rc = LOAD_BREAK;
                  goto quit;
               }

             ptr += im->w;
          }
     }

   rc = LOAD_SUCCESS;

 quit:
   free(data);
   png_write_end(png_ptr, info_ptr);
   png_destroy_write_struct(&png_ptr, (png_infopp) & info_ptr);
   if (info_ptr)
      png_destroy_info_struct(png_ptr, (png_infopp) & info_ptr);
   if (png_ptr)
      png_destroy_write_struct(&png_ptr, (png_infopp) NULL);

   fclose(f);

   return rc;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "png" };
   __imlib_LoaderSetFormats(l, list_formats, ARRAY_SIZE(list_formats));
}
