#ifndef __GRAD
#define __GRAD 1

#include "common.h"

typedef struct _ImlibRangeColor {
   DATA8               red, green, blue, alpha;
   int                 distance;
   struct _ImlibRangeColor *next;
} ImlibRangeColor;

typedef struct {
   ImlibRangeColor    *color;
} ImlibRange;

ImlibRange         *__imlib_CreateRange(void);
void                __imlib_FreeRange(ImlibRange * rg);
void                __imlib_AddRangeColor(ImlibRange * rg, DATA8 r, DATA8 g,
                                          DATA8 b, DATA8 a, int dist);
void                __imlib_DrawGradient(ImlibImage * im,
                                         int x, int y, int w, int h,
                                         ImlibRange * rg, double angle,
                                         ImlibOp op,
                                         int clx, int cly, int clw, int clh);
void                __imlib_DrawHsvaGradient(ImlibImage * im,
                                             int x, int y, int w, int h,
                                             ImlibRange * rg, double angle,
                                             ImlibOp op,
                                             int clx, int cly,
                                             int clw, int clh);

#endif
