#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Imlib2.h"
#include "debug.h"
#include "file.h"
#include "image.h"
#include "loaders.h"
#ifdef BUILD_X11
#include "x11_pixmap.h"
#endif

#define DBG_PFX "IMG"
#define DP(fmt...) DC(DBG_LOAD, fmt)

/* Imlib loader context */
struct _imlibldctx {
   ImlibProgressFunction progress;
   char                granularity;
   int                 pct, area, row;
   int                 pass, n_pass;
};

static ImlibImage  *images = NULL;

static int          cache_size = 4096 * 1024;

__EXPORT__ DATA32  *
__imlib_AllocateData(ImlibImage * im)
{
   int                 w = im->w;
   int                 h = im->h;

   if (w <= 0 || h <= 0)
      return NULL;

   if (im->data_memory_func)
      im->data = im->data_memory_func(NULL, w * h * sizeof(DATA32));
   else
      im->data = malloc(w * h * sizeof(DATA32));

   return im->data;
}

__EXPORT__ void
__imlib_FreeData(ImlibImage * im)
{
   if (im->data)
     {
        if (im->data_memory_func)
           im->data_memory_func(im->data, im->w * im->h * sizeof(DATA32));
        else
           free(im->data);

        im->data = NULL;
     }
   im->w = 0;
   im->h = 0;
}

__EXPORT__ void
__imlib_ReplaceData(ImlibImage * im, unsigned int *new_data)
{
   if (im->data)
     {
        if (im->data_memory_func)
           im->data_memory_func(im->data, im->w * im->h * sizeof(DATA32));
        else
           free(im->data);
     }
   im->data = new_data;
   im->data_memory_func = NULL;
}

/* create an image data struct and fill it in */
static ImlibImage  *
__imlib_ProduceImage(void)
{
   ImlibImage         *im;

   im = calloc(1, sizeof(ImlibImage));
   im->flags = F_FORMAT_IRRELEVANT | F_BORDER_IRRELEVANT | F_ALPHA_IRRELEVANT;

   return im;
}

/* free an image struct */
static void
__imlib_ConsumeImage(ImlibImage * im)
{
   __imlib_FreeAllTags(im);

   if (im->real_file && im->real_file != im->file)
      free(im->real_file);
   free(im->file);
   free(im->key);
   if ((IMAGE_FREE_DATA(im)) && (im->data))
      __imlib_FreeData(im);
   free(im->format);

   free(im);

#ifdef BUILD_X11
   __imlib_PixmapUnrefImage(im);
#endif
}

static ImlibImage  *
__imlib_FindCachedImage(const char *file)
{
   ImlibImage         *im, *im_prev;

   for (im = images, im_prev = NULL; im; im_prev = im, im = im->next)
     {
        /* if the filenames match and it's valid */
        if ((!strcmp(file, im->file)) && (IMAGE_IS_VALID(im)))
          {
             /* move the image to the head of the pixmap list */
             if (im_prev)
               {
                  im_prev->next = im->next;
                  im->next = images;
                  images = im;
               }
             return im;
          }
     }
   return NULL;
}

/* add an image to the cache of images (at the start) */
static void
__imlib_AddImageToCache(ImlibImage * im)
{
   im->next = images;
   images = im;
}

/* remove (unlink) an image from the cache of images */
static void
__imlib_RemoveImageFromCache(ImlibImage * im_del)
{
   ImlibImage         *im, *im_prev;

   for (im = images, im_prev = NULL; im; im_prev = im, im = im->next)
     {
        if (im == im_del)
          {
             if (im_prev)
                im_prev->next = im->next;
             else
                images = im->next;
             return;
          }
     }
}

