#include "common.h"

#include <X11/Xlib.h>

#include "x11_pixmap.h"

static ImlibImagePixmap *pixmaps = NULL;

/* create a pixmap cache data struct */
static ImlibImagePixmap *
__imlib_ProduceImagePixmap(void)
{
   ImlibImagePixmap   *ip;

   ip = calloc(1, sizeof(ImlibImagePixmap));

   return ip;
}

/* free a pixmap cache data struct and the pixmaps in it */
static void
__imlib_ConsumeImagePixmap(ImlibImagePixmap * ip)
{
#ifdef DEBUG_CACHE
   fprintf(stderr,
           "[Imlib2]  Deleting pixmap.  Reference count is %d, pixmap 0x%08lx, mask 0x%08lx\n",
           ip->references, ip->pixmap, ip->mask);
#endif
   if (ip->pixmap)
      XFreePixmap(ip->display, ip->pixmap);
   if (ip->mask)
      XFreePixmap(ip->display, ip->mask);
   free(ip->file);
   free(ip);
}

ImlibImagePixmap   *
__imlib_FindCachedImagePixmap(ImlibImage * im, int w, int h, Display * d,
                              Visual * v, int depth, int sx, int sy, int sw,
                              int sh, Colormap cm, char aa, char hiq,
                              char dmask, DATABIG modification_count)
{
   ImlibImagePixmap   *ip, *ip_prev;

   for (ip = pixmaps, ip_prev = NULL; ip; ip_prev = ip, ip = ip->next)
     {
        /* if all the pixmap attributes match */
        if ((ip->w == w) && (ip->h == h) && (ip->depth == depth) && (!ip->dirty)
            && (ip->visual == v) && (ip->display == d)
            && (ip->source_x == sx) && (ip->source_x == sy)
            && (ip->source_w == sw) && (ip->source_h == sh)
            && (ip->colormap == cm) && (ip->antialias == aa)
            && (ip->modification_count == modification_count)
            && (ip->dither_mask == dmask)
            && (ip->border.left == im->border.left)
            && (ip->border.right == im->border.right)
            && (ip->border.top == im->border.top)
            && (ip->border.bottom == im->border.bottom) &&
            (((im->file) && (ip->file) && !strcmp(im->file, ip->file)) ||
             ((!im->file) && (!ip->file) && (im == ip->image))))
          {
             /* move the pixmap to the head of the pixmap list */
             if (ip_prev)
               {
                  ip_prev->next = ip->next;
                  ip->next = pixmaps;
                  pixmaps = ip;
               }
             return ip;
          }
     }
   return NULL;
}

/* add a pixmap cahce struct to the pixmap cache (at the start of course */
ImlibImagePixmap   *
__imlib_AddImagePixmapToCache(ImlibImage * im, Pixmap pmap, Pixmap mask,
                              int w, int h,
                              Display * d, Visual * v, int depth,
                              int sx, int sy, int sw, int sh,
                              Colormap cm, char aa, char hiq, char dmask,
                              DATABIG modification_count)
{
   ImlibImagePixmap   *ip;

   ip = __imlib_ProduceImagePixmap();
   ip->visual = v;
   ip->depth = depth;
   ip->image = im;
   if (im->file)
      ip->file = strdup(im->file);
   ip->border.left = im->border.left;
   ip->border.right = im->border.right;
   ip->border.top = im->border.top;
   ip->border.bottom = im->border.bottom;
   ip->colormap = cm;
   ip->display = d;
   ip->w = w;
   ip->h = h;
   ip->source_x = sx;
   ip->source_y = sy;
   ip->source_w = sw;
   ip->source_h = sh;
   ip->antialias = aa;
   ip->modification_count = modification_count;
   ip->dither_mask = dmask;
   ip->hi_quality = hiq;
   ip->references = 1;
   ip->pixmap = pmap;
   ip->mask = mask;

   ip->next = pixmaps;
   pixmaps = ip;

   return ip;
}

void
__imlib_PixmapUnrefImage(ImlibImage * im)
{
   ImlibImagePixmap   *ip;

   for (ip = pixmaps; ip; ip = ip->next)
     {
        if (ip->image == im)
          {
             ip->image = NULL;
             ip->dirty = 1;
          }
     }
}

/* remove a pixmap cache struct from the pixmap cache */
static void
__imlib_RemoveImagePixmapFromCache(ImlibImagePixmap * ip_del)
{
   ImlibImagePixmap   *ip, *ip_prev;

   for (ip = pixmaps, ip_prev = NULL; ip; ip_prev = ip, ip = ip->next)
     {
        if (ip == ip_del)
          {
             if (ip_prev)
                ip_prev->next = ip->next;
             else
                pixmaps = ip->next;
             return;
          }
     }
}

