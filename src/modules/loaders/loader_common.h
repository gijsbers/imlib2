#ifndef __LOADER_COMMON_H
#define __LOADER_COMMON_H 1

#include "config.h"
#include "common.h"
#include "debug.h"
#include "image.h"

__EXPORT__ char     load(ImlibImage * im, ImlibProgressFunction progress,
                         char progress_granularity, char load_data);
__EXPORT__ int      load2(ImlibImage * im, int load_data);
__EXPORT__ char     save(ImlibImage * im, ImlibProgressFunction progress,
                         char progress_granularity);
__EXPORT__ void     formats(ImlibLoader * l);

typedef int         (imlib_decompress_load_f) (const void *fdata,
                                               unsigned int fsize, int dest);

int                 decompress_load(ImlibImage * im, int load_data,
                                    const char *const *pext, int next,
                                    imlib_decompress_load_f * fdec);

#define QUIT_WITH_RC(_err) { rc = _err; goto quit; }
#define QUITx_WITH_RC(_err, _lbl) { rc = _err; goto _lbl; }

#endif /* __LOADER_COMMON_H */
