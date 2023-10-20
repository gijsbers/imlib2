#include "loader_common.h"
#include <jpeglib.h>
#include <setjmp.h>

typedef struct {
   struct jpeg_error_mgr jem;
   sigjmp_buf          setjmp_buffer;
   DATA8              *data;
} ImLib_JPEG_data;

static void
_JPEGFatalErrorHandler(j_common_ptr cinfo)
{
   ImLib_JPEG_data    *jd = (ImLib_JPEG_data *) cinfo->err;

#if 0
   cinfo->err->output_message(cinfo);
#endif
   siglongjmp(jd->setjmp_buffer, 1);
}

static void
_JPEGErrorHandler(j_common_ptr cinfo)
{
#if 0
   ImLib_JPEG_data    *jd = (ImLib_JPEG_data *) cinfo->err;

   cinfo->err->output_message(cinfo);
   siglongjmp(jd->setjmp_buffer, 1);
#endif
}

static void
_JPEGErrorHandler2(j_common_ptr cinfo, int msg_level)
{
#if 0
   ImLib_JPEG_data    *jd = (ImLib_JPEG_data *) cinfo->err;

   cinfo->err->output_message(cinfo);
   siglongjmp(jd->setjmp_buffer, 1);
#endif
}

static struct jpeg_error_mgr *
_jdata_init(ImLib_JPEG_data * jd)
{
   struct jpeg_error_mgr *jem;

   jem = jpeg_std_error(&jd->jem);

   jd->jem.error_exit = _JPEGFatalErrorHandler;
   jd->jem.emit_message = _JPEGErrorHandler2;
   jd->jem.output_message = _JPEGErrorHandler;

   jd->data = NULL;

   return jem;
}

char
load(ImlibImage * im, ImlibProgressFunction progress,
     char progress_granularity, char immediate_load)
{
   int                 w, h, rc;
   struct jpeg_decompress_struct cinfo;
   ImLib_JPEG_data     jdata;
   FILE               *f;

   f = fopen(im->real_file, "rb");
   if (!f)
      return 0;

   /* set up error handling */
   cinfo.err = _jdata_init(&jdata);
   if (sigsetjmp(jdata.setjmp_buffer, 1))
      goto quit_error;

   jpeg_create_decompress(&cinfo);
   jpeg_stdio_src(&cinfo, f);
   jpeg_read_header(&cinfo, TRUE);
   im->w = w = cinfo.image_width;
   im->h = h = cinfo.image_height;

   rc = 1;                      /* Ok */

   if ((!im->loader) && (!im->data))
     {
        if (!IMAGE_DIMENSIONS_OK(w, h))
           goto quit_error;
        UNSET_FLAG(im->flags, F_HAS_ALPHA);
     }

   if (im->loader || immediate_load || progress)
     {
        DATA8              *ptr, *line[16];
        DATA32             *ptr2;
        int                 x, y, l, i, scans, count, prevy;

        cinfo.do_fancy_upsampling = FALSE;
        cinfo.do_block_smoothing = FALSE;
        jpeg_start_decompress(&cinfo);

        if ((cinfo.rec_outbuf_height > 16) || (cinfo.output_components <= 0) ||
            !IMAGE_DIMENSIONS_OK(w, h))
           goto quit_error;

        jdata.data = malloc(w * 16 * cinfo.output_components);
        if (!jdata.data)
           goto quit_error;

        /* must set the im->data member before callign progress function */
        ptr2 = __imlib_AllocateData(im);
        if (!ptr2)
           goto quit_error;

        count = 0;
        prevy = 0;

        for (i = 0; i < cinfo.rec_outbuf_height; i++)
           line[i] = jdata.data + (i * w * cinfo.output_components);

        for (l = 0; l < h; l += cinfo.rec_outbuf_height)
          {
             jpeg_read_scanlines(&cinfo, line, cinfo.rec_outbuf_height);
             scans = cinfo.rec_outbuf_height;
             if ((h - l) < scans)
                scans = h - l;
             ptr = jdata.data;
             for (y = 0; y < scans; y++)
               {
                  switch (cinfo.out_color_space)
                    {
                    default:
                       goto quit_error;
                    case JCS_GRAYSCALE:
                       for (x = 0; x < w; x++)
                         {
                            *ptr2 = PIXEL_ARGB(0xff, ptr[0], ptr[0], ptr[0]);
                            ptr++;
                            ptr2++;
                         }
                       break;
                    case JCS_RGB:
                       for (x = 0; x < w; x++)
                         {
                            *ptr2 = PIXEL_ARGB(0xff, ptr[0], ptr[1], ptr[2]);
                            ptr += cinfo.output_components;
                            ptr2++;
                         }
                       break;
                    case JCS_CMYK:
                       for (x = 0; x < w; x++)
                         {
                            *ptr2 = PIXEL_ARGB(0xff, ptr[0] * ptr[3] / 255,
                                               ptr[1] * ptr[3] / 255,
                                               ptr[2] * ptr[3] / 255);
                            ptr += cinfo.output_components;
                            ptr2++;
                         }
                       break;
                    }
               }

             if (progress)
               {
                  int                 per;

                  per = (l * 100) / h;
                  if (((per - count) >= progress_granularity)
                      || ((h - l) <= cinfo.rec_outbuf_height))
                    {
                       count = per;
                       if (!progress(im, per, 0, prevy, w, scans + l - prevy))
                         {
                            rc = 2;
                            goto done;
                         }
                       prevy = l + scans;
                    }
               }
          }

      done:
        jpeg_finish_decompress(&cinfo);
     }

 quit:
   jpeg_destroy_decompress(&cinfo);
   free(jdata.data);
   fclose(f);
   return rc;

 quit_error:
   rc = 0;                      /* Error */
   __imlib_FreeData(im);
   goto quit;
}

