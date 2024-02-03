#include "config.h"
#include "Imlib2_Loader.h"

#define MAX_RUNNERS 4           /* Maybe set to Ncpu/2? */

#include <jxl/decode.h>
#include <jxl/encode.h>
#if MAX_RUNNERS > 0
#include <jxl/thread_parallel_runner.h>
#endif

#define DBG_PFX "LDR-jxl"

static const char *const _formats[] = { "jxl" };

static void
_scanline_cb(void *opaque, size_t x, size_t y,
             size_t num_pixels, const void *pixels)
{
    ImlibImage     *im = opaque;
    const uint8_t  *pix = pixels;
    uint32_t       *imdata;
    size_t          i;

    DL("%s: x,y=%ld,%ld len=%lu\n", __func__,
       (long)x, (long)y, (long)num_pixels);

    imdata = im->data + (im->w * y) + x;

    /* libjxl outputs ABGR pixels (stream order RGBA) - convert to ARGB */
    for (i = 0; i < num_pixels; i++, pix += 4)
        *imdata++ = PIXEL_ARGB(pix[3], pix[0], pix[1], pix[2]);

    /* Due to possible multithreading it's probably best not do do
     * progress calbacks here. */
}

static int
_load(ImlibImage *im, int load_data)
{
    static const JxlPixelFormat pbuf_fmt = {
        .num_channels = 4,
        .data_type = JXL_TYPE_UINT8,
        .endianness = JXL_NATIVE_ENDIAN,
    };

    int             rc;
    JxlDecoderStatus jst;
    JxlDecoder     *dec;
    JxlBasicInfo    info;
    JxlFrameHeader  fhdr;
    int             frame, delay_unit;
    ImlibImageFrame *pf;

#if MAX_RUNNERS > 0
    unsigned int    n_runners;
    JxlParallelRunner *runner = NULL;
#endif

    rc = LOAD_FAIL;
    dec = NULL;

    switch (JxlSignatureCheck(im->fi->fdata, 128))
    {
    default:
//   case JXL_SIG_NOT_ENOUGH_BYTES:
//   case JXL_SIG_INVALID:
        goto quit;
    case JXL_SIG_CODESTREAM:
    case JXL_SIG_CONTAINER:
        break;
    }

    rc = LOAD_BADIMAGE;         /* Format accepted */

    dec = JxlDecoderCreate(NULL);
    if (!dec)
        goto quit;

#if MAX_RUNNERS > 0
    n_runners = JxlThreadParallelRunnerDefaultNumWorkerThreads();
    if (n_runners > MAX_RUNNERS)
        n_runners = MAX_RUNNERS;
    D("n_runners = %d\n", n_runners);
    runner = JxlThreadParallelRunnerCreate(NULL, n_runners);
    if (!runner)
        goto quit;

    jst = JxlDecoderSetParallelRunner(dec, JxlThreadParallelRunner, runner);
    if (jst != JXL_DEC_SUCCESS)
        goto quit;
#endif                          /* MAX_RUNNERS */

    jst =
        JxlDecoderSubscribeEvents(dec,
                                  JXL_DEC_BASIC_INFO | JXL_DEC_FRAME |
                                  JXL_DEC_FULL_IMAGE);
    if (jst != JXL_DEC_SUCCESS)
        goto quit;

    jst = JxlDecoderSetInput(dec, im->fi->fdata, im->fi->fsize);
    if (jst != JXL_DEC_SUCCESS)
        goto quit;

    delay_unit = 0;
    frame = im->frame;
    pf = NULL;

    for (;;)
    {
        jst = JxlDecoderProcessInput(dec);
        DL("Event 0x%04x\n", jst);
        switch (jst)
        {
        default:
            D("Unexpected status: %d\n", jst);
            goto quit;

        case JXL_DEC_BASIC_INFO:
            jst = JxlDecoderGetBasicInfo(dec, &info);
            if (jst != JXL_DEC_SUCCESS)
                goto quit;

            D("WxH=%dx%d alpha=%d bps=%d nc=%d+%d\n", info.xsize, info.ysize,
              info.alpha_bits, info.bits_per_sample,
              info.num_color_channels, info.num_extra_channels);
            if (info.have_animation)
            {
                D("anim=%d loops=%u tps_num/den=%u/%u\n", info.have_animation,
                  info.animation.num_loops, info.animation.tps_numerator,
                  info.animation.tps_denominator);
                delay_unit = info.animation.tps_denominator * 1000;
                if (info.animation.tps_numerator > 0)
                    delay_unit /= info.animation.tps_numerator;
            }

            if (!IMAGE_DIMENSIONS_OK(info.xsize, info.ysize))
                goto quit;

            im->w = info.xsize;
            im->h = info.ysize;
            im->has_alpha = info.alpha_bits > 0;

            if (frame > 0)
            {
                if (info.have_animation)
                {
                    pf = __imlib_GetFrame(im);
                    if (!pf)
                        QUIT_WITH_RC(LOAD_OOM);
                    pf->frame_count = 1234567890;       // FIXME - Hack
                    pf->loop_count = info.animation.num_loops;
                    pf->frame_flags |= FF_IMAGE_ANIMATED;
                    pf->canvas_w = info.xsize;
                    pf->canvas_h = info.ysize;

                    D("Canvas WxH=%dx%d frames=%d repeat=%d\n",
                      pf->canvas_w, pf->canvas_h,
                      pf->frame_count, pf->loop_count);

                    if (frame > 1 && pf->frame_count > 0 &&
                        frame > pf->frame_count)
                        QUIT_WITH_RC(LOAD_BADFRAME);
                }

                if (frame > 1)
                {
                    /* Fast forward to desired frame */
                    JxlDecoderSkipFrames(dec, frame - 1);
                }
            }

            if (!load_data)
                QUIT_WITH_RC(LOAD_SUCCESS);
            break;

        case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
            if (!__imlib_AllocateData(im))
                QUIT_WITH_RC(LOAD_OOM);

            jst = JxlDecoderSetImageOutCallback(dec, &pbuf_fmt,
                                                _scanline_cb, im);
            if (jst != JXL_DEC_SUCCESS)
                goto quit;
            break;

        case JXL_DEC_FRAME:
            if (!pf)
                break;
            JxlDecoderGetFrameHeader(dec, &fhdr);
            if (fhdr.is_last)
                pf->frame_count = frame;
            pf->frame_delay = fhdr.duration * delay_unit;
            D("Frame duration=%d tc=%08x nl=%d last=%d\n",
              pf->frame_delay, fhdr.timecode, fhdr.name_length, fhdr.is_last);
            break;

        case JXL_DEC_FULL_IMAGE:
            goto done;
        }
    }
  done:

    if (im->lc)
        __imlib_LoadProgress(im, 0, 0, im->w, im->h);

    rc = LOAD_SUCCESS;

  quit:
#if MAX_RUNNERS > 0
    if (runner)
        JxlThreadParallelRunnerDestroy(runner);
#endif
    if (dec)
        JxlDecoderDestroy(dec);

    return rc;
}

