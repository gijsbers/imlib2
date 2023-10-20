#include "common.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "debug.h"
#include "file.h"
#include "image.h"
#include "loaders.h"

#define DBG_PFX "LOAD"
#define DP(fmt...) DC(DBG_LOAD, fmt)

static ImlibLoader *loaders = NULL;
static char         loaders_loaded = 0;

typedef struct {
   const char         *dso;
   const char         *const *ext;
} KnownLoader;

static const char  *const ext_argb[] = { "argb", NULL };
static const char  *const ext_bmp[] = { "bmp", NULL };
#ifdef BUILD_BZ2_LOADER
static const char  *const ext_bz2[] = { "bz2", NULL };
#endif
static const char  *const ext_ff[] = { "ff", NULL };
#ifdef BUILD_GIF_LOADER
static const char  *const ext_gif[] = { "gif", NULL };
#endif
static const char  *const ext_ico[] = { "ico", NULL };
#ifdef BUILD_ID3_LOADER
static const char  *const ext_id3[] = { "mp3", NULL };
#endif
#ifdef BUILD_JPEG_LOADER
static const char  *const ext_jpeg[] = { "jpg", "jpeg", "jfif", "jfi", NULL };
#endif
static const char  *const ext_lbm[] = { "iff", "ilbm", "lbm", NULL };
#ifdef BUILD_PNG_LOADER
static const char  *const ext_png[] = { "png", NULL };
#endif
static const char  *const ext_pnm[] =
   { "pnm", "ppm", "pgm", "pbm", "pam", NULL };
static const char  *const ext_tga[] = { "tga", NULL };
#ifdef BUILD_TIFF_LOADER
static const char  *const ext_tiff[] = { "tiff", "tif", NULL };
#endif
#ifdef BUILD_WEBP_LOADER
static const char  *const ext_webp[] = { "webp", NULL };
#endif
static const char  *const ext_xbm[] = { "xbm", NULL };
static const char  *const ext_xpm[] = { "xpm", NULL };
#ifdef BUILD_ZLIB_LOADER
static const char  *const ext_zlib[] = { "gz", NULL };
#endif
static const KnownLoader loaders_known[] = {
   {"argb", ext_argb},
   {"bmp", ext_bmp},
#ifdef BUILD_BZ2_LOADER
   {"bz2", ext_bz2},
#endif
   {"ff", ext_ff},
#ifdef BUILD_GIF_LOADER
   {"gif", ext_gif},
#endif
   {"ico", ext_ico},
#ifdef BUILD_ID3_LOADER
   {"id3", ext_id3},
#endif
#ifdef BUILD_JPEG_LOADER
   {"jpeg", ext_jpeg},
#endif
   {"lbm", ext_lbm},
#ifdef BUILD_PNG_LOADER
   {"png", ext_png},
#endif
   {"pnm", ext_pnm},
   {"tga", ext_tga},
#ifdef BUILD_TIFF_LOADER
   {"tiff", ext_tiff},
#endif
#ifdef BUILD_WEBP_LOADER
   {"webp", ext_webp},
#endif
   {"xbm", ext_xbm},
   {"xpm", ext_xpm},
#ifdef BUILD_ZLIB_LOADER
   {"zlib", ext_zlib},
#endif
};

static int
__imlib_IsLoaderLoaded(const char *file)
{
   ImlibLoader        *l;

   for (l = loaders; l; l = l->next)
     {
        if (strcmp(file, l->file) == 0)
           return 1;
     }

   return 0;
}

/* try dlopen()ing the file if we succeed finish filling out the malloced */
/* loader struct and return it */
static ImlibLoader *
__imlib_ProduceLoader(const char *file)
{
   ImlibLoader        *l;
   void                (*l_formats)(ImlibLoader * l);

   DP("%s: %s\n", __func__, file);

   l = malloc(sizeof(ImlibLoader));
   l->num_formats = 0;
   l->formats = NULL;
   l->handle = dlopen(file, RTLD_NOW | RTLD_LOCAL);
   if (!l->handle)
     {
        free(l);
        return NULL;
     }
   l->load2 = dlsym(l->handle, "load2");
   if (!l->load2)
      l->load = dlsym(l->handle, "load");
   l->save = dlsym(l->handle, "save");
   l_formats = dlsym(l->handle, "formats");

   /* each loader must provide formats() and at least load() or save() */
   if (!l_formats || !(l->load2 || l->load || l->save))
     {
        dlclose(l->handle);
        free(l);
        return NULL;
     }
   l_formats(l);
   l->file = strdup(file);
   l->next = NULL;

   l->next = loaders;
   loaders = l;

   return l;
}

/* fre the struct for a loader and close its dlopen'd handle */
static void
__imlib_ConsumeLoader(ImlibLoader * l)
{
   free(l->file);
   if (l->handle)
      dlclose(l->handle);
   if (l->formats)
     {
        int                 i;

        for (i = 0; i < l->num_formats; i++)
           free(l->formats[i]);
        free(l->formats);
     }
   free(l);
}

/* remove all loaders int eh list we have cached so we can re-load them */
void
__imlib_RemoveAllLoaders(void)
{
   ImlibLoader        *l, *l_next;

   for (l = loaders; l; l = l_next)
     {
        l_next = l->next;
        __imlib_ConsumeLoader(l);
     }
   loaders = NULL;
   loaders_loaded = 0;
}