/* work out how much we have floaitng aroudn in our speculative cache */
/* (images and pixmaps that have 0 reference counts) */
int
__imlib_CurrentCacheSize(void)
{
   ImlibImage         *im, *im_next;
   int                 current_cache = 0;

   for (im = images; im; im = im_next)
     {
        im_next = im->next;

        /* mayaswell clean out stuff thats invalid that we dont need anymore */
        if (im->references == 0)
          {
             if (!(IMAGE_IS_VALID(im)))
               {
                  __imlib_RemoveImageFromCache(im);
                  __imlib_ConsumeImage(im);
               }
             /* it's valid but has 0 ref's - append to cache size count */
             else
                current_cache += im->w * im->h * sizeof(DATA32);
          }
     }

#ifdef BUILD_X11
   current_cache += __imlib_PixmapCacheSize();
#endif

   return current_cache;
}

/* clean out images from the cache if the cache is overgrown */
static void
__imlib_CleanupImageCache(void)
{
   ImlibImage         *im, *im_next, *im_del;
   int                 current_cache;

   current_cache = __imlib_CurrentCacheSize();

   /* remove 0 ref count invalid (dirty) images */
   for (im = images; im; im = im_next)
     {
        im_next = im->next;
        if ((im->references <= 0) && (!(IMAGE_IS_VALID(im))))
          {
             __imlib_RemoveImageFromCache(im);
             __imlib_ConsumeImage(im);
          }
     }

   /* while the cache size of 0 ref coutn data is bigger than the set value */
   /* clean out the oldest members of the imaeg cache */
   while (current_cache > cache_size)
     {
        for (im = images, im_del = NULL; im; im = im->next)
          {
             if (im->references <= 0)
                im_del = im;
          }
        if (!im_del)
           break;

        __imlib_RemoveImageFromCache(im_del);
        __imlib_ConsumeImage(im_del);

        current_cache = __imlib_CurrentCacheSize();
     }
}

/* set the cache size */
void
__imlib_SetCacheSize(int size)
{
   cache_size = size;
   __imlib_CleanupImageCache();
#ifdef BUILD_X11
   __imlib_CleanupImagePixmapCache();
#endif
}

/* return the cache size */
int
__imlib_GetCacheSize(void)
{
   return cache_size;
}

static int
__imlib_ErrorFromErrno(int err, int save)
{
   switch (err)
     {
     default:
        return IMLIB_LOAD_ERROR_UNKNOWN;
        /* standrad fopen() type errors translated */
     case 0:
        return IMLIB_LOAD_ERROR_NONE;
     case EEXIST:
        return IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST;
     case EISDIR:
        return IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY;
     case EACCES:
     case EROFS:
        return (save) ? IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_WRITE :
           IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ;
     case ENAMETOOLONG:
        return IMLIB_LOAD_ERROR_PATH_TOO_LONG;
     case ENOENT:
        return IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT;
     case ENOTDIR:
        return IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY;
     case EFAULT:
        return IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE;
     case ELOOP:
        return IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS;
     case ENOMEM:
        return IMLIB_LOAD_ERROR_OUT_OF_MEMORY;
     case EMFILE:
        return IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS;
     case ENOSPC:
        return IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE;
     }
}

/* create a new image struct from data passed that is wize w x h then return */
/* a pointer to that image sturct */
ImlibImage         *
__imlib_CreateImage(int w, int h, DATA32 * data)
{
   ImlibImage         *im;

   im = __imlib_ProduceImage();
   im->w = w;
   im->h = h;
   im->data = data;
   im->references = 1;
   SET_FLAG(im->flags, F_UNCACHEABLE);
   return im;
}

