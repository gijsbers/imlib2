#ifndef __IMAGE
#define __IMAGE 1

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "types.h"

typedef struct _imlibldctx ImlibLdCtx;

typedef void        (*ImlibDataDestructorFunction)(ImlibImage * im, void *data);
typedef void       *(*ImlibImageDataMemoryFunction)(void *, size_t size);

enum _iflags {
   F_NONE = 0,
   F_HAS_ALPHA = (1 << 0),
   F_UNCACHEABLE = (1 << 1),
   F_ALWAYS_CHECK_DISK = (1 << 2),
   F_INVALID = (1 << 3),
   F_DONT_FREE_DATA = (1 << 4),
   F_FORMAT_IRRELEVANT = (1 << 5),
};

/* Must match the ones in Imlib2.h.in */
#define FF_IMAGE_ANIMATED       (1 << 0)        /* Frames are an animated sequence    */
#define FF_FRAME_BLEND          (1 << 1)        /* Blend current onto previous frame  */
#define FF_FRAME_DISPOSE_CLEAR  (1 << 2)        /* Clear before rendering next frame  */
#define FF_FRAME_DISPOSE_PREV   (1 << 3)        /* Revert before rendering next frame */

typedef struct {
   int                 left, right, top, bottom;
} ImlibBorder;

typedef struct _ImlibImageTag {
   char               *key;
   int                 val;
   void               *data;
   void                (*destructor)(ImlibImage * im, void *data);
   struct _ImlibImageTag *next;
} ImlibImageTag;

struct _ImlibImage {
   char               *file;
   int                 w, h;
   uint32_t           *data;
   ImlibImageFlags     flags;
   time_t              moddate;
   ImlibBorder         border;
   int                 references;
   ImlibLoader        *loader;
   char               *format;
   ImlibImage         *next;
   ImlibImageTag      *tags;
   char               *real_file;
   char               *key;
   ImlibImageDataMemoryFunction data_memory_func;
   ImlibLdCtx         *lc;
   FILE               *fp;
   off_t               fsize;
   int                 canvas_w;        /* Canvas size      */
   int                 canvas_h;
   int                 frame_count;     /* Number of frames */
   int                 frame_num;       /* Current frame    */
   int                 frame_x; /* Frame origin     */
   int                 frame_y;
   int                 frame_flags;     /* Frame flags      */
   int                 frame_delay;     /* Frame delay (ms) */
};

typedef struct {
   FILE               *fp;
   ImlibProgressFunction pfunc;
   int                 pgran;
   char                immed;
   char                nocache;
   int                 err;
   int                 frame;
} ImlibLoadArgs;

ImlibLoader        *__imlib_FindBestLoader(const char *file, const char *format,
                                           int for_save);

void                __imlib_LoaderSetFormats(ImlibLoader * l,
                                             const char *const *fmt,
                                             unsigned int num);

ImlibImage         *__imlib_CreateImage(int w, int h, uint32_t * data);
ImlibImage         *__imlib_LoadImage(const char *file, ImlibLoadArgs * ila);
int                 __imlib_LoadEmbedded(ImlibLoader * l, ImlibImage * im,
                                         const char *file, int load_data);
int                 __imlib_LoadImageData(ImlibImage * im);
void                __imlib_DirtyImage(ImlibImage * im);
void                __imlib_FreeImage(ImlibImage * im);
void                __imlib_SaveImage(ImlibImage * im, const char *file,
                                      ImlibLoadArgs * ila);

uint32_t           *__imlib_AllocateData(ImlibImage * im);
void                __imlib_FreeData(ImlibImage * im);
void                __imlib_ReplaceData(ImlibImage * im, uint32_t * new_data);

void                __imlib_LoadProgressSetPass(ImlibImage * im,
                                                int pass, int n_pass);
int                 __imlib_LoadProgress(ImlibImage * im,
                                         int x, int y, int w, int h);
int                 __imlib_LoadProgressRows(ImlibImage * im,
                                             int row, int nrows);

const char         *__imlib_GetKey(const ImlibImage * im);

void                __imlib_AttachTag(ImlibImage * im, const char *key,
                                      int val, void *data,
                                      ImlibDataDestructorFunction destructor);
ImlibImageTag      *__imlib_GetTag(const ImlibImage * im, const char *key);
ImlibImageTag      *__imlib_RemoveTag(ImlibImage * im, const char *key);
void                __imlib_FreeTag(ImlibImage * im, ImlibImageTag * t);
void                __imlib_FreeAllTags(ImlibImage * im);

void                __imlib_SetCacheSize(int size);
int                 __imlib_GetCacheSize(void);
int                 __imlib_CurrentCacheSize(void);

#define IM_FLAG_SET(im, f)      ((im)->flags |= (f))
#define IM_FLAG_CLR(im, f)      ((im)->flags &= ~(f))
#define IM_FLAG_UPDATE(im, f, set) \
   do { if (set) IM_FLAG_SET(im, f); else IM_FLAG_CLR(im, f); } while(0)
#define IM_FLAG_ISSET(im, f)    (((im)->flags & (f)) != 0)

#define LOAD_BREAK       2      /* Break signaled by progress callback */
#define LOAD_SUCCESS     1      /* Image loaded successfully           */
#define LOAD_FAIL        0      /* Image was not recognized by loader  */
#define LOAD_OOM        -1      /* Could not allocate memory           */
#define LOAD_BADFILE    -2      /* File could not be accessed          */
#define LOAD_BADIMAGE   -3      /* Image is corrupt                    */
#define LOAD_BADFRAME   -4      /* Requested frame not found           */

/* 32767 is the maximum pixmap dimension and ensures that
 * (w * h * sizeof(uint32_t)) won't exceed ULONG_MAX */
#define X_MAX_DIM 32767
/* NB! The image dimensions are sometimes used in (dim << 16) like expressions
 * so great care must be taken if ever it is attempted to change this
 * condition */

#define IMAGE_DIMENSIONS_OK(w, h) \
   ( ((w) > 0) && ((h) > 0) && ((w) <= X_MAX_DIM) && ((h) <= X_MAX_DIM) )

#endif
