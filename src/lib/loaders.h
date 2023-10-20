#ifndef __LOADERS
#define __LOADERS 1

#include "types.h"

#define IMLIB2_LOADER_VERSION 3

#define LDR_FLAG_KEEP   0x01    /* Don't unload loader */

typedef struct {
   unsigned char       ldr_version;     /* Module ABI version */
   unsigned char       ldr_flags;       /* LDR_FLAG_... */
   unsigned short      num_formats;     /* Length og known extension list */
   const char         *const *formats;  /* Known extension list */
   void                (*inex)(int init);       /* Module init/exit */
   int                 (*load)(ImlibImage * im, int load_data);
   int                 (*save)(ImlibImage * im);
} ImlibLoaderModule;

struct _ImlibLoader {
   char               *file;
   void               *handle;
   ImlibLoaderModule  *module;
   ImlibLoader        *next;

   const char         *name;
};

void                __imlib_RemoveAllLoaders(void);
ImlibLoader       **__imlib_GetLoaderList(void);

#endif /* __LOADERS */
