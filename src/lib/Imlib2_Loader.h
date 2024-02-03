#ifndef IMLIB2_LOADER_H
#define IMLIB2_LOADER_H 1
/**
 * The Imlib2 loader API
 *
 * NB! TO BE USED BY LOADERS ONLY
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* types.h */

typedef struct _ImlibLoader ImlibLoader;

typedef struct _ImlibImage ImlibImage;

/* common.h */

#undef __EXPORT__
#if defined(__GNUC__) && __GNUC__ >= 4
#define __EXPORT__ __attribute__ ((visibility("default")))
#else
#define __EXPORT__
#endif

#if __GNUC__
#define __PRINTF_N__(no)  __attribute__((__format__(__printf__, (no), (no)+1)))
#else
#define __PRINTF_N__(no)
#endif
#define __PRINTF__   __PRINTF_N__(1)
#define __PRINTF_2__ __PRINTF_N__(2)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

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

#define PIXEL_ARGB(a, r, g, b)  ((uint32_t)(a) << 24) | ((r) << 16) | ((g) << 8) | (b)

#define PIXEL_A(argb)  (((argb) >> 24) & 0xff)
#define PIXEL_R(argb)  (((argb) >> 16) & 0xff)
#define PIXEL_G(argb)  (((argb) >>  8) & 0xff)
#define PIXEL_B(argb)  (((argb)      ) & 0xff)

#ifndef WORDS_BIGENDIAN
#define A_VAL(p) ((uint8_t *)(p))[3]
#define R_VAL(p) ((uint8_t *)(p))[2]
#define G_VAL(p) ((uint8_t *)(p))[1]
#define B_VAL(p) ((uint8_t *)(p))[0]
#else
#define A_VAL(p) ((uint8_t *)(p))[0]
#define R_VAL(p) ((uint8_t *)(p))[1]
#define G_VAL(p) ((uint8_t *)(p))[2]
#define B_VAL(p) ((uint8_t *)(p))[3]
#endif

/* debug.h */

#if IMLIB2_DEBUG

#define DC(PFX, M, fmt...) if (__imlib_debug & M) __imlib_printf(PFX, fmt)

#define DBG_FILE    0x0001
#define DBG_LOAD    0x0002
#define DBG_LDR     0x0004
#define DBG_LDR2    0x0008

#define D(fmt...)  DC(DBG_PFX, DBG_LDR, fmt)
#define Dx(fmt...) DC(NULL, DBG_LDR, fmt)
#define DL(fmt...) DC(DBG_PFX, DBG_LDR2, fmt)

extern unsigned int __imlib_debug;

__PRINTF_2__ void __imlib_printf(const char *pfx, const char *fmt, ...);

unsigned int    __imlib_time_us(void);

#else

#define DC(fmt...)
#define D(fmt...)
#define Dx(fmt...)
#define DL(fmt...)

#endif                          /* IMLIB2_DEBUG */

#define E(fmt...) __imlib_perror(DBG_PFX, fmt)
__PRINTF_2__ void __imlib_perror(const char *pfx, const char *fmt, ...);

/* image.h */

/* 32767 is the maximum pixmap dimension and ensures that
 * (w * h * sizeof(uint32_t)) won't exceed ULONG_MAX */
#define X_MAX_DIM 32767
/* NB! The image dimensions are sometimes used in (dim << 16) like expressions
 * so great care must be taken if ever it is attempted to change this
 * condition */

#define IMAGE_DIMENSIONS_OK(w, h) \
   ( ((w) > 0) && ((h) > 0) && ((w) <= X_MAX_DIM) && ((h) <= X_MAX_DIM) )

#define LOAD_BREAK       2      /* Break signaled by progress callback */
#define LOAD_SUCCESS     1      /* Image loaded successfully           */
#define LOAD_FAIL        0      /* Image was not recognized by loader  */
#define LOAD_OOM        -1      /* Could not allocate memory           */
#define LOAD_BADFILE    -2      /* File could not be accessed          */
#define LOAD_BADIMAGE   -3      /* Image is corrupt                    */
#define LOAD_BADFRAME   -4      /* Requested frame not found           */

typedef struct _ImlibImageFileInfo {
    struct _ImlibImageFileInfo *next;
    char           *name;
    FILE           *fp;
    const void     *fdata;
    off_t           fsize;
} ImlibImageFileInfo;

typedef struct _ImlibLoaderCtx ImlibLoaderCtx;

