/* Farbfeld (http://tools.suckless.org/farbfeld) */
#include "loader_common.h"

#include <stdint.h>
#include <arpa/inet.h>

#define mm_check(p) ((const char *)(p) <= (const char *)fdata + im->fsize)

typedef struct {
   unsigned char       magic[8];
   uint32_t            w, h;
} ff_hdr_t;

int
load2(ImlibImage * im, int load_data)
{
   int                 rc;
   void               *fdata;
   int                 rowlen, i, j;
   const ff_hdr_t     *hdr;
   const uint16_t     *row;
   uint8_t            *dat;

   rc = LOAD_FAIL;

   if (im->fsize < (long)sizeof(ff_hdr_t))
      return rc;

   fdata = mmap(NULL, im->fsize, PROT_READ, MAP_SHARED, fileno(im->fp), 0);
   if (fdata == MAP_FAILED)
      return LOAD_BADFILE;

   /* read and check the header */
   hdr = fdata;
   if (memcmp("farbfeld", hdr->magic, sizeof(hdr->magic)))
      goto quit;

   rc = LOAD_BADIMAGE;          /* Format accepted */

   im->w = ntohl(hdr->w);
   im->h = ntohl(hdr->h);
   if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
      goto quit;

   IM_FLAG_SET(im, F_HAS_ALPHA);

   if (!load_data)
      QUIT_WITH_RC(LOAD_SUCCESS);

   /* Load data */

   if (!__imlib_AllocateData(im))
      QUIT_WITH_RC(LOAD_OOM);

   rowlen = 4 * im->w;          /* RGBA */

   dat = (uint8_t *) im->data;
   row = (uint16_t *) (hdr + 1);
   for (i = 0; i < im->h; i++, dat += rowlen, row += rowlen)
     {
        if (!mm_check(row + rowlen))
           goto quit;

        for (j = 0; j < rowlen; j += 4)
          {
             /*
              * 16-Bit to 8-Bit (RGBA -> BGRA)
              * 255 * 257 = 65535 = 2^16-1 = UINT16_MAX
              */
             dat[j + 2] = ntohs(row[j + 0]) / 257;
             dat[j + 1] = ntohs(row[j + 1]) / 257;
             dat[j + 0] = ntohs(row[j + 2]) / 257;
             dat[j + 3] = ntohs(row[j + 3]) / 257;
          }

        if (im->lc && __imlib_LoadProgressRows(im, i, 1))
           QUIT_WITH_RC(LOAD_BREAK);
     }

   rc = LOAD_SUCCESS;

 quit:
   if (rc <= 0)
      __imlib_FreeData(im);
   munmap(fdata, im->fsize);

   return rc;
}

char
save(ImlibImage * im, ImlibProgressFunction progress, char progress_granularity)
{
   int                 rc;
   FILE               *f;
   size_t              rowlen, i, j;
   uint32_t            tmp32;
   uint16_t           *row;
   uint8_t            *dat;

   f = fopen(im->real_file, "wb");
   if (!f)
      return LOAD_FAIL;

   rc = LOAD_FAIL;
   row = NULL;

   /* write header */
   fputs("farbfeld", f);

   tmp32 = htonl(im->w);
   if (fwrite(&tmp32, sizeof(uint32_t), 1, f) != 1)
      goto quit;

   tmp32 = htonl(im->h);
   if (fwrite(&tmp32, sizeof(uint32_t), 1, f) != 1)
      goto quit;

   /* write data */
   rowlen = im->w * (sizeof("RGBA") - 1);
   row = malloc(rowlen * sizeof(uint16_t));
   if (!row)
      goto quit;

   dat = (uint8_t *) im->data;
   for (i = 0; i < (uint32_t) im->h; ++i, dat += rowlen)
     {
        for (j = 0; j < rowlen; j += 4)
          {
             /*
              * 8-Bit to 16-Bit
              * 255 * 257 = 65535 = 2^16-1 = UINT16_MAX
              */
             row[j + 0] = htons(dat[j + 2] * 257);
             row[j + 1] = htons(dat[j + 1] * 257);
             row[j + 2] = htons(dat[j + 0] * 257);
             row[j + 3] = htons(dat[j + 3] * 257);
          }
        if (fwrite(row, sizeof(uint16_t), rowlen, f) != rowlen)
           goto quit;

        if (im->lc && __imlib_LoadProgressRows(im, i, 1))
           QUIT_WITH_RC(LOAD_BREAK);
     }

   rc = LOAD_SUCCESS;

 quit:
   free(row);
   fclose(f);

   return rc;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "ff" };
   __imlib_LoaderSetFormats(l, list_formats, ARRAY_SIZE(list_formats));
}
