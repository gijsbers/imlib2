#include "config.h"
#include "Imlib2_Loader.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <libraw.h>
#pragma GCC diagnostic pop

#define DBG_PFX "LDR-raw"

static const char  *const _formats[] = { "raw",
   "arw", "cr2", "dcr", "dng", "nef", "orf", "raf", "rw2", "rwl", "srw"
};

#define _LIBRAW_ERROR(err) (err != LIBRAW_SUCCESS && LIBRAW_FATAL_ERROR(err))

/* Purely based on inspection of some raw file samples */
static int
_sig_check(const uint8_t * data, unsigned int size)
{
   if (size < 1024)
      return 1;                 /* Not likely */

   if ((data[0] == 'I' && data[1] == 'I') || (data[0] == 'M' && data[1] == 'M'))
      return 0;                 /* Ok - TIFF-like */

   if (memcmp(data, "FUJI", 4) == 0)
      return 0;                 /* Ok - May be Fuji raw */

   return 1;
}

static int
_load(ImlibImage * im, int load_data)
{
   int                 rc, err;
   libraw_data_t      *raw_data;
   libraw_processed_image_t *image;
   int                 i, j;
   uint8_t            *imdata;
   const uint8_t      *bufptr;

   rc = LOAD_FAIL;
   raw_data = NULL;
   image = NULL;

   /* Signature check (avoid LibRaw constructor) */
   if (_sig_check(im->fi->fdata, im->fi->fsize))
      goto quit;

   raw_data = libraw_init(0);
   if (!raw_data)
      goto quit;

#if 0                           /* TBD - Gives rather large speedup */
   raw_data->params.half_size = 1;
#endif

   err = libraw_open_buffer(raw_data, im->fi->fdata, im->fi->fsize);
   if (_LIBRAW_ERROR(err))
      goto quit;

   if (!load_data)
     {
        err = libraw_adjust_sizes_info_only(raw_data);
        if (_LIBRAW_ERROR(err))
           goto quit;

        im->w = raw_data->sizes.iwidth;
        im->h = raw_data->sizes.iheight;
        D("size = %dx%d\n", im->w, im->h);

        if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
           goto quit;
        QUIT_WITH_RC(LOAD_SUCCESS);
     }

   /* Load data */

   err = libraw_unpack(raw_data);
   if (_LIBRAW_ERROR(err))
      goto quit;

   err = libraw_dcraw_process(raw_data);
   if (_LIBRAW_ERROR(err))
      goto quit;

   image = libraw_dcraw_make_mem_image(raw_data, &err);
   if (!image)
      goto quit;

   im->w = image->width;
   im->h = image->height;
   D("Colors=%d size = %dx%d\n", image->colors, im->w, im->h);

   if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
      goto quit;

   if (image->colors != 3)
      goto quit;

   if (!__imlib_AllocateData(im))
      QUIT_WITH_RC(LOAD_OOM);

   imdata = (uint8_t *) im->data;
   bufptr = image->data;
   for (i = 0; i < im->h; i++)
     {
        for (j = 0; j < im->w; j++, imdata += 4, bufptr += 3)
          {
             imdata[0] = bufptr[2];
             imdata[1] = bufptr[1];
             imdata[2] = bufptr[0];
             imdata[3] = 0xff;
          }

        if (im->lc && __imlib_LoadProgressRows(im, i, 1))
           QUIT_WITH_RC(LOAD_BREAK);
     }

   rc = LOAD_SUCCESS;

 quit:
   free(image);
   if (raw_data)
      libraw_close(raw_data);

   return rc;
}

IMLIB_LOADER(_formats, _load, NULL);
