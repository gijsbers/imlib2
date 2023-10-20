#include "loader_common.h"

#include <gif_lib.h>

#define DBG_PFX "LDR-gif"

static struct {
   const unsigned char *data, *dptr;
   unsigned int        size;
} mdata;

static void
mm_init(const void *src, unsigned int size)
{
   mdata.data = mdata.dptr = src;
   mdata.size = size;
}

static int
mm_read(GifFileType * gif, GifByteType * dst, int len)
{
   if (mdata.dptr + len > mdata.data + mdata.size)
      return -1;                /* Out of data */

   memcpy(dst, mdata.dptr, len);
   mdata.dptr += len;

   return len;
}

static void
make_colormap(uint32_t * cmi, ColorMapObject * cmg, int bg, int tr)
{
   int                 i, r, g, b;

   memset(cmi, 0, 256 * sizeof(uint32_t));
   if (!cmg)
      return;

   for (i = cmg->ColorCount > 256 ? 256 : cmg->ColorCount; i-- > 0;)
     {
        r = cmg->Colors[i].Red;
        g = cmg->Colors[i].Green;
        b = cmg->Colors[i].Blue;
        cmi[i] = PIXEL_ARGB(0xff, r, g, b);
     }

   /* if bg > cmg->ColorCount, it is transparent black already */
   if (tr >= 0 && tr < 256)
      cmi[tr] = bg >= 0 && bg < 256 ? cmi[bg] & 0x00ffffff : 0x00000000;
}