static int
__imlib_LoadImageWrapper(const ImlibLoader * l, ImlibImage * im, int load_data)
{
   int                 rc;

   DP("%s: fmt='%s' file='%s'(%s), imm=%d\n", __func__,
      l->formats[0], im->file, im->real_file, load_data);

#if IMLIB2_DEBUG
   unsigned int        t0 = __imlib_time_us();
#endif

   if (l->load2)
     {
        FILE               *fp = NULL;

        if (!im->fp)
          {
             fp = im->fp = fopen(im->real_file, "rb");
             if (!im->fp)
                return 0;
          }
        rc = l->load2(im, load_data);

        if (fp)
           fclose(fp);
     }
   else if (l->load)
     {
        if (im->lc)
           rc = l->load(im, im->lc->progress, im->lc->granularity, 1);
        else
           rc = l->load(im, NULL, 0, load_data);
     }
   else
     {
        return LOAD_FAIL;
     }

   DP("%s: %-4s: %s: Elapsed time: %.3f ms\n", __func__,
      l->formats[0], im->file, 1e-3 * (__imlib_time_us() - t0));

   if (rc <= LOAD_FAIL)
     {
        /* Failed - clean up */
        DP("%s: Failed\n", __func__);

        if (im->w != 0 || im->h != 0)
          {
             DP("%s: Loader %s: Didn't clear w/h=%d/%d\n", __func__,
                l->formats[0], im->w, im->h);
             im->w = im->h = 0;
          }
        if (im->data)
          {
             DP("%s: Loader %s: Free im->data\n", __func__, l->formats[0]);
             __imlib_FreeData(im);
          }
        if (im->format)
          {
             DP("%s: Loader %s: Free im->format\n", __func__, l->formats[0]);
             free(im->format);
             im->format = NULL;
          }
     }
   else
     {
        if (!im->format && l->formats && l->formats[0])
           im->format = strdup(l->formats[0]);
     }

   return rc;
}

static void
__imlib_LoadCtxInit(ImlibImage * im, ImlibLdCtx * lc,
                    ImlibProgressFunction prog, int gran)
{
   im->lc = lc;
   lc->progress = prog;
   lc->granularity = gran;
   lc->pct = lc->row = 0;
   lc->area = 0;
   lc->pass = 0;
   lc->n_pass = 1;
}

__EXPORT__ int
__imlib_LoadEmbedded(ImlibLoader * l, ImlibImage * im, const char *file,
                     int load_data)
{
   int                 rc;
   struct stat         st;
   char               *file_save;
   FILE               *fp_save;
   off_t               fsize_save;

   if (!l || !im)
      return 0;

   /* remember the original filename */
   file_save = im->real_file;
   im->real_file = strdup(file);
   fp_save = im->fp;
   im->fp = NULL;
   fsize_save = im->fsize;
   __imlib_FileStat(file, &st);
   im->fsize = st.st_size;

   rc = __imlib_LoadImageWrapper(l, im, load_data);

   im->fp = fp_save;
   free(im->real_file);
   im->real_file = file_save;
   im->fsize = fsize_save;

   return rc;
}