static int
_save(ImlibImage *im)
{
    int             rc;
    JxlEncoderStatus jst;
    JxlEncoder     *enc;
    JxlBasicInfo    info;
    JxlEncoderFrameSettings *opts;

    JxlPixelFormat  pbuf_fmt = {
        .data_type = JXL_TYPE_UINT8,
        .endianness = JXL_NATIVE_ENDIAN,
    };
    ImlibImageTag  *tag;
    const uint32_t *imdata;
    uint8_t        *buffer = NULL, *buf_ptr;
    size_t          buf_len, i, npix;

#if MAX_RUNNERS > 0
    unsigned int    n_runners;
    JxlParallelRunner *runner = NULL;
#endif

    rc = LOAD_BADFILE;

    enc = JxlEncoderCreate(NULL);
    if (!enc)
        goto quit;

#if MAX_RUNNERS > 0
    n_runners = JxlThreadParallelRunnerDefaultNumWorkerThreads();
    if (n_runners > MAX_RUNNERS)
        n_runners = MAX_RUNNERS;
    D("n_runners = %d\n", n_runners);
    runner = JxlThreadParallelRunnerCreate(NULL, n_runners);
    if (!runner)
        goto quit;

    jst = JxlEncoderSetParallelRunner(enc, JxlThreadParallelRunner, runner);
    if (jst != JXL_ENC_SUCCESS)
        goto quit;
#endif                          /* MAX_RUNNERS */

    JxlEncoderInitBasicInfo(&info);
    info.xsize = im->w;
    info.ysize = im->h;
    if (im->has_alpha)
    {
        info.alpha_bits = 8;
        info.num_extra_channels = 1;
    }

    jst = JxlEncoderSetBasicInfo(enc, &info);
    if (jst != JXL_ENC_SUCCESS)
        goto quit;

    opts = JxlEncoderFrameSettingsCreate(enc, NULL);
    if (!opts)
        goto quit;

    tag = __imlib_GetTag(im, "quality");
    if (tag)
    {
        int             quality;
        float           distance;

        quality = (tag->val) >= 0 ? tag->val : 0;
        if (quality >= 100)
        {
            D("Quality=%d: Lossless\n", quality);
            JxlEncoderSetFrameDistance(opts, 0.f);
            JxlEncoderSetFrameLossless(opts, JXL_TRUE);
        }
        else
        {
            distance = 15.f * (1.f - .01 * quality);    // 0 - 100 -> 15 - 0
            D("Quality=%d: Distance=%.1f\n", quality, distance);
            JxlEncoderSetFrameLossless(opts, JXL_FALSE);
            JxlEncoderSetFrameDistance(opts, distance);
        }
    }

    tag = __imlib_GetTag(im, "compression");
    if (tag)
    {
        int             compression;

        compression = (tag->val < 1) ? 1 : (tag->val > 9) ? 9 : tag->val;
        D("Compression=%d\n", compression);
        JxlEncoderFrameSettingsSetOption(opts, JXL_ENC_FRAME_SETTING_EFFORT,
                                         compression);
    }

    // Create buffer for format conversion and output
    pbuf_fmt.num_channels = (im->has_alpha) ? 4 : 3;
    npix = im->w * im->h;
    buf_len = pbuf_fmt.num_channels * npix;
    if (buf_len < 4096)
        buf_len = 4096;         // Not too small for output

    buffer = malloc(buf_len);
    if (!buffer)
        QUIT_WITH_RC(LOAD_OOM);

    // Convert format for libjxl
    imdata = im->data;
    buf_ptr = buffer;
    if (pbuf_fmt.num_channels == 3)
    {
        for (i = 0; i < npix; i++, imdata++, buf_ptr += 3)
        {
            buf_ptr[0] = R_VAL(imdata);
            buf_ptr[1] = G_VAL(imdata);
            buf_ptr[2] = B_VAL(imdata);
        }
    }
    else
    {
        for (i = 0; i < npix; i++, imdata++, buf_ptr += 4)
        {
            buf_ptr[0] = R_VAL(imdata);
            buf_ptr[1] = G_VAL(imdata);
            buf_ptr[2] = B_VAL(imdata);
            buf_ptr[3] = A_VAL(imdata);
        }
    }

    jst = JxlEncoderAddImageFrame(opts, &pbuf_fmt, buffer, buf_len);
    if (jst != JXL_ENC_SUCCESS)
        goto quit;

    JxlEncoderCloseInput(enc);

    for (;;)
    {
        uint8_t        *next_out;
        size_t          avail_out;

        next_out = buffer;
        avail_out = buf_len;
        jst = JxlEncoderProcessOutput(enc, &next_out, &avail_out);
        switch (jst)
        {
        default:
            goto quit;
        case JXL_ENC_SUCCESS:
        case JXL_ENC_NEED_MORE_OUTPUT:
            D("Write: jst=%d %d\n", jst, (int)(buf_len - avail_out));
            if (next_out == buffer)
                goto quit;

            if (fwrite(buffer, 1, buf_len - avail_out, im->fi->fp) !=
                buf_len - avail_out)
                goto quit;

            if (jst == JXL_ENC_SUCCESS)
                goto done;

            break;
        }
    }
  done:

    rc = LOAD_SUCCESS;

  quit:
    free(buffer);
#if MAX_RUNNERS > 0
    if (runner)
        JxlThreadParallelRunnerDestroy(runner);
#endif
    if (enc)
        JxlEncoderDestroy(enc);

    return rc;
}

IMLIB_LOADER(_formats, _load, _save);
