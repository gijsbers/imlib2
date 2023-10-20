/*
 * XBM loader
 */
#include "loader_common.h"

#define DEBUG 0
#if DEBUG
#define D(fmt...) fprintf(stdout, "XBM loader: " fmt)
#else
#define D(fmt...)
#endif

static const DATA32 _bitmap_colors[2] = { 0xffffffff, 0xff000000 };

static              DATA32
_bitmap_color(int bit)
{
   return _bitmap_colors[!!bit];
}

static int
_bitmap_dither(int x, int y, DATA32 pixel)
{
   static const DATA8  _dither_44[4][4] = {
   /**INDENT-OFF**/
      { 0, 32,  8, 40},
      {48, 16, 56, 24},
      {12, 44,  4, 36},
      {60, 28, 52, 20},
   /**INDENT-ON**/
   };
   int                 val, set;

   if (PIXEL_A(pixel) < 0x80)
     {
        set = 0;
     }
   else
     {
        val = (PIXEL_R(pixel) + PIXEL_G(pixel) + PIXEL_B(pixel)) / 12;
        set = val <= _dither_44[x & 0x3][y & 0x3];
     }

   return set;
}

int
load2(ImlibImage * im, int load_data)
{
   int                 rc;
   char                buf[4096], tok1[1024], tok2[1024];
   DATA32             *ptr, pixel;
   int                 i, x, y, bit;
   char               *s;
   int                 header, val, nlen;

   rc = LOAD_FAIL;
   ptr = NULL;
   x = y = 0;

   header = 1;
   for (;;)
     {
        s = fgets(buf, sizeof(buf), im->fp);
        D(">>>%s", buf);
        if (!s)
           break;

        if (header)
          {
             /* Header */
             tok1[0] = tok2[0] = '\0';
             val = -1;
             sscanf(buf, " %1023s %1023s %d", tok1, tok2, &val);
             D("'%s': '%s': %d\n", tok1, tok2, val);
             if (strcmp(tok1, "#define") == 0)
               {
                  if (tok2[0] == '\0')
                     goto quit;

                  nlen = strlen(tok2);
                  if (nlen > 6 && strcmp(tok2 + nlen - 6, "_width") == 0)
                    {
                       D("'%s' = %d\n", tok2, val);
                       im->w = val;
                    }
                  else if (nlen > 7 && strcmp(tok2 + nlen - 7, "_height") == 0)
                    {
                       D("'%s' = %d\n", tok2, val);
                       im->h = val;
                    }
               }
             else if (strcmp(tok1, "static") == 0)
               {
                  if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
                     goto quit;

                  if (!load_data)
                    {
                       rc = LOAD_SUCCESS;
                       goto quit;
                    }

                  UNSET_FLAG(im->flags, F_HAS_ALPHA);

                  header = 0;

                  ptr = __imlib_AllocateData(im);
                  if (!ptr)
                     goto quit;
               }
             else
               {
                  goto quit;
               }
          }
        else
          {
             /* Data */
             for (; *s != '\0';)
               {
                  nlen = -1;
                  sscanf(s, "%i%n", &val, &nlen);
                  D("Data %02x (%d)\n", val, nlen);
                  if (nlen < 0)
                     break;
                  s += nlen;
                  if (*s == ',')
                     s++;

                  for (i = 0; i < 8 && x < im->w; i++, x++)
                    {
                       bit = (val & (1 << i)) != 0;
                       pixel = _bitmap_color(bit);
                       *ptr++ = pixel;
                       D("i, x, y: %2d %2d %2d: %08x\n", i, x, y, pixel);
                    }

                  if (x >= im->w)
                    {
                       if (im->lc && __imlib_LoadProgressRows(im, y, 1))
                         {
                            rc = LOAD_BREAK;
                            goto quit;
                         }

                       x = 0;
                       y += 1;
                    }
               }
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
   FILE               *f;
   int                 rc;
   const char         *s, *name;
   char               *bname;
   int                 i, k, x, y, bits, nval, val;
   DATA32             *ptr;

   f = fopen(im->real_file, "wb");
   if (!f)
      return LOAD_FAIL;

   rc = LOAD_SUCCESS;

   name = im->real_file;
   if ((s = strrchr(name, '/')) != 0)
      name = s + 1;

   bname = strndup(name, strcspn(name, "."));

   fprintf(f, "#define %s_width %d\n", bname, im->w);
   fprintf(f, "#define %s_height %d\n", bname, im->h);
   fprintf(f, "static unsigned char %s_bits[] = {\n", bname);

   free(bname);

   nval = ((im->w + 7) / 8) * im->h;
   ptr = im->data;
   x = k = 0;
   for (y = 0; y < im->h;)
     {
        bits = 0;
        for (i = 0; i < 8 && x < im->w; i++, x++)
          {
             val = _bitmap_dither(x, y, *ptr++);
             if (val)
                bits |= 1 << i;
          }
        if (x >= im->w)
          {
             x = 0;
             y += 1;
          }
        k++;
        D("x, y = %2d,%2d: %d/%d\n", x, y, k, nval);
        fprintf(f, " 0x%02x%s%s", bits, k < nval ? "," : "",
                (k == nval) || ((k % 12) == 0) ? "\n" : "");
     }

   fprintf(f, "};\n");

   fclose(f);

   return rc;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "xbm" };

   __imlib_LoaderSetFormats(l, list_formats,
                            sizeof(list_formats) / sizeof(char *));
}
