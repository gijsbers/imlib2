#include "loader_common.h"

#include <ctype.h>
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

static int
mm_read(void *dst, unsigned int len)
{
   if (mdata.dptr + len > mdata.data + mdata.size)
      return 1;                 /* Out of data */

   memcpy(dst, mdata.dptr, len);
   mdata.dptr += len;

   return 0;
}

static int
mm_getc(void)
{
   int                 ch;

   if (mdata.dptr + 1 > mdata.data + mdata.size)
      return -1;                /* Out of data */

   ch = *mdata.dptr++;

   return ch;
}

static int
mm_getu(unsigned int *pui)
{
   int                 ch;
   int                 uval;

   for (;;)
     {
        ch = mm_getc();
        if (ch < 0)
           return ch;
        if (!isspace(ch))
           break;
     }

   if (!isdigit(ch))
      return -1;

   for (uval = 0;;)
     {
        uval = 10 * uval + ch - '0';
        ch = mm_getc();
        if (ch < 0)
           return ch;
        if (!isdigit(ch))
           break;
     }

   *pui = uval;
   return 0;                    /* Ok */
}

int
load2(ImlibImage * im, int load_data)
{
   int                 rc;
   void               *fdata;
   char                p = ' ', numbers = 3, count = 0;
   int                 w = 0, h = 0, v = 255, c = 0;
   char                buf[256];
   DATA8              *data = NULL;     /* for the binary versions */
   DATA8              *ptr = NULL;
   int                *idata = NULL;    /* for the ASCII versions */
   DATA32             *ptr2, rval, gval, bval;
   int                 i, j, x, y;

   rc = LOAD_FAIL;

   fdata = mmap(NULL, im->fsize, PROT_READ, MAP_SHARED, fileno(im->fp), 0);
   if (fdata == MAP_FAILED)
      return rc;

   mm_init(fdata, im->fsize);

   /* read the header info */

   rc = LOAD_FAIL;

   c = mm_getc();
   if (c != 'P')
      goto quit;

   p = mm_getc();
   if (p == '1' || p == '4')
      numbers = 2;              /* bitimages don't have max value */

   if ((p < '1') || (p > '8'))
      goto quit;

   count = 0;
   while (count < numbers)
     {
        c = mm_getc();

        if (c == EOF)
           goto quit;

        /* eat whitespace */
        while (isspace(c))
           c = mm_getc();
        /* if comment, eat that */
        if (c == '#')
          {
             do
                c = mm_getc();
             while (c != '\n' && c != EOF);
          }
        /* no comment -> proceed */
        else
          {
             i = 0;

             /* read numbers */
             while (c != EOF && !isspace(c) && (i < 255))
               {
                  buf[i++] = c;
                  c = mm_getc();
               }
             if (i)
               {
                  buf[i] = 0;
                  count++;
                  switch (count)
                    {
                       /* width */
                    case 1:
                       w = atoi(buf);
                       break;
                       /* height */
                    case 2:
                       h = atoi(buf);
                       break;
                       /* max value, only for color and greyscale */
                    case 3:
                       v = atoi(buf);
                       break;
                    }
               }
          }
     }
   if ((v < 0) || (v > 255))
      goto quit;

   im->w = w;
   im->h = h;
   if (!IMAGE_DIMENSIONS_OK(w, h))
      goto quit;

   UPDATE_FLAG(im->flags, F_HAS_ALPHA, p == '8');

   if (!load_data)
     {
        rc = LOAD_SUCCESS;
        goto quit;
     }

   /* Load data */

   ptr2 = __imlib_AllocateData(im);
   if (!ptr2)
      goto quit;

   /* start reading the data */
   switch (p)
     {
     case '1':                 /* ASCII monochrome */
        for (y = 0; y < h; y++)
          {
             for (x = 0; x < w; x++)
               {
                  if (mm_getu(&gval))
                     goto quit;

                  if (gval == 1)
                     *ptr2++ = 0xff000000;
                  else if (gval == 0)
                     *ptr2++ = 0xffffffff;
                  else
                     goto quit;
               }

             if (im->lc && __imlib_LoadProgressRows(im, y, 1))
                goto quit_progress;
          }
        break;
     case '2':                 /* ASCII greyscale */
        for (y = 0; y < h; y++)
          {
             for (x = 0; x < w; x++)
               {
                  if (mm_getu(&gval))
                     goto quit;

                  if (v == 0 || v == 255)
                    {
                       *ptr2++ = 0xff000000 | (gval << 16) | (gval << 8) | gval;
                    }
                  else
                    {
                       *ptr2++ =
                          0xff000000 | (((gval * 255) / v) << 16) |
                          (((gval * 255) / v) << 8) | ((gval * 255) / v);
                    }
               }

             if (im->lc && __imlib_LoadProgressRows(im, y, 1))
                goto quit_progress;
          }
        break;
     case '3':                 /* ASCII RGB */
        for (y = 0; y < h; y++)
          {
             for (x = 0; x < w; x++)
               {
                  if (mm_getu(&rval))
                     goto quit;
                  if (mm_getu(&gval))
                     goto quit;
                  if (mm_getu(&bval))
                     goto quit;

                  if (v == 0 || v == 255)
                    {
                       *ptr2++ = 0xff000000 | (rval << 16) | (gval << 8) | bval;
                    }
                  else
                    {
                       *ptr2++ =
                          0xff000000 |
                          (((rval * 255) / v) << 16) |
                          (((gval * 255) / v) << 8) | ((bval * 255) / v);
                    }
               }

             if (im->lc && __imlib_LoadProgressRows(im, y, 1))
                goto quit_progress;
          }
        break;
     case '4':                 /* binary 1bit monochrome */
        data = malloc((w + 7) / 8 * sizeof(DATA8));
        if (!data)
           goto quit;

        ptr2 = im->data;
        for (y = 0; y < h; y++)
          {
             if (mm_read(data, (w + 7) / 8))
                goto quit;

             ptr = data;
             for (x = 0; x < w; x += 8)
               {
                  j = (w - x >= 8) ? 8 : w - x;
                  for (i = 0; i < j; i++)
                    {
                       if (ptr[0] & (0x80 >> i))
                          *ptr2 = 0xff000000;
                       else
                          *ptr2 = 0xffffffff;
                       ptr2++;
                    }
                  ptr++;
               }

             if (im->lc && __imlib_LoadProgressRows(im, y, 1))
                goto quit_progress;
          }
        break;
     case '5':                 /* binary 8bit grayscale GGGGGGGG */
        data = malloc(1 * sizeof(DATA8) * w);
        if (!data)
           goto quit;

        ptr2 = im->data;
        for (y = 0; y < h; y++)
          {
             if (mm_read(data, w * 1))
                goto quit;

             ptr = data;
             if (v == 0 || v == 255)
               {
                  for (x = 0; x < w; x++)
                    {
                       *ptr2 =
                          0xff000000 | (ptr[0] << 16) | (ptr[0] << 8) | ptr[0];
                       ptr2++;
                       ptr++;
                    }
               }
             else
               {
                  for (x = 0; x < w; x++)
                    {
                       *ptr2 =
                          0xff000000 |
                          (((ptr[0] * 255) / v) << 16) |
                          (((ptr[0] * 255) / v) << 8) | ((ptr[0] * 255) / v);
                       ptr2++;
                       ptr++;
                    }
               }

             if (im->lc && __imlib_LoadProgressRows(im, y, 1))
                goto quit_progress;
          }
        break;
     case '6':                 /* 24bit binary RGBRGBRGB */
        data = malloc(3 * sizeof(DATA8) * w);
        if (!data)
           goto quit;

        ptr2 = im->data;
        for (y = 0; y < h; y++)
          {
             if (mm_read(data, w * 3))
                goto quit;

             ptr = data;
             if (v == 0 || v == 255)
               {
                  for (x = 0; x < w; x++)
                    {
                       *ptr2 =
                          0xff000000 | (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];
                       ptr2++;
                       ptr += 3;
                    }
               }
             else
               {
                  for (x = 0; x < w; x++)
                    {
                       *ptr2 =
                          0xff000000 |
                          (((ptr[0] * 255) / v) << 16) |
                          (((ptr[1] * 255) / v) << 8) | ((ptr[2] * 255) / v);
                       ptr2++;
                       ptr += 3;
                    }
               }

             if (im->lc && __imlib_LoadProgressRows(im, y, 1))
                goto quit_progress;
          }
        break;
     case '7':                 /* XV's 8bit 332 format */
        data = malloc(1 * sizeof(DATA8) * w);
        if (!data)
           goto quit;

        ptr2 = im->data;
        for (y = 0; y < h; y++)
          {
             if (mm_read(data, w * 1))
                goto quit;

             ptr = data;
             for (x = 0; x < w; x++)
               {
                  int                 r, g, b;

                  r = (*ptr >> 5) & 0x7;
                  g = (*ptr >> 2) & 0x7;
                  b = (*ptr) & 0x3;
                  *ptr2 =
                     0xff000000 |
                     (((r << 21) | (r << 18) | (r << 15)) & 0xff0000) |
                     (((g << 13) | (g << 10) | (g << 7)) & 0xff00) |
                     ((b << 6) | (b << 4) | (b << 2) | (b << 0));
                  ptr2++;
                  ptr++;
               }

             if (im->lc && __imlib_LoadProgressRows(im, y, 1))
                goto quit_progress;
          }
        break;
     case '8':                 /* 24bit binary RGBARGBARGBA */
        data = malloc(4 * sizeof(DATA8) * w);
        if (!data)
           goto quit;

        ptr2 = im->data;
        for (y = 0; y < h; y++)
          {
             if (mm_read(data, w * 4))
                goto quit;

             ptr = data;
             if (v == 0 || v == 255)
               {
                  for (x = 0; x < w; x++)
                    {
                       *ptr2 = PIXEL_ARGB(ptr[3], ptr[0], ptr[1], ptr[2]);
                       ptr2++;
                       ptr += 4;
                    }
               }
             else
               {
                  for (x = 0; x < w; x++)
                    {
                       *ptr2 =
                          PIXEL_ARGB((ptr[3] * 255) / v,
                                     (ptr[0] * 255) / v,
                                     (ptr[1] * 255) / v, (ptr[2] * 255) / v);
                       ptr2++;
                       ptr += 4;
                    }
               }

             if (im->lc && __imlib_LoadProgressRows(im, y, 1))
                goto quit_progress;
          }
        break;
     default:
        goto quit;
      quit_progress:
        rc = LOAD_BREAK;
        goto quit;
     }

   rc = LOAD_SUCCESS;

 quit:
   free(idata);
   free(data);
   if (fdata != MAP_FAILED)
      munmap(fdata, im->fsize);

   if (rc == 0)
      __imlib_FreeData(im);

   return rc;
}

