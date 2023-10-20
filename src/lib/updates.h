#ifndef __UPDATES
#define __UPDATES 1

#include "common.h"

typedef struct _ImlibUpdate {
   int                 x, y, w, h;
   struct _ImlibUpdate *next;
} ImlibUpdate;

ImlibUpdate        *__imlib_AddUpdate(ImlibUpdate * u,
                                      int x, int y, int w, int h);
ImlibUpdate        *__imlib_MergeUpdate(ImlibUpdate * u,
                                        int w, int h, int hgapmax);
void                __imlib_FreeUpdates(ImlibUpdate * u);
ImlibUpdate        *__imlib_DupUpdates(ImlibUpdate * u);

#endif
