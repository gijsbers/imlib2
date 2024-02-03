#include "config.h"
#include "Imlib2_Loader.h"

#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>

#define DBG_PFX "LDR-webp"

#define CLAMP(X, MIN, MAX)  ((X) < (MIN) ? (MIN) : ((X) > (MAX) ? (MAX) : (X)))

static const char *const _formats[] = { "webp" };

static int
_load(ImlibImage *im, int load_data)
{
    int             rc;
    WebPData        webp_data;
    WebPDemuxer    *demux;
    WebPIterator    iter;
    int             frame, fcount;
    ImlibImageFrame *pf;

    rc = LOAD_FAIL;

    if (im->fi->fsize < 12)
        return rc;

    webp_data.bytes = im->fi->fdata;
    webp_data.size = im->fi->fsize;

    /* Init (includes signature check) */
    demux = WebPDemux(&webp_data);
    if (!demux)
        goto quit;

    rc = LOAD_BADIMAGE;         /* Format accepted */

    pf = NULL;
    frame = im->frame;
    if (frame > 0)
    {
        fcount = WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT);
        if (frame > 1 && frame > fcount)
            QUIT_WITH_RC(LOAD_BADFRAME);

        pf = __imlib_GetFrame(im);
        if (!pf)
            QUIT_WITH_RC(LOAD_OOM);
        pf->frame_count = fcount;
        pf->loop_count = WebPDemuxGetI(demux, WEBP_FF_LOOP_COUNT);
        if (pf->frame_count > 1)
            pf->frame_flags |= FF_IMAGE_ANIMATED;
        pf->canvas_w = WebPDemuxGetI(demux, WEBP_FF_CANVAS_WIDTH);
        pf->canvas_h = WebPDemuxGetI(demux, WEBP_FF_CANVAS_HEIGHT);

        D("Canvas WxH=%dx%d frames=%d repeat=%d\n",
          pf->canvas_w, pf->canvas_h, pf->frame_count, pf->loop_count);
    }
    else
    {
        frame = 1;
    }

    if (!WebPDemuxGetFrame(demux, frame, &iter))
        goto quit;

    WebPDemuxReleaseIterator(&iter);

    im->w = iter.width;
    im->h = iter.height;
    if (pf)
    {
        pf->frame_x = iter.x_offset;
        pf->frame_y = iter.y_offset;
        pf->frame_delay = iter.duration;
        if (iter.dispose_method == WEBP_MUX_DISPOSE_BACKGROUND)
            pf->frame_flags |= FF_FRAME_DISPOSE_CLEAR;
        if (iter.blend_method == WEBP_MUX_BLEND)
            pf->frame_flags |= FF_FRAME_BLEND;

        D("Canvas WxH=%dx%d frame=%d/%d X,Y=%d,%d WxH=%dx%d alpha=%d T=%d dm=%d co=%d bl=%d\n", //
          pf->canvas_w, pf->canvas_h, iter.frame_num, pf->frame_count,
          pf->frame_x, pf->frame_y, im->w, im->h, iter.has_alpha,
          pf->frame_delay, iter.dispose_method, iter.complete,
          iter.blend_method);
    }

    if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
        goto quit;

    im->has_alpha = iter.has_alpha;

    if (!load_data)
        QUIT_WITH_RC(LOAD_SUCCESS);

    /* Load data */

    if (!__imlib_AllocateData(im))
        QUIT_WITH_RC(LOAD_OOM);

    if (WebPDecodeBGRAInto
        (iter.fragment.bytes, iter.fragment.size, (uint8_t *) im->data,
         sizeof(uint32_t) * im->w * im->h, im->w * 4) == NULL)
        goto quit;

    if (im->lc)
        __imlib_LoadProgress(im, 0, 0, im->w, im->h);

    rc = LOAD_SUCCESS;

  quit:
    if (demux)
        WebPDemuxDelete(demux);

    return rc;
}

static int
webp_write(const uint8_t *data, size_t size, const WebPPicture *pic)
{
    FILE           *f = pic->custom_ptr;

    return fwrite(data, 1, size, f) == size;
}

static int
_save(ImlibImage *im)
{
    int             rc;
    FILE           *f = im->fi->fp;
    ImlibImageTag  *quality_tag;
    ImlibImageTag  *compression_tag;
    WebPConfig      conf;
    WebPPicture     pic;
    int             compression;
    int             lossless;
    int             free_pic = 0;

    rc = LOAD_BADFILE;

    if (!WebPConfigInit(&conf) || !WebPPictureInit(&pic))
        goto quit;

    conf.quality = 75;
    quality_tag = __imlib_GetTag(im, "quality");
    if (quality_tag)
        conf.quality = CLAMP(quality_tag->val, 0, 100);

    compression_tag = __imlib_GetTag(im, "compression");
    /* other savers seem to treat quality 100 as lossless, do the same here. */
    lossless = conf.quality == 100;
    if (lossless)
    {
        compression = 9 - (conf.quality / 10);  /* convert to compression */
        if (compression_tag)
            compression = compression_tag->val;
        WebPConfigLosslessPreset(&conf, CLAMP(compression, 0, 9));
    }
    else if (compression_tag)   /* for lossly encoding, only change conf.method if compression_tag was set */
    {
        conf.method = CLAMP(compression_tag->val, 0, 9);
        conf.method *= 0.67;    /* convert from [0, 9] to [0, 6]. (6/9 == 0.67) */
    }

    if (!WebPValidateConfig(&conf))
    {
        D("WebPValidateConfig failed");
        goto quit;
    }

    pic.use_argb = lossless;    /* use_argb is recommended for lossless */
    pic.width = im->w;
    pic.height = im->h;
    pic.writer = webp_write;
    pic.custom_ptr = f;
    if (!WebPPictureImportBGRA(&pic, (uint8_t *) im->data, im->w * 4))
        QUIT_WITH_RC(LOAD_OOM);
    free_pic = 1;

    if (!WebPEncode(&conf, &pic))
        goto quit;

    rc = LOAD_SUCCESS;

  quit:
    if (free_pic)
        WebPPictureFree(&pic);

    return rc;
}

IMLIB_LOADER(_formats, _load, _save);