/* clean out 0 reference count & dirty pixmaps from the cache */
void
__imlib_CleanupImagePixmapCache(void)
{
   ImlibImagePixmap   *ip, *ip_next, *ip_del;
   int                 current_cache;

   current_cache = __imlib_CurrentCacheSize();

   for (ip = pixmaps; ip; ip = ip_next)
     {
        ip_next = ip->next;
        if ((ip->references <= 0) && (ip->dirty))
          {
             __imlib_RemoveImagePixmapFromCache(ip);
             __imlib_ConsumeImagePixmap(ip);
          }
     }

   while (current_cache > __imlib_GetCacheSize())
     {
        for (ip = pixmaps, ip_del = NULL; ip; ip = ip->next)
          {
             if (ip->references <= 0)
                ip_del = ip;
          }
        if (!ip_del)
           break;

        __imlib_RemoveImagePixmapFromCache(ip_del);
        __imlib_ConsumeImagePixmap(ip_del);

        current_cache = __imlib_CurrentCacheSize();
     }
}

/* find an imagepixmap cache entry by the display and pixmap id */
static ImlibImagePixmap *
__imlib_FindImlibImagePixmapByID(Display * d, Pixmap p)
{
   ImlibImagePixmap   *ip;

   for (ip = pixmaps; ip; ip = ip->next)
     {
        /* if all the pixmap ID & Display match */
        if ((ip->pixmap == p) && (ip->display == d))
          {
#ifdef DEBUG_CACHE
             fprintf(stderr,
                     "[Imlib2]  Match found.  Reference count is %d, pixmap 0x%08lx, mask 0x%08lx\n",
                     ip->references, ip->pixmap, ip->mask);
#endif
             return ip;
          }
     }
   return NULL;
}

/* free a cached pixmap */
void
__imlib_FreePixmap(Display * d, Pixmap p)
{
   ImlibImagePixmap   *ip;

   /* find the pixmap in the cache by display and id */
   ip = __imlib_FindImlibImagePixmapByID(d, p);
   if (ip)
     {
        /* if tis positive reference count */
        if (ip->references > 0)
          {
             /* dereference it by one */
             ip->references--;
#ifdef DEBUG_CACHE
             fprintf(stderr,
                     "[Imlib2]  Reference count is now %d for pixmap 0x%08lx\n",
                     ip->references, ip->pixmap);
#endif
             /* if it becaume 0 reference count - clean the cache up */
             if (ip->references == 0)
                __imlib_CleanupImagePixmapCache();
          }
     }
   else
     {
#ifdef DEBUG_CACHE
        fprintf(stderr, "[Imlib2]  Pixmap 0x%08lx not found.  Freeing.\n", p);
#endif
        XFreePixmap(d, p);
     }
}

/* mark all pixmaps generated from this image as dirty so the cache code */
/* wont pick up on them again since they are now invalid since the original */
/* data they were generated from has changed */
void
__imlib_DirtyPixmapsForImage(ImlibImage * im)
{
   ImlibImagePixmap   *ip;

   for (ip = pixmaps; ip; ip = ip->next)
     {
        /* if image matches */
        if (ip->image == im)
           ip->dirty = 1;
     }
   __imlib_CleanupImagePixmapCache();
}

int
__imlib_PixmapCacheSize(void)
{
   int                 current_cache = 0;
   ImlibImagePixmap   *ip, *ip_next;

   for (ip = pixmaps; ip; ip = ip_next)
     {
        ip_next = ip->next;

        /* if the pixmap has 0 references */
        if (ip->references == 0)
          {
             /* if the image is invalid */
             if ((ip->dirty) || ((ip->image) && (!(IMAGE_IS_VALID(ip->image)))))
               {
                  __imlib_RemoveImagePixmapFromCache(ip);
                  __imlib_ConsumeImagePixmap(ip);
               }
             else
               {
                  /* add the pixmap data size to the cache size */
                  if (ip->pixmap)
                    {
                       if (ip->depth < 8)
                          current_cache += ip->w * ip->h * (ip->depth / 8);
                       else if (ip->depth == 8)
                          current_cache += ip->w * ip->h;
                       else if (ip->depth <= 16)
                          current_cache += ip->w * ip->h * 2;
                       else if (ip->depth <= 32)
                          current_cache += ip->w * ip->h * 4;
                    }
                  /* if theres a mask add it too */
                  if (ip->mask)
                     current_cache += ip->w * ip->h / 8;
               }
          }
     }

   return current_cache;
}