char
save(ImlibImage * im, ImlibProgressFunction progress, char progress_granularity)
{
   int                 rc;
   FILE               *f;
   DATA8              *buf, *bptr;
   DATA32             *ptr;
   int                 x, y;

   f = fopen(im->real_file, "wb");
   if (!f)
      return LOAD_FAIL;

   rc = LOAD_FAIL;

   /* allocate a small buffer to convert image data */
   buf = malloc(im->w * 4 * sizeof(DATA8));
   if (!buf)
      goto quit;

   ptr = im->data;

   /* if the image has a useful alpha channel */
   if (im->flags & F_HAS_ALPHA)
     {
        fprintf(f, "P8\n" "# PNM File written by Imlib2\n" "%i %i\n" "255\n",
                im->w, im->h);
        for (y = 0; y < im->h; y++)
          {
             bptr = buf;
             for (x = 0; x < im->w; x++)
               {
                  DATA32              pixel = *ptr++;

                  bptr[0] = PIXEL_R(pixel);
                  bptr[1] = PIXEL_G(pixel);
                  bptr[2] = PIXEL_B(pixel);
                  bptr[3] = PIXEL_A(pixel);
                  bptr += 4;
               }
             fwrite(buf, im->w * 4, 1, f);

             if (im->lc && __imlib_LoadProgressRows(im, y, 1))
                goto quit_progress;
          }
     }
   else
     {
        fprintf(f, "P6\n" "# PNM File written by Imlib2\n" "%i %i\n" "255\n",
                im->w, im->h);
        for (y = 0; y < im->h; y++)
          {
             bptr = buf;
             for (x = 0; x < im->w; x++)
               {
                  DATA32              pixel = *ptr++;

                  bptr[0] = PIXEL_R(pixel);
                  bptr[1] = PIXEL_G(pixel);
                  bptr[2] = PIXEL_B(pixel);
                  bptr += 3;
               }
             fwrite(buf, im->w * 3, 1, f);

             if (im->lc && __imlib_LoadProgressRows(im, y, 1))
                goto quit_progress;
          }
     }

   rc = LOAD_SUCCESS;

 quit:
   /* finish off */
   free(buf);
   fclose(f);

   return rc;

 quit_progress:
   rc = LOAD_BREAK;
   goto quit;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] =
      { "pnm", "ppm", "pgm", "pbm", "pam" };
   __imlib_LoaderSetFormats(l, list_formats, ARRAY_SIZE(list_formats));
}
