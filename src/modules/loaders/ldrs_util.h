#ifndef LDRS_UTIL_H
#define LDRS_UTIL_H 1

#include "Imlib2_Loader.h"

typedef struct {
    int             compr_type; /* Saver specific */
    int             compression;        /* 0 -   9 */
    int             quality;    /* 1 - 100 */
    int             interlacing;        /* 0 or 1 */
} ImlibSaverParam;

void            get_saver_params(const ImlibImage * im, ImlibSaverParam * imsp);

#endif                          /* LDRS_UTIL_H */