int
load2(ImlibImage * im, int load_data)
{
   int                 rc, err;
   void               *fdata;
   uint32_t           *ptr;
   GifFileType        *gif;
   GifRowType         *rows;
   GifRecordType       rec;
   ColorMapObject     *cmap;
   int                 i, j, bg, bits;
   int                 transp;
   uint32_t            colormap[256];
   int                 fcount, frame, multiframe;

   fdata = mmap(NULL, im->fsize, PROT_READ, MAP_SHARED, fileno(im->fp), 0);
   if (fdata == MAP_FAILED)
      return LOAD_BADFILE;

   rc = LOAD_FAIL;
   rows = NULL;

   mm_init(fdata, im->fsize);

#if GIFLIB_MAJOR >= 5
   gif = DGifOpen(NULL, mm_read, &err);
#else
   gif = DGifOpen(NULL, mm_read);
#endif
   if (!gif)
      goto quit;

   rc = LOAD_BADIMAGE;          /* Format accepted */

   transp = -1;
   fcount = 0;
   frame = 1;
   if (im->frame_num > 0)
     {
        frame = im->frame_num;
        im->frame_count = gif->ImageCount;
        if (im->frame_count > 1)
           im->frame_flags |= FF_IMAGE_ANIMATED;
        im->canvas_w = gif->SWidth;
        im->canvas_h = gif->SHeight;

        D("Canvas WxH=%dx%d frames=%d\n",
          im->canvas_w, im->canvas_h, im->frame_count);

#if 0
        if (frame > 1 && frame > im->frame_count)
           goto quit;
#endif
     }

   bg = gif->SBackGroundColor;
   cmap = gif->SColorMap;

   for (;;)
     {
        if (DGifGetRecordType(gif, &rec) == GIF_ERROR)
          {
             /* PrintGifError(); */
             DL("err1\n");
             break;
          }

        if (rec == TERMINATE_RECORD_TYPE)
          {
             DL(" TERMINATE_RECORD_TYPE(%d)\n", rec);
             break;
          }

        if (rec == IMAGE_DESC_RECORD_TYPE)
          {
             if (DGifGetImageDesc(gif) == GIF_ERROR)
               {
                  /* PrintGifError(); */
                  DL("err2\n");
                  break;
               }

             DL(" IMAGE_DESC_RECORD_TYPE(%d): ic=%d x,y=%d,%d wxh=%dx%d\n",
                rec, gif->ImageCount, gif->Image.Left, gif->Image.Top,
                gif->Image.Width, gif->Image.Height);

             fcount += 1;

             if (gif->ImageCount != frame)
               {
                  int                 size = 0;
                  GifByteType        *data;

                  if (DGifGetCode(gif, &size, &data) == GIF_ERROR)
                     goto quit;
                  DL("DGifGetCode: size=%d data=%p\n", size, data);
                  while (data)
                    {
                       if (DGifGetCodeNext(gif, &data) == GIF_ERROR)
                          goto quit;
                       DL(" DGifGetCodeNext: size=%d data=%p\n", size, data);
                    }
                  continue;
               }

             im->w = gif->Image.Width;
             im->h = gif->Image.Height;
             im->frame_x = gif->Image.Left;
             im->frame_y = gif->Image.Top;

             if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
                goto quit;

             D("Canvas WxH=%dx%d frame=%d/%d X,Y=%d,%d WxH=%dx%d\n",
               im->canvas_w, im->canvas_h, gif->ImageCount, im->frame_count,
               im->frame_x, im->frame_y, im->w, im->h);

             DL(" CM S=%p I=%p\n", cmap, gif->Image.ColorMap);
             if (gif->Image.ColorMap)
                cmap = gif->Image.ColorMap;
             if (load_data)
                make_colormap(colormap, cmap, bg, transp);

             rows = calloc(im->h, sizeof(GifRowType));
             if (!rows)
                QUIT_WITH_RC(LOAD_OOM);

             for (i = 0; i < im->h; i++)
               {
                  rows[i] = calloc(im->w, sizeof(GifPixelType));
                  if (!rows[i])
                     QUIT_WITH_RC(LOAD_OOM);
               }

             if (gif->Image.Interlace)
               {
                  static const int    intoffset[] = { 0, 4, 2, 1 };
                  static const int    intjump[] = { 8, 8, 4, 2 };
                  for (i = 0; i < 4; i++)
                    {
                       for (j = intoffset[i]; j < im->h; j += intjump[i])
                         {
                            DGifGetLine(gif, rows[j], im->w);
                         }
                    }
               }
             else
               {
                  for (i = 0; i < im->h; i++)
                    {
                       DGifGetLine(gif, rows[i], im->w);
                    }
               }

             /* Break if no specific frame was requested */
             if (im->frame_num == 0)
                break;
          }
        else if (rec == EXTENSION_RECORD_TYPE)
          {
             int                 ext_code, disp;
             GifByteType        *ext;

             ext = NULL;
             DGifGetExtension(gif, &ext_code, &ext);
             while (ext)
               {
                  DL(" EXTENSION_RECORD_TYPE(%d): ic=%d: ext_code=%02x: %02x %02x %02x %02x %02x\n",    //
                     rec, gif->ImageCount, ext_code,
                     ext[0], ext[1], ext[2], ext[3], ext[4]);
                  if (ext_code == GRAPHICS_EXT_FUNC_CODE
                      && gif->ImageCount == frame - 1)
                    {
                       bits = ext[1];
                       im->frame_delay = 10 * (0x100 * ext[3] + ext[2]);
                       if (bits & 1)
                          transp = ext[4];
                       disp = (bits >> 2) & 0x7;
                       if (disp == 2 || disp == 3)
                          im->frame_flags |= FF_FRAME_DISPOSE_CLEAR;
                       im->frame_flags |= FF_FRAME_BLEND;
                       D(" Frame %d: disp=%d ui=%d tr=%d, delay=%d transp = #%02x\n",   //
                         gif->ImageCount + 1, disp, (bits >> 1) & 1, bits & 1,
                         im->frame_delay, transp);
                    }
                  ext = NULL;
                  DGifGetExtensionNext(gif, &ext);
               }
          }
        else
          {
             DL(" Unknown record type(%d)\n", rec);
          }
     }

   IM_FLAG_UPDATE(im, F_HAS_ALPHA, transp >= 0);
   im->frame_count = fcount;
   multiframe = im->frame_count > 1;
   if (multiframe)
      im->frame_flags |= FF_IMAGE_ANIMATED;

   if (!rows)
     {
        if (frame > 1 && frame > im->frame_count)
           QUIT_WITH_RC(LOAD_BADFRAME);

        goto quit;
     }

   if (!load_data)
      QUIT_WITH_RC(LOAD_SUCCESS);

   /* Load data */

   ptr = __imlib_AllocateData(im);
   if (!ptr)
      QUIT_WITH_RC(LOAD_OOM);

   for (i = 0; i < im->h; i++)
     {
        for (j = 0; j < im->w; j++)
          {
             *ptr++ = colormap[rows[i][j]];
          }

        if (!multiframe && im->lc && __imlib_LoadProgressRows(im, i, 1))
           QUIT_WITH_RC(LOAD_BREAK);
     }

   if (multiframe && im->lc)
      __imlib_LoadProgress(im, im->frame_x, im->frame_y, im->w, im->h);

   rc = LOAD_SUCCESS;

 quit:
   if (rows)
     {
        for (i = 0; i < im->h; i++)
           free(rows[i]);
        free(rows);
     }

#if GIFLIB_MAJOR > 5 || (GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 1)
   DGifCloseFile(gif, NULL);
#else
   DGifCloseFile(gif);
#endif

   munmap(fdata, im->fsize);

   return rc;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "gif" };
   __imlib_LoaderSetFormats(l, list_formats, ARRAY_SIZE(list_formats));
}
