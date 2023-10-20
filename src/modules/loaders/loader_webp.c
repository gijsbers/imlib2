#include "loader_common.h"

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

   fdata = mmap(NULL, im->fsize, PROT_READ, MAP_SHARED, fileno(im->fp), 0);
   if (fdata == MAP_FAILED)
      return LOAD_BADFILE;

   webp_data.bytes = fdata;
   webp_data.size = im->fsize;

   /* Init (includes signature check) */
   demux = WebPDemux(&webp_data);
   if (!demux)
      goto quit;

   rc = LOAD_BADIMAGE;          /* Format accepted */

   frame = 1;
   if (im->frame_num > 0)
     {
        frame = im->frame_num;
        im->frame_count = WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT);
        if (im->frame_count > 1)
           im->frame_flags |= FF_IMAGE_ANIMATED;
        im->canvas_w = WebPDemuxGetI(demux, WEBP_FF_CANVAS_WIDTH);
        im->canvas_h = WebPDemuxGetI(demux, WEBP_FF_CANVAS_HEIGHT);

        D("Canvas WxH=%dx%d frames=%d\n",
          im->canvas_w, im->canvas_h, im->frame_count);

        if (frame > 1 && frame > im->frame_count)
           QUIT_WITH_RC(LOAD_BADFRAME);
     }

   if (!WebPDemuxGetFrame(demux, frame, &iter))
      goto quit;

   WebPDemuxReleaseIterator(&iter);

   im->w = iter.width;
   im->h = iter.height;
   im->frame_x = iter.x_offset;
   im->frame_y = iter.y_offset;
   im->frame_delay = iter.duration;
   if (iter.dispose_method == WEBP_MUX_DISPOSE_BACKGROUND)
      im->frame_flags |= FF_FRAME_DISPOSE_CLEAR;
   if (iter.blend_method == WEBP_MUX_BLEND)
      im->frame_flags |= FF_FRAME_BLEND;

   D("Canvas WxH=%dx%d frame=%d/%d X,Y=%d,%d WxH=%dx%d alpha=%d T=%d dm=%d co=%d bl=%d\n",      //
     im->canvas_w, im->canvas_h, iter.frame_num, im->frame_count,
     im->frame_x, im->frame_y, im->w, im->h, iter.has_alpha,
     im->frame_delay, iter.dispose_method, iter.complete, iter.blend_method);

   if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
      goto quit;

   IM_FLAG_UPDATE(im, F_HAS_ALPHA, iter.has_alpha);

   if (!load_data)
      QUIT_WITH_RC(LOAD_SUCCESS);

   /* Load data */

   if (!__imlib_AllocateData(im))
      QUIT_WITH_RC(LOAD_OOM);

   if (WebPDecodeBGRAInto
       (iter.fragment.bytes, iter.fragment.size, (uint8_t *) im->data,
        sizeof(DATA32) * im->w * im->h, im->w * 4) == NULL)
      goto quit;

   if (im->lc)
      __imlib_LoadProgress(im, im->frame_x, im->frame_y, im->w, im->h);

   rc = LOAD_SUCCESS;

 quit:
   if (rc <= 0)
      __imlib_FreeData(im);
   if (demux)
      WebPDemuxDelete(demux);
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