/* find all the loaders we can find and load them up to see what they can */
/* load / save */
static void
__imlib_LoadAllLoaders(void)
{
   int                 i, num;
   char              **list;

   DP("%s\n", __func__);

   /* list all the loaders imlib can find */
   list = __imlib_ListModules(__imlib_PathToLoaders(), &num);
   /* no loaders? well don't load anything */
   if (!list)
      return;

   /* go through the list of filenames for loader .so's and load them */
   /* (or try) and if it succeeds, append to our loader list */
   for (i = num - 1; i >= 0; i--)
     {
        if (!__imlib_IsLoaderLoaded(list[i]))
           __imlib_ProduceLoader(list[i]);
        free(list[i]);
     }
   free(list);

   loaders_loaded = 1;
}

ImlibLoader       **
__imlib_GetLoaderList(void)
{
   if (!loaders_loaded)
      __imlib_LoadAllLoaders();
   return &loaders;
}

static ImlibLoader *
__imlib_LookupKnownLoader(const char *format)
{
   const KnownLoader  *kl;
   ImlibLoader        *l;
   unsigned int        i;
   const char         *const *exts;
   char                nbuf[4096];

   kl = NULL;
   for (i = 0; i < ARRAY_SIZE(loaders_known); i++)
     {
        for (exts = loaders_known[i].ext; *exts; exts++)
          {
             if (strcasecmp(format, *exts) != 0)
                continue;
             kl = &loaders_known[i];
             goto done;
          }
     }

 done:
   l = NULL;
   if (kl)
     {
        snprintf(nbuf, sizeof(nbuf), "%s/%s.so", __imlib_PathToLoaders(),
                 kl->dso);
        l = __imlib_ProduceLoader(nbuf);
     }
   DP("%s: '%s' -> '%s': %p\n", __func__, format, kl ? kl->dso : "-", l);
   return l;
}

static ImlibLoader *
__imlib_LookupLoadedLoader(const char *format, int for_save)
{
   ImlibLoader        *l;

   DP("%s: fmt='%s'\n", __func__, format);

   /* go through the loaders - first loader that claims to handle that */
   /* image type (extension wise) wins as a first guess to use - NOTE */
   /* this is an OPTIMISATION - it is possible the file has no extension */
   /* or has an unrecognised one but still is loadable by a loader. */
   /* if thkis initial loader failes to load the load mechanism will */
   /* systematically go from most recently used to least recently used */
   /* loader until one succeeds - or none are left and all have failed */
   /* and only if all fail does the laod fail. the lao9der that does */
   /* succeed gets it way to the head of the list so it's going */
   /* to be used first next time in this search mechanims - this */
   /* assumes you tend to laod a few image types and ones generally */
   /* of the same format */
   for (l = loaders; l; l = l->next)
     {
        int                 i;

        /* go through all the formats that loader supports */
        for (i = 0; i < l->num_formats; i++)
          {
             /* does it match ? */
             if (strcasecmp(l->formats[i], format) == 0)
               {
                  /* does it provide the function we need? */
                  if ((for_save && l->save) ||
                      (!for_save && (l->load || l->load2)))
                     goto done;
               }
          }
     }

 done:
   DP("%s: fmt='%s': %s\n", __func__, format, l ? l->file : "-");
   return l;
}

__EXPORT__ ImlibLoader *
__imlib_FindBestLoaderForFormat(const char *format, int for_save)
{
   ImlibLoader        *l;

   DP("%s: fmt='%s'\n", __func__, format);

   if (!format || format[0] == '\0')
      return NULL;

   if (loaders)
     {
        /* At least one loader loaded */
        l = __imlib_LookupLoadedLoader(format, for_save);
        if (l || loaders_loaded)
           goto done;
     }

   l = __imlib_LookupKnownLoader(format);
   if (l)
      goto done;

   __imlib_LoadAllLoaders();

   l = __imlib_LookupLoadedLoader(format, for_save);

 done:
   DP("%s: fmt='%s': %s\n", __func__, format, l ? l->file : "-");
   return l;
}

__EXPORT__ ImlibLoader *
__imlib_FindBestLoaderForFile(const char *file, int for_save)
{
   ImlibLoader        *l;

   DP("%s: file='%s'\n", __func__, file);

   l = __imlib_FindBestLoaderForFormat(__imlib_FileExtension(file), for_save);

   return l;
}

ImlibLoader        *
__imlib_FindBestLoaderForFileFormat(const char *file, const char *format,
                                    int for_save)
{
   DP("%s: file='%s' ext='%s'\n", __func__, file, format);

   /* if the format is provided ("png" "jpg" etc.) use that */
   /* otherwise us the file extension */
   if (format)
      return __imlib_FindBestLoaderForFormat(format, for_save);
   else
      return __imlib_FindBestLoaderForFile(file, for_save);
}

__EXPORT__ void
__imlib_LoaderSetFormats(ImlibLoader * l,
                         const char *const *fmt, unsigned int num)
{
   unsigned int        i;

   l->num_formats = num;
   l->formats = malloc(sizeof(char *) * num);

   for (i = 0; i < num; i++)
      l->formats[i] = strdup(fmt[i]);
}
