#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"

static const char  *
__imlib_PathToModules(void)
{
#if 0
   static const char  *path = NULL;

   if (path)
      return path;

   path = getenv("IMLIB2_MODULE_PATH");
   if (path && __imlib_FileIsDir(path))
      return path;
#endif

   return PACKAGE_LIB_DIR "/imlib2";
}

const char         *
__imlib_PathToFilters(void)
{
   static char        *path = NULL;
   char                buf[1024];

   if (path)
      return path;

   path = getenv("IMLIB2_FILTER_PATH");
   if (path && __imlib_FileIsDir(path))
      return path;

   snprintf(buf, sizeof(buf), "%s/%s", __imlib_PathToModules(), "filters");
   path = strdup(buf);

   return path;
}

const char         *
__imlib_PathToLoaders(void)
{
   static char        *path = NULL;
   char                buf[1024];

   if (path)
      return path;

   path = getenv("IMLIB2_LOADER_PATH");
   if (path && __imlib_FileIsDir(path))
      return path;

   snprintf(buf, sizeof(buf), "%s/%s", __imlib_PathToModules(), "loaders");
   path = strdup(buf);

   return path;
}

static char       **
__imlib_TrimLoaderList(char **list, int *num)
{
   int                 i, n, size = 0;

   if (!list)
      return NULL;

   n = *num;

   for (i = 0; i < n; i++)
     {
        char               *ext;

        if (!list[i])
           continue;
        ext = strrchr(list[i], '.');
        if ((ext) && (
#ifdef __CYGWIN__
                        (!strcasecmp(ext, ".dll")) ||
#endif
                        (!strcasecmp(ext, ".so"))))
          {
             /* Don't add the same loader multiple times... */
             if (!__imlib_ItemInList(list, size, list[i]))
               {
                  list[size++] = list[i];
                  continue;
               }
          }
        free(list[i]);
     }
   if (!size)
     {
        free(list);
        list = NULL;
     }
   else
     {
        list = realloc(list, size * sizeof(char *));
     }
   *num = size;
   return list;
}

char              **
__imlib_ListModules(const char *path, int *num_ret)
{
   char              **list = NULL, **l;
   char                file[1024];
   int                 num, i;

   *num_ret = 0;

   l = __imlib_FileDir(path, &num);
   if (num <= 0)
      return NULL;

   list = malloc(num * sizeof(char *));
   if (list)
     {
        for (i = 0; i < num; i++)
          {
             snprintf(file, sizeof(file), "%s/%s", path, l[i]);
             list[i] = strdup(file);
          }
        *num_ret = num;
     }
   __imlib_FileFreeDirList(l, num);

   /* List currently contains *everything in there* we need to weed out
    * the .so, .la, .a versions of the same loader or whatever else.
    * dlopen can take an extension-less name and do the Right Thing
    * with it, so that's what we'll give it. */
   list = __imlib_TrimLoaderList(list, num_ret);

   return list;
}
