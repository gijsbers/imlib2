#include "Imlib2_Loader.h"
#include "ldrs_util.h"

#define CLAMP(X, MIN, MAX)  ((X) < (MIN) ? (MIN) : ((X) > (MAX) ? (MAX) : (X)))

/*
 * Default compression to quality "conversion"
 * compression quality
 *      0       100     Lossless
 *      1        88
 *      2        77
 *      3        66
 *      4        55
 *      5        44
 *      6        33
 *      7        22
 *      8        11
 *      9         0
 */

void
get_saver_params(const ImlibImage *im, ImlibSaverParam *imsp)
{
    ImlibImageTag  *tag;

    /* Set defaults */
    imsp->compr_type = -1;
    imsp->compression = 6;
    imsp->quality = 75;
    imsp->interlacing = 0;

    /* Compression type */
    if ((tag = __imlib_GetTag(im, "compression_type")))
        imsp->compr_type = tag->val;

    /* Compression (effort) */
    if ((tag = __imlib_GetTag(im, "compression")))
        imsp->compression = CLAMP(tag->val, 0, 9);

    /* "Convert" to quality */
    imsp->quality = (9 - imsp->compression) * 100 / 9;

    /* Quality (100 is lossless) */
    if ((tag = __imlib_GetTag(im, "quality")))
        imsp->quality = CLAMP(tag->val, 1, 100);

    /* Interlacing */
    if ((tag = __imlib_GetTag(im, "interlacing")))
        imsp->interlacing = !!tag->val;
}
