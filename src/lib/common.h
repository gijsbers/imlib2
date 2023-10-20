#ifndef __COMMON
#define __COMMON 1

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define DATABIG unsigned long long
#define DATA64  unsigned long long
#define DATA32  unsigned int
#define DATA16  unsigned short
#define DATA8   unsigned char

#define SWAP32(x) \
    ((((x) & 0x000000ff ) << 24) | \
     (((x) & 0x0000ff00 ) <<  8) | \
     (((x) & 0x00ff0000 ) >>  8) | \
     (((x) & 0xff000000 ) >> 24))

#define SWAP16(x) \
    ((((x) & 0x00ff ) << 8) | \
     (((x) & 0xff00 ) >> 8))

#ifdef WORDS_BIGENDIAN
#define SWAP_LE_16(x) SWAP16(x)
#define SWAP_LE_32(x) SWAP32(x)
#define SWAP_LE_16_INPLACE(x) x = SWAP16(x)
#define SWAP_LE_32_INPLACE(x) x = SWAP32(x)
#else
#define SWAP_LE_16(x) (x)
#define SWAP_LE_32(x) (x)
#define SWAP_LE_16_INPLACE(x)
#define SWAP_LE_32_INPLACE(x)
#endif

#define PIXEL_ARGB(a, r, g, b)  ((a) << 24) | ((r) << 16) | ((g) << 8) | (b)

#define PIXEL_A(argb)  (((argb) >> 24) & 0xff)
#define PIXEL_R(argb)  (((argb) >> 16) & 0xff)
#define PIXEL_G(argb)  (((argb) >>  8) & 0xff)
#define PIXEL_B(argb)  (((argb)      ) & 0xff)

#ifdef DO_MMX_ASM
int                 __imlib_get_cpuid(void);

#define CPUID_MMX (1 << 23)
#define CPUID_XMM (1 << 25)
#endif

#define CLIP(x, y, w, h, xx, yy, ww, hh) \
    if (x < (xx)) { w += (x - (xx)); x = (xx); } \
    if (y < (yy)) { h += (y - (yy)); y = (yy); } \
    if ((x + w) > ((xx) + (ww))) { w = (ww) - (x - xx); } \
    if ((y + h) > ((yy) + (hh))) { h = (hh) - (y - yy); }

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define round(x) ((x) >=0 ? (int)((x) + 0.5) : (int)((x) - 0.5))

#endif
