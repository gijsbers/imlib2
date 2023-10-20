#include "loader_common.h"

#include <sys/mman.h>
#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>

#define DBG_PFX "LDR-webp"

int
load2(ImlibImage * im, int load_data)
{
   int                 rc;
   void               *fdata;
   WebPData            webp_data;
   WebPDemuxer        *demux;
   WebPIterator        iter;
   int                 frame;

   rc = LOAD_FAIL;

   if (im->fsize < 12)
      return rc;

   fdata = mmap(0, im->fsize, PROT_READ, MAP_SHARED, fileno(im->fp), 0);
   if (fdata == MAP_FAILED)
      return rc;

   webp_data.bytes = fdata;
   webp_data.size = im->fsize;

   /* Init (includes signature check) */
   demux = WebPDemux(&webp_data);
   if (!demux)
      goto quit;

   /* Key may select frame other than first */
   frame = 1;
   if (im->key)
     {
        frame = atoi(im->key);
        iter.num_frames = WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT);
        if (frame > iter.num_frames)
           frame = 0;           /* Select last */
     }

   if (!WebPDemuxGetFrame(demux, frame, &iter))
      goto quit;

   D("Frame=%d/%d X,Y=%d,%d WxH=%dx%d\n", iter.frame_num, iter.num_frames,
     iter.x_offset, iter.y_offset, iter.width, iter.height);

   WebPDemuxReleaseIterator(&iter);

   im->w = iter.width;
   im->h = iter.height;

   if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
      goto quit;

   UPDATE_FLAG(im->flags, F_HAS_ALPHA, iter.has_alpha);

   if (!load_data)
     {
        rc = LOAD_SUCCESS;
        goto quit;
     }

   /* Load data */

   if (!__imlib_AllocateData(im))
      goto quit;

   if (WebPDecodeBGRAInto
       (iter.fragment.bytes, iter.fragment.size, (uint8_t *) im->data,
        sizeof(DATA32) * im->w * im->h, im->w * 4) == NULL)
      goto quit;

   if (im->lc)
      __imlib_LoadProgressRows(im, 0, im->h);

   rc = LOAD_SUCCESS;

 quit:
   if (rc <= 0)
      __imlib_FreeData(im);
   if (demux)
      WebPDemuxDelete(demux);
   if (fdata != MAP_FAILED)
      munmap(fdata, im->fsize);

   return rc;
}

char
save(ImlibImage * im, ImlibProgressFunction progress, char progress_granularity)
{
   FILE               *f;
   int                 rc;
   ImlibImageTag      *quality_tag;
   float               quality;
   uint8_t            *fdata;
   size_t              encoded_size;

   f = fopen(im->real_file, "wb");
   if (!f)
      return LOAD_FAIL;

   rc = LOAD_FAIL;
   fdata = NULL;

   quality = 75;
   quality_tag = __imlib_GetTag(im, "quality");
   if (quality_tag)
     {
        quality = quality_tag->val;
        if (quality < 0)
          {
             fprintf(stderr,
                     "Warning: 'quality' setting %f too low for WebP, using 0\n",
                     quality);
             quality = 0;
          }

        if (quality > 100)
          {
             fprintf(stderr,
                     "Warning, 'quality' setting %f too high for WebP, using 100\n",
                     quality);
             quality = 100;
          }
     }

   encoded_size = WebPEncodeBGRA((uint8_t *) im->data, im->w, im->h,
                                 im->w * 4, quality, &fdata);

   if (fwrite(fdata, encoded_size, 1, f) != encoded_size)
      goto quit;

   rc = LOAD_SUCCESS;

 quit:
   if (fdata)
      WebPFree(fdata);
   fclose(f);

   return rc;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "webp" };
   __imlib_LoaderSetFormats(l, list_formats, ARRAY_SIZE(list_formats));
}