ImlibImage         *
__imlib_LoadImage(const char *file, FILE * fp, ImlibProgressFunction progress,
                  char progress_granularity, char immediate_load,
                  char dont_cache, ImlibLoadError * er)
{
   ImlibImage         *im;
   ImlibLoader        *best_loader;
   int                 err, loader_ret;
   ImlibLdCtx          ilc;
   struct stat         st;
   char               *im_file, *im_key;

   if (!file || file[0] == '\0')
      return NULL;

   /* see if we already have the image cached */
   im = __imlib_FindCachedImage(file);

   /* if we found a cached image and we should always check that it is */
   /* accurate to the disk conents if they changed since we last loaded */
   /* and that it is still a valid image */
   if ((im) && (IMAGE_IS_VALID(im)))
     {
        if (IMAGE_ALWAYS_CHECK_DISK(im))
          {
             time_t              current_modified_time;

             current_modified_time = fp ?
                __imlib_FileModDateFd(fileno(fp)) :
                __imlib_FileModDate(im->real_file);
             /* if the file on disk is newer than the cached one */
             if (current_modified_time != im->moddate)
               {
                  /* invalidate image */
                  SET_FLAG(im->flags, F_INVALID);
               }
             else
               {
                  /* image is ok to re-use - program is just being stupid loading */
                  /* the same data twice */
                  im->references++;
                  return im;
               }
          }
        else
          {
             im->references++;
             return im;
          }
     }

   im_file = im_key = NULL;
   if (fp)
     {
        err = fstat(fileno(fp), &st);
     }
   else
     {
        err = __imlib_FileStat(file, &st);
        if (err)
          {
             im_file = __imlib_FileRealFile(file);
             im_key = __imlib_FileKey(file);
             err = __imlib_FileStat(im_file, &st);
          }
     }

   if (err)
      err = IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST;
   else if (__imlib_StatIsDir(&st))
      err = IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY;
   else if (st.st_size == 0)
      err = IMLIB_LOAD_ERROR_UNKNOWN;

   if (er)
      *er = err;

   if (err)
     {
        free(im_file);
        free(im_key);
        return NULL;
     }

   /* either image in cache is invalid or we dont even have it in cache */
   /* so produce a new one and load an image into that */
   im = __imlib_ProduceImage();
   im->file = strdup(file);
   im->fsize = st.st_size;
   im->real_file = im_file ? im_file : im->file;
   im->key = im_key;

   if (fp)
      im->fp = fp;
   else
      im->fp = fopen(im->real_file, "rb");

   if (!im->fp)
     {
        if (er)
           *er = __imlib_ErrorFromErrno(errno, 0);
        __imlib_ConsumeImage(im);
        return NULL;
     }

   im->moddate = __imlib_StatModDate(&st);

   im->data_memory_func = imlib_context_get_image_data_memory_function();

   if (progress)
     {
        __imlib_LoadCtxInit(im, &ilc, progress, progress_granularity);
        immediate_load = 1;
     }

   loader_ret = LOAD_FAIL;

   /* take a guess by extension on the best loader to use */
   best_loader = __imlib_FindBestLoaderForFile(im->real_file, 0);
   errno = 0;
   if (best_loader)
      loader_ret = __imlib_LoadImageWrapper(best_loader, im, immediate_load);

   if (loader_ret > LOAD_FAIL)
     {
        im->loader = best_loader;
     }
   else
     {
        ImlibLoader       **loaders = __imlib_GetLoaderList();
        ImlibLoader        *l, *previous_l;

        errno = 0;
        /* run through all loaders and try load until one succeeds */
        for (l = *loaders, previous_l = NULL; l; previous_l = l, l = l->next)
          {
             /* if its not the best loader that already failed - try load */
             if (l == best_loader)
                continue;
             fflush(im->fp);
             rewind(im->fp);
             loader_ret = __imlib_LoadImageWrapper(l, im, immediate_load);
             if (loader_ret > LOAD_FAIL)
                break;
          }

        /* if we have a loader then its the loader that succeeded */
        /* move the successful loader to the head of the list */
        /* as long as it's not already at the head of the list */
        if (l)
          {
             im->loader = l;
             if (previous_l)
               {
                  previous_l->next = l->next;
                  l->next = *loaders;
                  *loaders = l;
               }
          }
     }

   im->lc = NULL;

   if (!fp)
      fclose(im->fp);
   im->fp = NULL;

   /* all loaders have been tried and they all failed. free the skeleton */
   /* image struct we had and return NULL */
   if (loader_ret <= LOAD_FAIL)
     {
        /* if the caller wants an error return */
        if (er)
           *er = __imlib_ErrorFromErrno(errno, 0);
        __imlib_ConsumeImage(im);
        return NULL;
     }

   /* the load succeeded - make sure the image is referenced then add */
   /* it to our cache if dont_cache isn't set */
   im->references = 1;
   if (loader_ret == LOAD_BREAK)
      dont_cache = 1;
   if (!dont_cache)
      __imlib_AddImageToCache(im);
   else
      SET_FLAG(im->flags, F_UNCACHEABLE);

   return im;
}

