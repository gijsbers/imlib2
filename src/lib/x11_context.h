#ifndef X11_CONTEXT_H
#define X11_CONTEXT_H 1

#include "types.h"
#include "x11_types.h"

typedef struct _Context {
   int                 last_use;
   ImlibContextX11     x11;
   struct _Context    *next;

   uint8_t            *palette;
   unsigned char       palette_type;
   void               *r_dither;
   void               *g_dither;
   void               *b_dither;
} Context;

void                __imlib_SetMaxContexts(int num);
int                 __imlib_GetMaxContexts(void);
void                __imlib_FlushContexts(void);
Context            *__imlib_FindContext(const ImlibContextX11 * x11);
Context            *__imlib_NewContext(const ImlibContextX11 * x11);
Context            *__imlib_GetContext(const ImlibContextX11 * x11);

#endif /* X11_CONTEXT_H */
