#include "config.h"
#include "Imlib2_Loader.h"
#include "ldrs_util.h"

#include <avif/avif.h>

#define DBG_PFX "LDR-avif"
#define MAX_THREADS 4

static const char *const _formats[] = { "avif", "avifs" };

static int
_load(ImlibImage *im, int load_data)
{
    avifDecoder    *dec;
    avifRGBImage    rgb;
    avifImageTiming timing;
    int             rc;
    int             frame, fcount;
    ImlibImageFrame *pf;

    dec = avifDecoderCreate();
    if (!dec)
        return LOAD_OOM;
    dec->maxThreads = MAX_THREADS;

    if (avifDecoderSetIOMemory(dec, im->fi->fdata, im->fi->fsize) !=
        AVIF_RESULT_OK)
        QUIT_WITH_RC(LOAD_OOM);

    switch (avifDecoderParse(dec))
    {
    case AVIF_RESULT_OK:
        break;
    case AVIF_RESULT_OUT_OF_MEMORY:
        QUIT_WITH_RC(LOAD_OOM);
        break;
    default:
        QUIT_WITH_RC(LOAD_FAIL);
        break;
    }

    im->w = dec->image->width;
    im->h = dec->image->height;
    im->has_alpha = dec->alphaPresent;
    if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
        QUIT_WITH_RC(LOAD_BADIMAGE);

    pf = NULL;
    frame = im->frame;
    if (frame > 0)
    {
        fcount = dec->imageCount;
        if (frame > 1 && frame > fcount)
            QUIT_WITH_RC(LOAD_BADFRAME);

        pf = __imlib_GetFrame(im);
        if (!pf)
            QUIT_WITH_RC(LOAD_OOM);
        pf->frame_count = fcount;
        pf->loop_count = 0;     /* loop forever */
        if (pf->frame_count > 1)
            pf->frame_flags |= FF_IMAGE_ANIMATED;
        pf->canvas_w = dec->image->width;
        pf->canvas_h = dec->image->height;

        if (avifDecoderNthImageTiming(dec, frame - 1, &timing) !=
            AVIF_RESULT_OK)
            QUIT_WITH_RC(LOAD_BADIMAGE);
        pf->frame_delay = (int)(timing.duration * 1000);
    }
    else
    {
        frame = 1;
    }

    if (!load_data)
        QUIT_WITH_RC(LOAD_SUCCESS);

    if (avifDecoderNthImage(dec, frame - 1) != AVIF_RESULT_OK)
        QUIT_WITH_RC(LOAD_BADIMAGE);

    if (!__imlib_AllocateData(im))
        QUIT_WITH_RC(LOAD_OOM);

    avifRGBImageSetDefaults(&rgb, dec->image);
    rgb.depth = 8;
    rgb.pixels = (uint8_t *) im->data;
    rgb.rowBytes = im->w * 4;
    rgb.maxThreads = MAX_THREADS;
#ifdef WORDS_BIGENDIAN          /* NOTE(NRK): untested on big endian */
    rgb.format = AVIF_RGB_FORMAT_ARGB;
#else
    rgb.format = AVIF_RGB_FORMAT_BGRA;
#endif
    /* NOTE(NRK): shouldn't fail, i think. check anyways. */
    if (avifImageYUVToRGB(dec->image, &rgb) != AVIF_RESULT_OK)
        QUIT_WITH_RC(LOAD_BADIMAGE);

    if (im->lc)
        __imlib_LoadProgressRows(im, 0, im->h);
    rc = LOAD_SUCCESS;

  quit:
    avifDecoderDestroy(dec);
    return rc;
}

static int
_save(ImlibImage *im)
{
    int             rc;
    ImlibSaverParam imsp;
    avifResult      avrc;
    avifEncoder    *enc;
    avifImage      *avim = NULL;
    avifRGBImage    rgb;
    avifRWData      avout = AVIF_DATA_EMPTY;
    unsigned int    nw;

    enc = avifEncoderCreate();
    if (!enc)
        return LOAD_OOM;

    rc = LOAD_FAIL;

    avim = avifImageCreate(im->w, im->h, 8, AVIF_PIXEL_FORMAT_YUV444);
    if (!avim)
        QUIT_WITH_RC(LOAD_OOM);

    avifRGBImageSetDefaults(&rgb, avim);

    rgb.pixels = (uint8_t *) im->data;
    rgb.rowBytes = im->w * 4;
    rgb.format = AVIF_RGB_FORMAT_RGBA;
    rgb.ignoreAlpha = !im->has_alpha;

    avrc = avifImageRGBToYUV(avim, &rgb);
    if (avrc != AVIF_RESULT_OK)
        QUIT_WITH_RC(LOAD_FAIL);

    get_saver_params(im, &imsp);

    enc->speed = 10 - imsp.compression;
    if (imsp.quality == 100)
        enc->quality = enc->qualityAlpha = AVIF_QUALITY_LOSSLESS;
    else
        enc->quality = enc->qualityAlpha = imsp.quality;

    D("Quality/compr: %d/%d\n", imsp.quality, imsp.compression);
    D("Quality/speed: %d/%d\n", enc->quality, enc->speed);

    avrc = avifEncoderAddImage(enc, avim, 1, AVIF_ADD_IMAGE_FLAG_SINGLE);
    if (avrc != AVIF_RESULT_OK)
        QUIT_WITH_RC(LOAD_FAIL);

    avrc = avifEncoderFinish(enc, &avout);
    if (avrc != AVIF_RESULT_OK)
        QUIT_WITH_RC(LOAD_FAIL);

    nw = fwrite(avout.data, 1, avout.size, im->fi->fp);
    if (nw != avout.size)
        QUIT_WITH_RC(LOAD_FAIL);

    rc = LOAD_SUCCESS;

  quit:
    if (avim)
        avifImageDestroy(avim);
    if (enc)
        avifEncoderDestroy(enc);
    avifRWDataFree(&avout);

    return rc;
}

IMLIB_LOADER(_formats, _load, _save);
