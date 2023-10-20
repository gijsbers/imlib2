#include "loader_common.h"

int
load2(ImlibImage * im, int load_data)
{
   int                 rc;
   int                 w = 0, h = 0, alpha = 0;
   DATA32             *ptr;
   int                 y;
#ifdef WORDS_BIGENDIAN
   int                 l;
#endif

   rc = LOAD_FAIL;

   /* header */
   {
      char                buf[256], buf2[256];

      buf[0] = '\0';
      if (!fgets(buf, 255, im->fp))
         goto quit;

      buf2[0] = '\0';
      sscanf(buf, "%s %i %i %i", buf2, &w, &h, &alpha);
      if (strcmp(buf2, "ARGB"))
         goto quit;

      if (!IMAGE_DIMENSIONS_OK(w, h))
         goto quit;

      im->w = w;
      im->h = h;
      if (alpha)
         SET_FLAG(im->flags, F_HAS_ALPHA);
      else
         UNSET_FLAG(im->flags, F_HAS_ALPHA);
   }

   if (!load_data)
     {
        rc = LOAD_SUCCESS;
        goto quit;
     }

   /* Load data */

   ptr = __imlib_AllocateData(im);
   if (!ptr)
      goto quit;

   for (y = 0; y < h; y++)
     {
        if (fread(ptr, im->w, 4, im->fp) != 4)
           goto quit;

#ifdef WORDS_BIGENDIAN
        for (l = 0; l < im->w; l++)
           SWAP_LE_32_INPLACE(ptr[l]);
#endif
        ptr += im->w;

        if (im->lc && __imlib_LoadProgressRows(im, y, 1))
          {
             rc = LOAD_BREAK;
             goto quit;
          }
     }

   rc = LOAD_SUCCESS;

 quit:
   if (rc <= 0)
      __imlib_FreeData(im);

   return rc;
}

char
save(ImlibImage * im, ImlibProgressFunction progress, char progress_granularity)
{
   int                 rc;
   FILE               *f;
   DATA32             *ptr;
   int                 y, alpha = 0;

#ifdef WORDS_BIGENDIAN
   DATA32             *buf = (DATA32 *) malloc(im->w * 4);
#endif

   f = fopen(im->real_file, "wb");
   if (!f)
      return LOAD_FAIL;

   if (im->flags & F_HAS_ALPHA)
      alpha = 1;

   fprintf(f, "ARGB %i %i %i\n", im->w, im->h, alpha);

   ptr = im->data;
   for (y = 0; y < im->h; y++)
     {
#ifdef WORDS_BIGENDIAN
        {
           int                 x;

           memcpy(buf, ptr, im->w * 4);
           for (x = 0; x < im->w; x++)
              SWAP_LE_32_INPLACE(buf[x]);
           fwrite(buf, im->w, 4, f);
        }
#else
        fwrite(ptr, im->w, 4, f);
#endif
        ptr += im->w;

        if (im->lc && __imlib_LoadProgressRows(im, y, 1))
          {
             rc = LOAD_BREAK;
             goto quit;
          }
     }

   rc = LOAD_SUCCESS;

 quit:
   /* finish off */
#ifdef WORDS_BIGENDIAN
   if (buf)
      free(buf);
#endif
   fclose(f);

   return rc;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "argb", "arg" };

   __imlib_LoaderSetFormats(l, list_formats,
                            sizeof(list_formats) / sizeof(char *));
}
