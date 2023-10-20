#include "loader_common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static FILE        *rgb_txt = NULL;

static void
xpm_parse_color(char *color, DATA32 * pixel)
{
   char                buf[4096];
   int                 r, g, b;

   r = g = b = 0;

   /* is a #ff00ff like color */
   if (color[0] == '#')
     {
        int                 len;
        char                val[32];

        len = strlen(color) - 1;
        if (len < 96)
          {
             int                 i;

             len /= 3;
             for (i = 0; i < len; i++)
                val[i] = color[1 + i + (0 * len)];
             val[i] = 0;
             sscanf(val, "%x", &r);
             for (i = 0; i < len; i++)
                val[i] = color[1 + i + (1 * len)];
             val[i] = 0;
             sscanf(val, "%x", &g);
             for (i = 0; i < len; i++)
                val[i] = color[1 + i + (2 * len)];
             val[i] = 0;
             sscanf(val, "%x", &b);
             if (len == 1)
               {
                  r = (r << 4) | r;
                  g = (g << 4) | g;
                  b = (b << 4) | b;
               }
             else if (len > 2)
               {
                  r >>= (len - 2) * 4;
                  g >>= (len - 2) * 4;
                  b >>= (len - 2) * 4;
               }
          }
        goto done;
     }

   /* look in rgb txt database */
   if (!rgb_txt)
      rgb_txt = fopen("/usr/share/X11/rgb.txt", "r");
   if (!rgb_txt)
      rgb_txt = fopen("/usr/X11R6/lib/X11/rgb.txt", "r");
   if (!rgb_txt)
      rgb_txt = fopen("/usr/openwin/lib/X11/rgb.txt", "r");
   if (!rgb_txt)
      goto done;

   fseek(rgb_txt, 0, SEEK_SET);
   while (fgets(buf, 4000, rgb_txt))
     {
        if (buf[0] != '!')
          {
             int                 rr, gg, bb;
             char                name[4096];

             sscanf(buf, "%i %i %i %[^\n]", &rr, &gg, &bb, name);
             if (!strcasecmp(name, color))
               {
                  r = rr;
                  g = gg;
                  b = bb;
                  goto done;
               }
          }
     }
 done:
   *pixel = PIXEL_ARGB(0xff, r, g, b);
}

static void
xpm_parse_done(void)
{
   if (rgb_txt)
      fclose(rgb_txt);
   rgb_txt = NULL;
}