char
save(ImlibImage * im, ImlibProgressFunction progress, char progress_granularity)
{
   struct jpeg_compress_struct cinfo;
   ImLib_JPEG_data     jdata;
   FILE               *f;
   DATA8              *buf;
   DATA32             *ptr;
   JSAMPROW           *jbuf;
   int                 y, quality, compression;
   ImlibImageTag      *tag;
   int                 i, j, pl;
   char                pper;

   /* no image data? abort */
   if (!im->data)
      return 0;

   /* allocate a small buffer to convert image data */
   buf = malloc(im->w * 3 * sizeof(DATA8));
   if (!buf)
      return 0;

   f = fopen(im->real_file, "wb");
   if (!f)
     {
        free(buf);
        return 0;
     }

   /* set up error handling */
   cinfo.err = _jdata_init(&jdata);
   if (sigsetjmp(jdata.setjmp_buffer, 1))
     {
        jpeg_destroy_compress(&cinfo);
        free(buf);
        fclose(f);
        return 0;
     }

   /* setup compress params */
   jpeg_create_compress(&cinfo);
   jpeg_stdio_dest(&cinfo, f);
   cinfo.image_width = im->w;
   cinfo.image_height = im->h;
   cinfo.input_components = 3;
   cinfo.in_color_space = JCS_RGB;

   /* look for tags attached to image to get extra parameters like quality */
   /* settigns etc. - this is the "api" to hint for extra information for */
   /* saver modules */

   /* compression */
   compression = 2;
   tag = __imlib_GetTag(im, "compression");
   if (tag)
     {
        compression = tag->val;
        if (compression < 0)
           compression = 0;
        if (compression > 9)
           compression = 9;
     }
   /* convert to quality */
   quality = (9 - compression) * 10;
   quality = quality * 10 / 9;
   /* quality */
   tag = __imlib_GetTag(im, "quality");
   if (tag)
      quality = tag->val;
   if (quality < 1)
      quality = 1;
   if (quality > 100)
      quality = 100;

   /* set up jepg compression parameters */
   y = 0;
   pl = 0;
   pper = 0;
   jpeg_set_defaults(&cinfo);
   jpeg_set_quality(&cinfo, quality, TRUE);
   jpeg_start_compress(&cinfo, TRUE);
   /* get the start pointer */
   ptr = im->data;
   /* go one scanline at a time... and save */
   while (cinfo.next_scanline < cinfo.image_height)
     {
        /* convcert scaline from ARGB to RGB packed */
        for (j = 0, i = 0; i < im->w; i++)
          {
             DATA32              pixel = *ptr++;

             buf[j++] = PIXEL_R(pixel);
             buf[j++] = PIXEL_G(pixel);
             buf[j++] = PIXEL_B(pixel);
          }
        /* write scanline */
        jbuf = (JSAMPROW *) (&buf);
        jpeg_write_scanlines(&cinfo, jbuf, 1);
        y++;
        if (progress)
          {
             char                per;
             int                 l;

             per = (char)((100 * y) / im->h);
             if (((per - pper) >= progress_granularity) || (y == (im->h - 1)))
               {
                  l = y - pl;
                  if (!progress(im, per, 0, (y - l), im->w, l))
                    {
                       jpeg_finish_compress(&cinfo);
                       jpeg_destroy_compress(&cinfo);
                       free(buf);
                       fclose(f);
                       return 2;
                    }
                  pper = per;
                  pl = y;
               }
          }
     }
   /* finish off */
   jpeg_finish_compress(&cinfo);
   jpeg_destroy_compress(&cinfo);
   free(buf);
   fclose(f);
   return 1;
   progress = NULL;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "jpg", "jpeg", "jfif", "jfi" };
   int                 i;

   l->num_formats = sizeof(list_formats) / sizeof(char *);
   l->formats = malloc(sizeof(char *) * l->num_formats);

   for (i = 0; i < l->num_formats; i++)
      l->formats[i] = strdup(list_formats[i]);
}
