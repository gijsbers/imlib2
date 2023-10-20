#ifndef __LOADERS
#define __LOADERS 1

#include "types.h"

struct _ImlibLoader {
   char               *file;
   int                 num_formats;
   char              **formats;
   void               *handle;
   char                (*load)(ImlibImage * im,
                               ImlibProgressFunction progress,
                               char progress_granularity, char load_data);
   char                (*save)(ImlibImage * im,
                               ImlibProgressFunction progress,
                               char progress_granularity);
   ImlibLoader        *next;
   int                 (*load2)(ImlibImage * im, int load_data);
};

void                __imlib_RemoveAllLoaders(void);
ImlibLoader       **__imlib_GetLoaderList(void);

#endif /* __LOADERS */
