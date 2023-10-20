/*
 * ICO loader
 *
 * ICO(/BMP) file format:
 * https://en.wikipedia.org/wiki/ICO_(file_format)
 * https://en.wikipedia.org/wiki/BMP_file_format
 */
#include "loader_common.h"

#include <string.h>

#define DEBUG 0
#if DEBUG
#define D(fmt...) fprintf(stdout, "ICO loader: " fmt)
#else
#define D(fmt...)
#endif

/* The ICONDIR */
typedef struct {
   DATA16              rsvd;
   DATA16              type;
   DATA16              icons;
} idir_t;

/* The ICONDIRENTRY */
typedef struct {
   DATA8               width;
   DATA8               height;
   DATA8               colors;
   DATA8               rsvd;
   DATA16              planes;
   DATA16              bpp;
   DATA32              size;
   DATA32              offs;
} ide_t;

/* The BITMAPINFOHEADER */
typedef struct {
   DATA32              header_size;
   DATA32              width;
   DATA32              height;
   DATA16              planes;
   DATA16              bpp;
   DATA32              compression;
   DATA32              size;
   DATA32              res_hor;
   DATA32              res_ver;
   DATA32              colors;
   DATA32              colors_important;
} bih_t;

typedef struct {
   ide_t               ide;     /* ICONDIRENTRY     */
   bih_t               bih;     /* BITMAPINFOHEADER */

   unsigned short      w;
   unsigned short      h;

   DATA32             *cmap;    /* Colormap (bpp <= 8) */
   DATA8              *pxls;    /* Pixel data */
   DATA8              *mask;    /* Bitmask    */
} ie_t;

typedef struct {
   FILE               *fp;
   idir_t              idir;    /* ICONDIR */
   ie_t               *ie;      /* Icon entries */
} ico_t;

static void
ico_delete(ico_t * ico)
{
   int                 i;

   if (ico->fp)
      fclose(ico->fp);

   if (ico->ie)
     {
        for (i = 0; i < ico->idir.icons; i++)
          {
             free(ico->ie[i].cmap);
             free(ico->ie[i].pxls);
             free(ico->ie[i].mask);
          }
        free(ico->ie);
     }

   free(ico);
}

static void
ico_read_idir(ico_t * ico, int ino)
{
   ie_t               *ie;
   unsigned int        nr;

   ie = &ico->ie[ino];

   fseek(ico->fp, sizeof(idir_t) + ino * sizeof(ide_t), SEEK_SET);
   nr = fread(&ie->ide, 1, sizeof(ie->ide), ico->fp);
   if (nr != sizeof(ie->ide))
      return;

   ie->w = (ie->ide.width > 0) ? ie->ide.width : 256;
   ie->h = (ie->ide.height > 0) ? ie->ide.height : 256;

   SWAP_LE_16_INPLACE(ie->ide.planes);
   SWAP_LE_16_INPLACE(ie->ide.bpp);

   SWAP_LE_32_INPLACE(ie->ide.size);
   SWAP_LE_32_INPLACE(ie->ide.offs);

   D("Entry %2d: Idir: WxHxD = %dx%dx%d, colors = %d\n",
     ino, ie->w, ie->h, ie->ide.bpp, ie->ide.colors);
}

