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

int
load2(ImlibImage * im, int load_data)
{
   int                 w, h, rc;
   struct jpeg_decompress_struct cinfo;
   ImLib_JPEG_data     jdata;
   DATA8              *ptr, *line[16];
   DATA32             *ptr2;
   int                 x, y, l, i, scans;

   /* set up error handling */
   cinfo.err = _jdata_init(&jdata);
   if (sigsetjmp(jdata.setjmp_buffer, 1))
     {
        rc = LOAD_FAIL;
        goto quit;
     }

   rc = LOAD_FAIL;

   jpeg_create_decompress(&cinfo);
   jpeg_stdio_src(&cinfo, im->fp);
   jpeg_read_header(&cinfo, TRUE);

   im->w = w = cinfo.image_width;
   im->h = h = cinfo.image_height;
   if (!IMAGE_DIMENSIONS_OK(w, h))
      goto quit;

   UNSET_FLAG(im->flags, F_HAS_ALPHA);

   if (!load_data)
     {
        rc = LOAD_SUCCESS;
        goto quit;
     }

   /* Load data */

   cinfo.do_fancy_upsampling = FALSE;
   cinfo.do_block_smoothing = FALSE;
   jpeg_start_decompress(&cinfo);

   if ((cinfo.rec_outbuf_height > 16) || (cinfo.output_components <= 0))
      goto quit;

   jdata.data = malloc(w * 16 * cinfo.output_components);
   if (!jdata.data)
      goto quit;

   /* must set the im->data member before callign progress function */
   ptr2 = __imlib_AllocateData(im);
   if (!ptr2)
      goto quit;

   for (i = 0; i < cinfo.rec_outbuf_height; i++)
      line[i] = jdata.data + (i * w * cinfo.output_components);

   for (l = 0; l < h; l += cinfo.rec_outbuf_height)
     {
        jpeg_read_scanlines(&cinfo, line, cinfo.rec_outbuf_height);

        scans = cinfo.rec_outbuf_height;
        if ((h - l) < scans)
           scans = h - l;

        for (y = 0; y < scans; y++)
          {
             ptr = line[y];

             switch (cinfo.out_color_space)
               {
               default:
                  goto quit;
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

        if (im->lc && __imlib_LoadProgressRows(im, l, scans))
          {
             rc = LOAD_BREAK;
             goto quit;
          }
     }

   jpeg_finish_decompress(&cinfo);

   rc = LOAD_SUCCESS;

 quit:
   jpeg_destroy_decompress(&cinfo);
   free(jdata.data);
   if (rc <= 0)
      __imlib_FreeData(im);

   return rc;
}

char
save(ImlibImage * im, ImlibProgressFunction progress, char progress_granularity)
{
   int                 rc;
   struct jpeg_compress_struct cinfo;
   ImLib_JPEG_data     jdata;
   FILE               *f;
   DATA8              *buf;
   DATA32             *ptr;
   JSAMPROW           *jbuf;
   int                 y, quality, compression;
   ImlibImageTag      *tag;
   int                 i, j;

   /* allocate a small buffer to convert image data */
   buf = malloc(im->w * 3 * sizeof(DATA8));
   if (!buf)
      return LOAD_FAIL;

   rc = LOAD_FAIL;

   f = fopen(im->real_file, "wb");
   if (!f)
      goto quit;

   /* set up error handling */
   cinfo.err = _jdata_init(&jdata);
   if (sigsetjmp(jdata.setjmp_buffer, 1))
      goto quit;

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
   jpeg_set_defaults(&cinfo);
   jpeg_set_quality(&cinfo, quality, TRUE);
   jpeg_start_compress(&cinfo, TRUE);
   /* get the start pointer */
   ptr = im->data;
   /* go one scanline at a time... and save */
   for (y = 0; cinfo.next_scanline < cinfo.image_height; y++)
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

        if (im->lc && __imlib_LoadProgressRows(im, y, 1))
          {
             rc = LOAD_BREAK;
             goto quit;
          }
     }

   rc = LOAD_SUCCESS;

 quit:
   /* finish off */
   jpeg_finish_compress(&cinfo);
   jpeg_destroy_compress(&cinfo);
   free(buf);
   fclose(f);

   return rc;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "jpg", "jpeg", "jfif", "jfi" };
   __imlib_LoaderSetFormats(l, list_formats,
                            sizeof(list_formats) / sizeof(char *));
}