int
__imlib_LoadImageData(ImlibImage * im)
{
   if (!im->data && im->loader)
      if (__imlib_LoadImageWrapper(im->loader, im, 1) <= LOAD_FAIL)
         return 1;              /* Load failed */
   return im->data == NULL;
}

__EXPORT__ void
__imlib_LoadProgressSetPass(ImlibImage * im, int pass, int n_pass)
{
   im->lc->pass = pass;
   im->lc->n_pass = n_pass;

   im->lc->row = 0;
}

__EXPORT__ int
__imlib_LoadProgress(ImlibImage * im, int x, int y, int w, int h)
{
   ImlibLdCtx         *lc = im->lc;
   int                 rc;

   lc->area += w * h;
   lc->pct = (100. * lc->area + .1) / (im->w * im->h);

   rc = !lc->progress(im, lc->pct, x, y, w, h);

   return rc;
}

__EXPORT__ int
__imlib_LoadProgressRows(ImlibImage * im, int row, int nrows)
{
   ImlibLdCtx         *lc = im->lc;
   int                 rc = 0;
   int                 pct, nrtot;

   if (nrows > 0)
     {
        /* Row index counting up */
        nrtot = row + nrows;
        row = lc->row;
        nrows = nrtot - lc->row;
     }
   else
     {
        /* Row index counting down */
        nrtot = im->h - row;
        row = row;
        nrows = nrtot - lc->row;
     }

   pct = (100 * nrtot * (lc->pass + 1)) / (im->h * lc->n_pass);
   if (pct == 100 || pct >= lc->pct + lc->granularity)
     {
        rc = !lc->progress(im, pct, 0, row, im->w, nrows);
        lc->row = nrtot;
        lc->pct += lc->granularity;
     }

   return rc;
}

/* free and image - if its uncachable and refcoutn is 0 - free it in reality */
void
__imlib_FreeImage(ImlibImage * im)
{
   /* if the refcount is positive */
   if (im->references >= 0)
     {
        /* reduce a reference from the count */
        im->references--;
        /* if its uncachchable ... */
        if (IMAGE_IS_UNCACHEABLE(im))
          {
             /* and we're down to no references for the image then free it */
             if (im->references == 0)
                __imlib_ConsumeImage(im);
          }
        /* otherwise clean up our cache if the image becoem 0 ref count */
        else if (im->references == 0)
           __imlib_CleanupImageCache();
     }
}

/* dirty and image by settings its invalid flag */
void
__imlib_DirtyImage(ImlibImage * im)
{
   SET_FLAG(im->flags, F_INVALID);
#ifdef BUILD_X11
   /* and dirty all pixmaps generated from it */
   __imlib_DirtyPixmapsForImage(im);
#endif
}

void
__imlib_SaveImage(ImlibImage * im, const char *file,
                  ImlibProgressFunction progress, char progress_granularity,
                  ImlibLoadError * er)
{
   ImlibLoader        *l;
   char                e, *pfile;
   ImlibLdCtx          ilc;

   if (!file)
     {
        if (er)
           *er = IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST;
        return;
     }

   /* find the laoder for the format - if its null use the extension */
   l = __imlib_FindBestLoaderForFileFormat(file, im->format, 1);
   /* no loader - abort */
   if (!l)
     {
        if (er)
           *er = IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT;
        return;
     }

   if (progress)
      __imlib_LoadCtxInit(im, &ilc, progress, progress_granularity);

   /* set the filename to the user supplied one */
   pfile = im->real_file;
   im->real_file = strdup(file);

   /* call the saver */
   e = l->save(im, progress, progress_granularity);

   /* set the filename back to the laoder image filename */
   free(im->real_file);
   im->real_file = pfile;

   im->lc = NULL;

   if (er)
      *er = __imlib_ErrorFromErrno(e > 0 ? 0 : errno, 1);
}