static void
ico_read_icon(ico_t * ico, int ino)
{
   ie_t               *ie;
   unsigned int        nr, size;

   ie = &ico->ie[ino];

   fseek(ico->fp, ie->ide.offs, SEEK_SET);
   nr = fread(&ie->bih, 1, sizeof(ie->bih), ico->fp);
   if (nr != sizeof(ie->bih))
      goto bail;

   SWAP_LE_32_INPLACE(ie->bih.header_size);
   SWAP_LE_32_INPLACE(ie->bih.width);
   SWAP_LE_32_INPLACE(ie->bih.height);

   SWAP_LE_32_INPLACE(ie->bih.planes);
   SWAP_LE_32_INPLACE(ie->bih.bpp);

   SWAP_LE_32_INPLACE(ie->bih.compression);
   SWAP_LE_32_INPLACE(ie->bih.size);
   SWAP_LE_32_INPLACE(ie->bih.res_hor);
   SWAP_LE_32_INPLACE(ie->bih.res_ver);
   SWAP_LE_32_INPLACE(ie->bih.colors);
   SWAP_LE_32_INPLACE(ie->bih.colors_important);

   if (ie->bih.header_size != 40)
     {
        D("Entry %2d: Skipping entry with unknown format\n", ino);
        goto bail;
     }

   D("Entry %2d: Icon: WxHxD = %dx%dx%d, colors = %d\n",
     ino, ie->w, ie->h, ie->bih.bpp, ie->bih.colors);

   if (ie->bih.width != ie->w || ie->bih.height != 2 * ie->h)
     {
        D("Entry %2d: Skipping entry with unexpected content (WxH = %dx%d/2)\n",
          ino, ie->bih.width, ie->bih.height);
        goto bail;
     }

   if (ie->bih.colors == 0 && ie->bih.bpp < 32)
      ie->bih.colors = 1U << ie->bih.bpp;

   switch (ie->bih.bpp)
     {
     case 1:
     case 4:
     case 8:
        D("Allocating a %d slot colormap\n", ie->bih.colors);
        size = ie->bih.colors * sizeof(DATA32);
        ie->cmap = malloc(size);
        nr = fread(ie->cmap, 1, size, ico->fp);
        if (nr != size)
           goto bail;
#ifdef WORDS_BIGENDIAN
        for (nr = 0; nr < ie->bih.colors; nr++)
           SWAP_LE_32_INPLACE(ie->cmap[nr]);
#endif
        break;
     default:
        break;
     }

   size = ((ie->bih.bpp * ie->w + 31) / 32 * 4) * ie->h;
   ie->pxls = malloc(size);
   nr = fread(ie->pxls, 1, size, ico->fp);
   if (nr != size)
      goto bail;
   D("Pixel data size: %u\n", size);

   size = ((ie->w + 31) / 32 * 4) * ie->h;
   ie->mask = malloc(size);
   nr = fread(ie->mask, 1, size, ico->fp);
   if (nr != size)
      goto bail;
   D("Mask  data size: %u\n", size);

   return;

 bail:
   ie->w = ie->h = 0;           /* Mark invalid */
}

static ico_t       *
ico_read(char *filename)
{
   ico_t              *ico;
   unsigned int        nr, i;

   ico = calloc(1, sizeof(ico_t));
   if (!ico)
      return NULL;

   ico->fp = fopen(filename, "rb");
   if (!ico->fp)
      goto bail;

   nr = fread(&ico->idir, 1, sizeof(ico->idir), ico->fp);
   if (nr != sizeof(ico->idir))
      goto bail;

   SWAP_LE_16_INPLACE(ico->idir.rsvd);
   SWAP_LE_16_INPLACE(ico->idir.type);
   SWAP_LE_16_INPLACE(ico->idir.icons);

   if (ico->idir.rsvd != 0 ||
       (ico->idir.type != 1 && ico->idir.type != 2) || ico->idir.icons <= 0)
      goto bail;

   ico->ie = calloc(ico->idir.icons, sizeof(ie_t));
   if (!ico->ie)
      goto bail;

   D("Loading '%s' Nicons = %d\n", filename, ico->idir.icons);

   for (i = 0; i < ico->idir.icons; i++)
     {
        ico_read_idir(ico, i);
        ico_read_icon(ico, i);
     }

   return ico;

 bail:
   ico_delete(ico);
   return NULL;
}

static int
ico_data_get_bit(DATA8 * data, int w, int x, int y)
{
   int                 w32, res;

   w32 = (w + 31) / 32 * 4;     /* Line length in bytes */
   res = data[y * w32 + x / 8]; /* Byte containing bit */
   res >>= 7 - (x & 7);
   res &= 0x01;

   return res;
}

static int
ico_data_get_nibble(DATA8 * data, int w, int x, int y)
{
   int                 w32, res;

   w32 = (4 * w + 31) / 32 * 4; /* Line length in bytes */
   res = data[y * w32 + x / 2]; /* Byte containing nibble */
   res >>= 4 * (1 - (x & 1));
   res &= 0x0f;

   return res;
}