typedef struct _ImlibImageTag {
    char           *key;
    int             val;
    void           *data;
    void            (*destructor)(ImlibImage * im, void *data);
    struct _ImlibImageTag *next;
} ImlibImageTag;

typedef struct {
    int             canvas_w;   /* Canvas size      */
    int             canvas_h;
    int             frame_count;        /* Number of frames */
    int             frame_x;    /* Frame origin     */
    int             frame_y;
    int             frame_flags;        /* Frame flags      */
    int             frame_delay;        /* Frame delay (ms) */
    int             loop_count; /* Animation loops  */
} ImlibImageFrame;

struct _ImlibImage {
    ImlibImageFileInfo *fi;
    ImlibLoaderCtx *lc;

    int             w, h;
    uint32_t       *data;
    char            has_alpha;
    char            rsvd[3];

    int             frame;
};

/* Must match the ones in Imlib2.h.in */
#define FF_IMAGE_ANIMATED       (1 << 0)        /* Frames are an animated sequence    */
#define FF_FRAME_BLEND          (1 << 1)        /* Blend current onto previous frame  */
#define FF_FRAME_DISPOSE_CLEAR  (1 << 2)        /* Clear before rendering next frame  */
#define FF_FRAME_DISPOSE_PREV   (1 << 3)        /* Revert before rendering next frame */

ImlibLoader    *__imlib_FindBestLoader(const char *file, const char *format,
                                       int for_save);
int             __imlib_LoadEmbedded(ImlibLoader * l, ImlibImage * im,
                                     int load_data, const char *file);
int             __imlib_LoadEmbeddedMem(ImlibLoader * l, ImlibImage * im,
                                        int load_data, const void *fdata,
                                        unsigned int fsize);

uint32_t       *__imlib_AllocateData(ImlibImage * im);
void            __imlib_FreeData(ImlibImage * im);

typedef void    (*ImlibDataDestructorFunction)(ImlibImage * im, void *data);

void            __imlib_AttachTag(ImlibImage * im, const char *key,
                                  int val, void *data,
                                  ImlibDataDestructorFunction destructor);
ImlibImageTag  *__imlib_GetTag(const ImlibImage * im, const char *key);
ImlibImageTag  *__imlib_RemoveTag(ImlibImage * im, const char *key);
void            __imlib_FreeTag(ImlibImage * im, ImlibImageTag * t);

const char     *__imlib_GetKey(const ImlibImage * im);

ImlibImageFrame *__imlib_GetFrame(ImlibImage * im);

void            __imlib_LoadProgressSetPass(ImlibImage * im,
                                            int pass, int n_pass);
int             __imlib_LoadProgress(ImlibImage * im,
                                     int x, int y, int w, int h);
int             __imlib_LoadProgressRows(ImlibImage * im, int row, int nrows);

/* loader.h */

#define IMLIB2_LOADER_VERSION 3

#define LDR_FLAG_KEEP   0x01    /* Don't unload loader */

typedef struct {
    unsigned char   ldr_version;        /* Module ABI version */
    unsigned char   ldr_flags;  /* LDR_FLAG_... */
    unsigned short  num_formats;        /* Length og known extension list */
    const char     *const *formats;     /* Known extension list */
    void            (*inex)(int init);  /* Module init/exit */
    int             (*load)(ImlibImage * im, int load_data);
    int             (*save)(ImlibImage * im);
} ImlibLoaderModule;

#define IMLIB_LOADER_(_fmts, _ldr, _svr, _inex, _flags) \
    __EXPORT__ ImlibLoaderModule loader = { \
        .ldr_version = IMLIB2_LOADER_VERSION, \
        .ldr_flags = _flags, \
        .num_formats = ARRAY_SIZE(_fmts), \
        .formats = _fmts, \
        .inex = _inex, \
        .load = _ldr, \
        .save = _svr, \
    }

#define IMLIB_LOADER(_fmts, _ldr, _svr) \
    IMLIB_LOADER_(_fmts, _ldr, _svr, NULL, 0)

#define IMLIB_LOADER_KEEP(_fmts, _ldr, _svr) \
    IMLIB_LOADER_(_fmts, _ldr, _svr, NULL, LDR_FLAG_KEEP)

#define IMLIB_LOADER_INEX(_fmts, _ldr, _svr, _inex) \
    IMLIB_LOADER_(_fmts, _ldr, _svr, _inex, 0)

#define QUIT_WITH_RC(_err) { rc = _err; goto quit; }

#define PCAST(T, p) ((T)(const void *)(p))

#endif                          /* IMLIB2_LOADER_H */
