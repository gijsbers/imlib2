#ifndef X11_RGBA_H
#define X11_RGBA_H 1

#include "types.h"
#include "x11_context.h"

#define DM_BS1 (8 + 3)
#define DM_BS2 (8)
#define DM_X (8)
#define DM_Y (8)

void            __imlib_RGBASetupContext(Context * ct);
void            __imlib_RGBA_init(void *rd, void *gd, void *bd, int depth,
                                  int palette_type);

typedef void    (*ImlibRGBAFunction)(const uint32_t *, int, uint8_t *,
                                     int, int, int, int, int);
typedef void    (*ImlibMaskFunction)(const uint32_t *, int, uint8_t *,
                                     int, int, int, int, int, int);
ImlibRGBAFunction __imlib_GetRGBAFunction(int depth, unsigned long rm,
                                          unsigned long gm, unsigned long bm,
                                          char hiq, int palette_type);
ImlibMaskFunction __imlib_GetMaskFunction(char hiq);

#ifdef DO_MMX_ASM
void            __imlib_mmx_rgb555_fast(const uint32_t *, int, uint8_t *,
                                        int, int, int, int, int);
void            __imlib_mmx_bgr555_fast(const uint32_t *, int, uint8_t *,
                                        int, int, int, int, int);
void            __imlib_mmx_rgb565_fast(const uint32_t *, int, uint8_t *,
                                        int, int, int, int, int);
void            __imlib_mmx_bgr565_fast(const uint32_t *, int, uint8_t *,
                                        int, int, int, int, int);
#endif

#endif                          /* X11_RGBA_H */