char
load(ImlibImage * im, ImlibProgressFunction progress, char progress_granularity,
     char immediate_load)
{
   int                 rc;
   DATA32             *ptr;
   FILE               *f;
   int                 pc, c, i, j, k, w, h, ncolors, cpp;
   int                 comment, transp, quote, context, len, done, backslash;
   char               *line, s[256], tok[256], col[256];
   int                 lsz = 256;
   struct _cmap {
      char                assigned;
      unsigned char       transp;
      char                str[6];
      DATA32              pixel;
   }                  *cmap;
   short               lookup[128 - 32][128 - 32];
   float               per = 0.0, per_inc = 0.0;
   int                 last_per = 0, last_y = 0;
   int                 count, pixels;

   rc = 0;
   done = 0;
   transp = -1;
   line = NULL;
   cmap = NULL;

   f = fopen(im->real_file, "rb");
   if (!f)
      return 0;

   len = fread(s, 1, sizeof(s) - 1, f);
   if (len < 9)
      goto quit;

   s[len] = '\0';
   if (!strstr(s, " XPM */"))
      goto quit;

   rewind(f);

   i = 0;
   j = 0;
   w = 10;
   h = 10;
   ncolors = 0;
   cpp = 0;
   ptr = NULL;
   c = ' ';
   comment = 0;
   quote = 0;
   context = 0;
   pixels = 0;
   count = 0;
   line = malloc(lsz);
   if (!line)
      goto quit;
   len = 0;

   backslash = 0;
   memset(lookup, 0, sizeof(lookup));
   while (!done)
     {
        pc = c;
        c = fgetc(f);
        if (c == EOF)
           break;

        if (!quote)
          {
             if ((pc == '/') && (c == '*'))
                comment = 1;
             else if ((pc == '*') && (c == '/') && (comment))
                comment = 0;
          }

        if (comment)
           continue;

        if ((!quote) && (c == '"'))
          {
             quote = 1;
             len = 0;
          }
        else if ((quote) && (c == '"'))
          {
             line[len] = 0;
             quote = 0;
             if (context == 0)
               {
                  /* Header */
                  sscanf(line, "%i %i %i %i", &w, &h, &ncolors, &cpp);
                  if ((ncolors > 32766) || (ncolors < 1))
                    {
                       fprintf(stderr,
                               "IMLIB ERROR: XPM files with colors > 32766 or < 1 not supported\n");
                       goto quit;
                    }
                  if ((cpp > 5) || (cpp < 1))
                    {
                       fprintf(stderr,
                               "IMLIB ERROR: XPM files with characters per pixel > 5 or < 1 not supported\n");
                       goto quit;
                    }
                  if (!IMAGE_DIMENSIONS_OK(w, h))
                    {
                       fprintf(stderr,
                               "IMLIB ERROR: Invalid image dimension: %dx%d\n",
                               w, h);
                       goto quit;
                    }
                  im->w = w;
                  im->h = h;

                  cmap = calloc(ncolors, sizeof(struct _cmap));
                  if (!cmap)
                     goto quit;

                  per_inc = 100.0 / (((float)w) * h);

                  if (im->loader || immediate_load || progress)
                    {
                       ptr = __imlib_AllocateData(im);
                       if (!ptr)
                          goto quit;
                       pixels = w * h;
                    }
                  else
                    {
                       rc = 1;
                       goto quit;
                    }

                  j = 0;
                  context++;
               }
             else if (context == 1)
               {
                  /* Color Table */
                  if (j < ncolors)
                    {
                       int                 slen;
                       int                 hascolor, iscolor;

                       iscolor = 0;
                       hascolor = 0;
                       tok[0] = 0;
                       col[0] = 0;
                       s[0] = 0;
                       if (len < cpp)
                          goto quit;
                       strncpy(cmap[j].str, line, cpp);
                       for (k = cpp; k < len; k++)
                         {
                            if (line[k] == ' ')
                               continue;

                            s[0] = 0;
                            sscanf(&line[k], "%255s", s);
                            slen = strlen(s);
                            k += slen;
                            if (!strcmp(s, "c"))
                               iscolor = 1;
                            if ((!strcmp(s, "m")) || (!strcmp(s, "s"))
                                || (!strcmp(s, "g4"))
                                || (!strcmp(s, "g"))
                                || (!strcmp(s, "c")) || (k >= len))
                              {
                                 if (k >= len)
                                   {
                                      if (col[0])
                                        {
                                           if (strlen(col) < (sizeof(col) - 2))
                                              strcat(col, " ");
                                           else
                                              done = 1;
                                        }
                                      if (strlen(col) + strlen(s) <
                                          (sizeof(col) - 1))
                                         strcat(col, s);
                                   }
                                 if (col[0])
                                   {
                                      if (!strcasecmp(col, "none"))
                                        {
                                           cmap[j].transp = 1;
                                           cmap[j].pixel = 0;
                                        }
                                      else if ((!cmap[j].assigned ||
                                                !strcmp(tok, "c"))
                                               && (!hascolor))
                                        {
                                           xpm_parse_color(col, &cmap[j].pixel);
                                           cmap[j].assigned = 1;
                                           cmap[j].transp = 0;
                                           if (iscolor)
                                              hascolor = 1;
                                        }
                                   }
                                 strcpy(tok, s);
                                 col[0] = 0;
                              }
                            else
                              {
                                 if (col[0])
                                   {
                                      if (strlen(col) < (sizeof(col) - 2))
                                         strcat(col, " ");
                                      else
                                         done = 1;
                                   }
                                 if (strlen(col) + strlen(s) <
                                     (sizeof(col) - 1))
                                    strcat(col, s);
                              }
                         }
                       if (cmap[j].transp)
                          transp = 1;
                    }
                  j++;
                  if (j >= ncolors)
                    {
                       if (cpp == 1)
                          for (i = 0; i < ncolors; i++)
                             lookup[(int)cmap[i].str[0] - 32][0] = i;
                       if (cpp == 2)
                          for (i = 0; i < ncolors; i++)
                             lookup[(int)cmap[i].str[0] -
                                    32][(int)cmap[i].str[1] - 32] = i;
                       context++;
                    }
               }
             else
               {
                  /* Image Data */

                  if (cpp == 1)
                    {
#define CM1(c0) (&cmap[lookup[c0 - ' '][0]])
                       for (i = 0; count < pixels && i < len; i++)
                         {
                            *ptr++ = CM1(line[i])->pixel;
                            count++;
                         }
                    }
                  else if (cpp == 2)
                    {
#define CM2(c0, c1) (&cmap[lookup[c0 - ' '][c1 - ' ']])
                       for (i = 0; count < pixels && i < len - 1; i += 2)
                         {
                            *ptr++ = CM2(line[i], line[i + 1])->pixel;
                            count++;
                         }
                    }
                  else
                    {
#define CMn(_j) (&cmap[_j])
                       for (i = 0; count < pixels && i < len - (cpp - 1);
                            i += cpp)
                         {
                            for (j = 0; j < cpp; j++)
                               col[j] = line[i + j];
                            col[j] = 0;
                            for (j = 0; j < ncolors; j++)
                              {
                                 if (!strcmp(col, cmap[j].str))
                                   {
                                      *ptr++ = CMn(j)->pixel;
                                      count++;
                                      j = ncolors;
                                   }
                              }
                         }
                    }
                  per += per_inc;
                  if (progress && (((int)per) != last_per)
                      && (((int)per) % progress_granularity == 0))
                    {
                       last_per = (int)per;
                       if (!(progress(im, (int)per, 0, last_y, w, i)))
                         {
                            rc = 2;
                            goto quit;
                         }
                       last_y = i;
                    }
               }
          }

        /* Scan in line from XPM file */
        if ((quote) && (c != '"'))
          {
             if (c < 32)
                c = 32;
             else if (c > 127)
                c = 127;
             if (c == '\\')
               {
                  if (++backslash < 2)
                    {
                       line[len++] = c;
                    }
                  else
                    {
                       backslash = 0;
                    }
               }
             else
               {
                  backslash = 0;
                  line[len++] = c;
               }
          }

        if (len >= lsz)
          {
             char               *nline;

             lsz += 256;
             nline = realloc(line, lsz);
             if (!nline)
                goto quit;
             line = nline;
          }

        if ((context > 1) && (count >= pixels))
           done = 1;
     }

   for (; count < pixels; count++)
     {
        /* Fill in missing pixels
         * (avoid working with uninitialized data in bad xpms) */
        im->data[count] = cmap[0].pixel;
     }

   if (transp >= 0)
     {
        SET_FLAG(im->flags, F_HAS_ALPHA);
     }
   else
     {
        UNSET_FLAG(im->flags, F_HAS_ALPHA);
     }

   if (progress)
     {
        progress(im, 100, 0, last_y, w, h);
     }

   rc = 1;

 quit:
   if (rc == 0)
      __imlib_FreeData(im);

   fclose(f);
   free(cmap);
   free(line);

   xpm_parse_done();

   return rc;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "xpm" };
   int                 i;

   l->num_formats = sizeof(list_formats) / sizeof(char *);
   l->formats = malloc(sizeof(char *) * l->num_formats);

   for (i = 0; i < l->num_formats; i++)
      l->formats[i] = strdup(list_formats[i]);
}
