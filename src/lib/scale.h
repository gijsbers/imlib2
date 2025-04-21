#ifndef __SCALE
#define __SCALE 1

#include "types.h"

typedef struct _imlib_scale_info ImlibScaleInfo;

ImlibScaleInfo *__imlib_CalcScaleInfo(const ImlibImage * im,
                                      int sw, int sh, int dw, int dh, bool aa);
ImlibScaleInfo *__imlib_FreeScaleInfo(ImlibScaleInfo * isi);

void            __imlib_Scale(const ImlibScaleInfo * isi, bool aa, bool alpha,
                              const uint32_t * srce, uint32_t * dest,
                              int dxx, int dyy, int dx, int dy,
                              int dw, int dh, int dow, int sow);

#ifdef DO_MMX_ASM
void            __imlib_Scale_mmx_AARGBA(const ImlibScaleInfo * isi,
                                         uint32_t * dest,
                                         int dxx, int dyy, int dx, int dy,
                                         int dw, int dh, int dow, int sow);
#endif

#endif
