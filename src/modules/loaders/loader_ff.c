/* Farbfeld (http://tools.suckless.org/farbfeld) */
#include <arpa/inet.h>
#include <stdint.h>

#include "loader_common.h"

#define LEN(x) (sizeof((x)) / sizeof(*(x)))

int
load2(ImlibImage * im, int load_data)
{
   int                 rc;
   size_t              rowlen, i, j;
   uint32_t            hdr[2 + 1 + 1], w, h;
   uint16_t           *row;
   uint8_t            *dat;

   rc = LOAD_FAIL;
   row = NULL;

   /* read and check the header */
   if (fread(hdr, sizeof(uint32_t), LEN(hdr), im->fp) != LEN(hdr) ||
       memcmp("farbfeld", hdr, sizeof("farbfeld") - 1))
      goto quit;

   im->w = ntohl(hdr[2]);
   im->h = ntohl(hdr[3]);
   if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
      goto quit;

   SET_FLAG(im->flags, F_HAS_ALPHA);

   if (!load_data)
     {
        rc = LOAD_SUCCESS;
        goto quit;
     }

   /* Load data */

   w = im->w;
   h = im->h;
   rowlen = w * (sizeof("RGBA") - 1);

   if (!__imlib_AllocateData(im))
      goto quit;

   row = malloc(rowlen * sizeof(uint16_t));
   if (!row)
      goto quit;

   dat = (uint8_t *) im->data;
   for (i = 0; i < h; i++, dat += rowlen)
     {
        if (fread(row, sizeof(uint16_t), rowlen, im->fp) != (size_t)rowlen)
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
          {
             rc = LOAD_BREAK;
             goto quit;
          }
     }

   rc = LOAD_SUCCESS;

 quit:
   free(row);
   if (rc <= 0)
      __imlib_FreeData(im);

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
          {
             rc = LOAD_BREAK;
             goto quit;
          }
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
   __imlib_LoaderSetFormats(l, list_formats,
                            sizeof(list_formats) / sizeof(char *));
}