static int
ico_load(ico_t * ico, ImlibImage * im, int load_data)
{
   int                 ic, x, y, w, h, d;
   DATA32             *cmap;
   DATA8              *pxls, *mask, *psrc;
   ie_t               *ie;
   DATA32             *pdst;
   DATA32              pixel;

   /* Find icon with largest size and depth */
   ic = y = d = 0;
   for (x = 0; x < ico->idir.icons; x++)
     {
        ie = &ico->ie[x];
        w = ie->w;
        h = ie->h;
        if (w * h < y)
           continue;
        if (w * h == y && ie->bih.bpp < d)
           continue;
        ic = x;
        y = w * h;
        d = ie->bih.bpp;
     }

   /* Enable overriding selected index for debug purposes */
   {
      const char         *s = getenv("IMLIB2_LOADER_ICO");

      ic = (s) ? atoi(s) : ic;
      if (ic >= ico->idir.icons)
         return 0;
   }

   ie = &ico->ie[ic];

   w = ie->w;
   h = ie->h;
   if (!IMAGE_DIMENSIONS_OK(w, h))
      return 0;

   im->w = w;
   im->h = h;

   SET_FLAG(im->flags, F_HAS_ALPHA);

   if (!load_data)
      return 1;

   if (!__imlib_AllocateData(im))
      return 0;

   D("Loading icon %d: WxHxD=%dx%dx%d\n", ic, w, h, ie->bih.bpp);

   cmap = ie->cmap;
   pxls = ie->pxls;
   mask = ie->mask;

   switch (ie->bih.bpp)
     {
     case 1:
        for (y = 0; y < h; y++)
          {
             for (x = 0; x < w; x++)
               {
                  pdst = &(im->data[(h - 1 - y) * w + x]);

                  pixel = cmap[ico_data_get_bit(pxls, w, x, y)];
                  if (ico_data_get_bit(mask, w, x, y) == 0)
                     pixel |= 0xff000000;

                  *pdst = pixel;
               }
          }
        break;

     case 4:
        for (y = 0; y < h; y++)
          {
             for (x = 0; x < w; x++)
               {
                  pdst = &(im->data[(h - 1 - y) * w + x]);

                  pixel = cmap[ico_data_get_nibble(pxls, w, x, y)];
                  if (ico_data_get_bit(mask, w, x, y) == 0)
                     pixel |= 0xff000000;

                  *pdst = pixel;
               }
          }
        break;

     case 8:
        for (y = 0; y < h; y++)
          {
             for (x = 0; x < w; x++)
               {
                  pdst = &(im->data[(h - 1 - y) * w + x]);

                  pixel = cmap[pxls[y * w + x]];
                  if (ico_data_get_bit(mask, w, x, y) == 0)
                     pixel |= 0xff000000;

                  *pdst = pixel;
               }
          }
        break;

     default:
        for (y = 0; y < h; y++)
          {
             for (x = 0; x < w; x++)
               {
                  pdst = &(im->data[(h - 1 - y) * w + x]);
                  psrc = &pxls[(y * w + x) * ie->bih.bpp / 8];

                  pixel = PIXEL_ARGB(0, psrc[2], psrc[1], psrc[0]);
                  if (ie->bih.bpp == 32)
                     pixel |= psrc[3] << 24;
                  else if (ico_data_get_bit(mask, w, x, y) == 0)
                     pixel |= 0xff000000;

                  *pdst = pixel;
               }
          }
        break;
     }

   return 1;
}

char
load(ImlibImage * im, ImlibProgressFunction progress, char progress_granularity,
     char immediate_load)
{
   ico_t              *ico;
   int                 ok, load_data;

   ico = ico_read(im->real_file);
   if (!ico)
      return 0;

   load_data = im->loader || immediate_load || progress;
   ok = ico_load(ico, im, load_data);
   if (ok)
     {
        if (progress)
           progress(im, 100, 0, 0, im->w, im->h);
     }

   ico_delete(ico);

   return ok;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "ico" };
   int                 i;

   l->num_formats = sizeof(list_formats) / sizeof(char *);
   l->formats = malloc(sizeof(char *) * l->num_formats);

   for (i = 0; i < l->num_formats; i++)
      l->formats[i] = strdup(list_formats[i]);
}
