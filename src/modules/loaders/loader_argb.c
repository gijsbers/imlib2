#include "loader_common.h"

#include <sys/mman.h>

static struct {
   const unsigned char *data, *dptr;
   unsigned int        size;
} mdata;

static void
mm_init(void *src, unsigned int size)
{
   mdata.data = mdata.dptr = src;
   mdata.size = size;
}

static void
mm_seek(unsigned int offs)
{
   mdata.dptr = mdata.data + offs;
}

static int
mm_read(void *dst, unsigned int len)
{
   if (mdata.dptr + len > mdata.data + mdata.size)
      return 1;                 /* Out of data */

   memcpy(dst, mdata.dptr, len);
   mdata.dptr += len;

   return 0;
}

int
load2(ImlibImage * im, int load_data)
{
   int                 rc;
   void               *fdata;
   int                 alpha;
   DATA32             *ptr;
   int                 y;
   const char         *fptr, *row;
   unsigned int        size;

#ifdef WORDS_BIGENDIAN
   int                 l;
#endif

   rc = LOAD_FAIL;

   fdata = mmap(NULL, im->fsize, PROT_READ, MAP_SHARED, fileno(im->fp), 0);
   if (fdata == MAP_FAILED)
      return rc;

   mm_init(fdata, im->fsize);

   /* header */

   fptr = fdata;
   size = im->fsize > 31 ? 31 : im->fsize;      /* Look for \n in at most 31 byte */
   row = memchr(fptr, '\n', size);
   if (!row)
      goto quit;
   row += 1;                    /* After LF */

   im->w = im->h = alpha = 0;
   sscanf(fptr, "ARGB %i %i %i", &im->w, &im->h, &alpha);

   if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
      goto quit;

   UPDATE_FLAG(im->flags, F_HAS_ALPHA, alpha);

   if (!load_data)
     {
        rc = LOAD_SUCCESS;
        goto quit;
     }

   /* Load data */

   ptr = __imlib_AllocateData(im);
   if (!ptr)
      goto quit;

   mm_seek(row - fptr);

   for (y = 0; y < im->h; y++)
     {
        if (mm_read(ptr, 4 * im->w))
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
   if (fdata != MAP_FAILED)
      munmap(fdata, im->fsize);

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
   free(buf);
#endif
   fclose(f);

   return rc;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "argb", "arg" };
   __imlib_LoaderSetFormats(l, list_formats, ARRAY_SIZE(list_formats));
}
