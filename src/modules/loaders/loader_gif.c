#include "loader_common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gif_lib.h>

char
load(ImlibImage * im, ImlibProgressFunction progress, char progress_granularity,
     char immediate_load)
{
   static const int    intoffset[] = { 0, 4, 2, 1 };
   static const int    intjump[] = { 8, 8, 4, 2 };
   int                 rc;
   DATA32             *ptr;
   GifFileType        *gif;
   GifRowType         *rows;
   GifRecordType       rec;
   ColorMapObject     *cmap;
   int                 i, j, done, bg, r, g, b, w = 0, h = 0;
   float               per = 0.0, per_inc;
   int                 last_per = 0, last_y = 0;
   int                 transp;
   int                 fd;

   done = 0;
   rows = NULL;
   transp = -1;

   /* if immediate_load is 1, then dont delay image laoding as below, or */
   /* already data in this image - dont load it again */
   if (im->data)
      return 0;

   fd = open(im->real_file, O_RDONLY);
   if (fd < 0)
      return 0;

#if GIFLIB_MAJOR >= 5
   gif = DGifOpenFileHandle(fd, NULL);
#else
   gif = DGifOpenFileHandle(fd);
#endif
   if (!gif)
     {
        close(fd);
        return 0;
     }

   rc = 0;                      /* Failure */

   do
     {
        if (DGifGetRecordType(gif, &rec) == GIF_ERROR)
          {
             /* PrintGifError(); */
             rec = TERMINATE_RECORD_TYPE;
          }
        if ((rec == IMAGE_DESC_RECORD_TYPE) && (!done))
          {
             if (DGifGetImageDesc(gif) == GIF_ERROR)
               {
                  /* PrintGifError(); */
                  rec = TERMINATE_RECORD_TYPE;
                  break;
               }
             w = gif->Image.Width;
             h = gif->Image.Height;
             if (!IMAGE_DIMENSIONS_OK(w, h))
                goto quit2;

             rows = calloc(h, sizeof(GifRowType *));
             if (!rows)
                goto quit2;

             for (i = 0; i < h; i++)
               {
                  rows[i] = calloc(w, sizeof(GifPixelType));
                  if (!rows[i])
                     goto quit;
               }

             if (gif->Image.Interlace)
               {
                  for (i = 0; i < 4; i++)
                    {
                       for (j = intoffset[i]; j < h; j += intjump[i])
                         {
                            DGifGetLine(gif, rows[j], w);
                         }
                    }
               }
             else
               {
                  for (i = 0; i < h; i++)
                    {
                       DGifGetLine(gif, rows[i], w);
                    }
               }
             done = 1;
          }
        else if (rec == EXTENSION_RECORD_TYPE)
          {
             int                 ext_code;
             GifByteType        *ext;

             ext = NULL;
             DGifGetExtension(gif, &ext_code, &ext);
             while (ext)
               {
                  if ((ext_code == 0xf9) && (ext[1] & 1) && (transp < 0))
                    {
                       transp = (int)ext[4];
                    }
                  ext = NULL;
                  DGifGetExtensionNext(gif, &ext);
               }
          }
     }
   while (rec != TERMINATE_RECORD_TYPE);

   if (transp >= 0)
     {
        SET_FLAG(im->flags, F_HAS_ALPHA);
     }
   else
     {
        UNSET_FLAG(im->flags, F_HAS_ALPHA);
     }
   if (!rows)
     {
        goto quit2;
     }

   /* set the format string member to the lower-case full extension */
   /* name for the format - so example names would be: */
   /* "png", "jpeg", "tiff", "ppm", "pgm", "pbm", "gif", "xpm" ... */
   im->w = w;
   im->h = h;
   if (!im->format)
      im->format = strdup("gif");

   if (im->loader || immediate_load || progress)
     {
        DATA32              colormap[256];

        bg = gif->SBackGroundColor;
        cmap = (gif->Image.ColorMap ? gif->Image.ColorMap : gif->SColorMap);
        memset(colormap, 0, sizeof(colormap));
        if (cmap != NULL)
          {
             for (i = cmap->ColorCount > 256 ? 256 : cmap->ColorCount; i-- > 0;)
               {
                  r = cmap->Colors[i].Red;
                  g = cmap->Colors[i].Green;
                  b = cmap->Colors[i].Blue;
                  colormap[i] = (0xff << 24) | (r << 16) | (g << 8) | b;
               }
             /* if bg > cmap->ColorCount, it is transparent black already */
             if (transp >= 0 && transp < 256)
                colormap[transp] = bg >= 0 && bg < 256 ?
                   colormap[bg] & 0x00ffffff : 0x00000000;
          }
        im->data = (DATA32 *) malloc(sizeof(DATA32) * w * h);
        if (!im->data)
           goto quit;

        ptr = im->data;
        per_inc = 100.0 / (float)h;
        for (i = 0; i < h; i++)
          {
             for (j = 0; j < w; j++)
               {
                  *ptr++ = colormap[rows[i][j]];
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

        if (progress)
           progress(im, 100, 0, last_y, w, h);
     }

   rc = 1;                      /* Success */

 quit:
   for (i = 0; i < h; i++)
      free(rows[i]);
   free(rows);

 quit2:
#if GIFLIB_MAJOR > 5 || (GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 1)
   DGifCloseFile(gif, NULL);
#else
   DGifCloseFile(gif);
#endif

   return rc;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "gif" };
   int                 i;

   l->num_formats = sizeof(list_formats) / sizeof(char *);
   l->formats = malloc(sizeof(char *) * l->num_formats);

   for (i = 0; i < l->num_formats; i++)
      l->formats[i] = strdup(list_formats[i]);
}
