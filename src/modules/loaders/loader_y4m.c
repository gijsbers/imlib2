/*
 * Loader for Y4M images.
 *
 * Only loads the image.
 */
#include "config.h"
#include "Imlib2_Loader.h"

#include <stdio.h>
#include <unistd.h>

#include <y4mTypes.h>
#include <liby4m.h>
#include <libyuv.h>

static const char  *const _formats[] = { "y4m" };

// Notes:
// * y4m does not support alpha channels.

// in the following code, "encoded" refers to YUV data (as present in a
// y4m file).
static int
_load(ImlibImage * im, int load_data)
{
   int                 rc = LOAD_FAIL;
   int                 broken_image = 0;
   uint32_t           *ptr = NULL;

   /* we do not support the file being loaded from memory */
   if (!im->fi->fp)
      goto quit;

   /* guess whether this is a y4m file */
   const char         *fptr = im->fi->fdata;

   if (strncmp(fptr, "YUV4MPEG2 ", 10) != 0)
      goto quit;

   /* format accepted */
   rc = LOAD_BADIMAGE;

   /* liby4m insists of getting control of the fp, so we will dup it */
   FILE               *fp2 = fdopen(dup(fileno(im->fi->fp)), "r");

   /* read the input file pointer */
   y4mFile_t           y4m_file;

   if (y4mOpenFp(&y4m_file, fp2))
     {
        broken_image = 1;
        goto clean;
     }

   /* read image info */
   im->w = y4mGetWidth(&y4m_file);
   im->h = y4mGetHeight(&y4m_file);
   if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
      goto clean;
   /* y4m does not support alpha channels */
   im->has_alpha = 0;

   /* check whether we are done */
   if (!load_data)
      goto clean;

   /* load data */
   /* allocate space for the raw image */
   ptr = __imlib_AllocateData(im);
   if (!ptr)
      goto clean;
   uint8_t            *data = (uint8_t *) ptr;
   char               *input_ptr = y4mGetFrameDataPointer(&y4m_file);

   /* convert input to ARGB (as expected by imlib) */
   if (y4m_file.colourspace == COLOUR_C422)
     {
        if (I422ToARGB((const uint8_t *)input_ptr,      // src_y,
                       im->w,   // src_stride_y
                       (const uint8_t *)(input_ptr + im->w * im->h),    // src_u
                       im->w / 2,       // src_stride_u
                       (const uint8_t *)(input_ptr + im->w * im->h * 3 / 2),    // src_v
                       im->w / 2,       // src_stride_v
                       data,    // dst_bgra
                       im->w * 4,       // dst_stride_bgra
                       im->w,   // width
                       im->h) != 0)     // height
           goto clean;

     }
   else if (y4m_file.colourspace == COLOUR_C444)
     {
        if (I444ToARGB((const uint8_t *)input_ptr,      // src_y,
                       im->w,   // src_stride_y
                       (const uint8_t *)(input_ptr + im->w * im->h),    // src_u
                       im->w,   // src_stride_u
                       (const uint8_t *)(input_ptr + im->w * im->h * 2),        // src_v
                       im->w,   // src_stride_v
                       data,    // dst_bgra
                       im->w * 4,       // dst_stride_bgra
                       im->w,   // width
                       im->h) != 0)     // height
           goto clean;

     }
   else
     {                          /* 4:2:0 */
        if (I420ToARGB((const uint8_t *)input_ptr,      // src_y,
                       im->w,   // src_stride_y
                       (const uint8_t *)(input_ptr + im->w * im->h),    // src_u
                       im->w / 2,       // src_stride_u
                       (const uint8_t *)(input_ptr + im->w * im->h * 5 / 4),    // src_v
                       im->w / 2,       // src_stride_v
                       data,    // dst_bgra
                       im->w * 4,       // dst_stride_bgra
                       im->w,   // width
                       im->h) != 0)     // height
           goto clean;
     }

   rc = LOAD_SUCCESS;

   /* implement progress callback */
   if (im->lc)
      __imlib_LoadProgressRows(im, 0, im->h);

 clean:
   y4mCloseFile(&y4m_file);
   if (broken_image == 1)
     {
        QUIT_WITH_RC(LOAD_BADFILE);
     }
   if (!load_data)
      QUIT_WITH_RC(LOAD_SUCCESS);
   if (!ptr)
      QUIT_WITH_RC(LOAD_OOM);
 quit:
   return rc;
}

IMLIB_LOADER(_formats, _load, NULL);
