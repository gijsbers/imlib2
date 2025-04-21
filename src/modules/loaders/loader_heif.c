/*
 * Loader for HEIF images.
 *
 * Only loads the primary image for any file, whether it be a still image or an
 * image sequence.
 */
#include "config.h"
#include "Imlib2_Loader.h"
#include "ldrs_util.h"

#include <errno.h>
#include <stdbool.h>
#include <libheif/heif.h>

#define DBG_PFX "LDR-heif"

static const char *const _formats[] = {
    "heif", "heifs", "heic", "heics",
#ifndef BUILD_AVIF_LOADER
    "avci", "avcs", "avif", "avifs"
#endif
};

#define HEIF_BYTES_TO_CHECK 12L
#define HEIF_8BIT_TO_PIXEL_ARGB(plane, has_alpha) \
   PIXEL_ARGB((has_alpha) ? (plane)[3] : 0xff, (plane)[0], (plane)[1], (plane)[2])

#if IMLIB2_DEBUG
static bool
_is_error(const struct heif_error *err)
{
    if (err->code == heif_error_Ok)
        return false;

    D("%s: error=%d:%d: %s\n", "libheif", err->code, err->subcode,
      err->message);

    return true;
}
#define IS_ERROR(err) _is_error(&err)
#else
#define IS_ERROR(err) (err.code != heif_error_Ok)
#endif

static int
_load(ImlibImage *im, int load_data)
{
    int             rc;
    int             img_has_alpha;
    int             stride = 0;
    int             bytes_per_px;
    int             y, x;
    struct heif_error error;
    struct heif_context *ctx = NULL;
    struct heif_image_handle *img_handle = NULL;
    struct heif_image *img_data = NULL;
    struct heif_decoding_options *decode_opts = NULL;
    uint32_t       *ptr;
    const uint8_t  *img_plane = NULL;

    rc = LOAD_FAIL;

    /* input data needs to be atleast 12 bytes */
    if (im->fi->fsize < HEIF_BYTES_TO_CHECK)
        return rc;

    /* check signature */
    switch (heif_check_filetype(im->fi->fdata, im->fi->fsize))
    {
    case heif_filetype_no:
    case heif_filetype_yes_unsupported:
        goto quit;

        /* Have to let heif_filetype_maybe through because mif1 brand gives
         * heif_filetype_maybe on check */
    case heif_filetype_maybe:
    case heif_filetype_yes_supported:
        break;
    }

    ctx = heif_context_alloc();
    if (!ctx)
        goto quit;

    error = heif_context_read_from_memory_without_copy(ctx, im->fi->fdata,
                                                       im->fi->fsize, NULL);
    if (IS_ERROR(error))
        goto quit;

    error = heif_context_get_primary_image_handle(ctx, &img_handle);
    if (IS_ERROR(error))
        goto quit;

    rc = LOAD_BADIMAGE;         /* Format accepted */

    /* Freeing heif_context, since we got primary image handle */
    heif_context_free(ctx);
    ctx = NULL;

    im->w = heif_image_handle_get_width(img_handle);
    im->h = heif_image_handle_get_height(img_handle);
    if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
        goto quit;

    img_has_alpha = heif_image_handle_has_alpha_channel(img_handle);
    im->has_alpha = img_has_alpha;

    if (!load_data)
    {
        rc = LOAD_SUCCESS;
        goto quit;
    }

    /* load data */

    /* Set decoding option to convert HDR to 8-bit if libheif>=1.7.0 and
     * successful allocation of heif_decoding_options */
#if LIBHEIF_HAVE_VERSION(1, 7, 0)
    decode_opts = heif_decoding_options_alloc();
    if (decode_opts)
        decode_opts->convert_hdr_to_8bit = 1;
#endif
    error =
        heif_decode_image(img_handle, &img_data, heif_colorspace_RGB,
                          img_has_alpha ? heif_chroma_interleaved_RGBA :
                          heif_chroma_interleaved_RGB, decode_opts);
    heif_decoding_options_free(decode_opts);
    decode_opts = NULL;
    if (IS_ERROR(error))
        goto quit;

    im->w = heif_image_get_width(img_data, heif_channel_interleaved);
    im->h = heif_image_get_height(img_data, heif_channel_interleaved);
    if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
        goto quit;
    ptr = __imlib_AllocateData(im);
    if (!ptr)
        goto quit;

    img_plane =
        heif_image_get_plane_readonly(img_data, heif_channel_interleaved,
                                      &stride);
    if (!img_plane)
        goto quit;

    /* Divide the number of bits per pixel by 8, always rounding up */
    bytes_per_px =
        (heif_image_get_bits_per_pixel(img_data, heif_channel_interleaved) +
         7) >> 3;
    /* If somehow bytes_per_pixel < 1, set it to 1 */
    bytes_per_px = bytes_per_px < 1 ? 1 : bytes_per_px;

    /* Correct the stride, since img_plane will be incremented after each pixel */
    stride -= im->w * bytes_per_px;
    for (y = 0; y != im->h; y++, img_plane += stride)
    {
        for (x = 0; x != im->w; x++, img_plane += bytes_per_px)
            *(ptr++) = HEIF_8BIT_TO_PIXEL_ARGB(img_plane, img_has_alpha);

        /* Report progress of each row. */
        if (im->lc && __imlib_LoadProgressRows(im, y, 1))
        {
            rc = LOAD_BREAK;
            goto quit;
        }
    }
    rc = LOAD_SUCCESS;

  quit:
    /* Free memory if it is still allocated.
     * Working this way means explicitly setting pointers to NULL if they were
     * freed beforehand to avoid freeing twice. */
    /* decode_opts was freed as soon as decoding was complete */
    heif_image_release(img_data);
    heif_image_handle_release(img_handle);
    heif_context_free(ctx);
    heif_decoding_options_free(decode_opts);

    return rc;
}

static struct heif_error
_heif_writer(struct heif_context *ctx, const void *data, size_t size,
             void *userdata)
{
    struct heif_error error = heif_error_success;
    FILE           *fp = userdata;
    size_t          nw;

    nw = fwrite(data, 1, size, fp);
    if (nw != size)
    {
        error.code = heif_error_Encoding_error;
        error.subcode = errno;
    }

    return error;
}

static int
_heif_write(struct heif_context *ctx, FILE *fp)
{
    struct heif_error error;
    struct heif_writer writer;

    writer.writer_api_version = 1;
    writer.write = _heif_writer;

    error = heif_context_write(ctx, &writer, fp);

    return IS_ERROR(error) ? LOAD_FAIL : LOAD_SUCCESS;
}

static int
_save(ImlibImage *im)
{
    int             rc = LOAD_FAIL;
    ImlibSaverParam imsp;
    int             compr_type;
    bool            has_alpha;
    struct heif_error error;
    struct heif_context *ctx = NULL;
    struct heif_encoder *encoder = NULL;
    struct heif_image *image = NULL;
    int             bit_depth, bypp, stride;
    uint8_t        *img_plane;

    ctx = heif_context_alloc();
    if (!ctx)
        goto quit;

    compr_type = heif_compression_HEVC;
    if (im->fi->name)
    {
        if (strstr(im->fi->name, ".avif"))
            compr_type = heif_compression_AV1;
        else if (strstr(im->fi->name, ".heic"))
            compr_type = heif_compression_HEVC;
    }

    get_saver_params(im, &imsp);

    if (imsp.compr_type >= 0)
        compr_type = imsp.compr_type;

    D("Compression type   : %d\n", compr_type);
    D("Compression/quality: %d/%d\n", imsp.compression, imsp.quality);

    error = heif_context_get_encoder_for_format(ctx, compr_type, &encoder);
    if (IS_ERROR(error))
        goto quit;

    if (imsp.quality == 100)
    {
        heif_encoder_set_lossless(encoder, 1);
    }
    else
    {
        heif_encoder_set_lossless(encoder, 0);
        heif_encoder_set_lossy_quality(encoder, imsp.quality);
    }

    has_alpha = im->has_alpha;

    error = heif_image_create(im->w, im->h, heif_colorspace_RGB,
                              has_alpha ?
                              heif_chroma_interleaved_RGBA :
                              heif_chroma_interleaved_RGB, &image);
    if (IS_ERROR(error))
        goto quit;

    bit_depth = 8;
    heif_image_add_plane(image, heif_channel_interleaved, im->w, im->h,
                         bit_depth);

    img_plane = heif_image_get_plane(image, heif_channel_interleaved, &stride);
    if (!img_plane)
        goto quit;

    bypp = has_alpha ? 4 : 3;

    for (int y = 0; y < im->h; y++)
    {
        const uint8_t  *pi;
        uint8_t        *po;

        pi = (const uint8_t *)(im->data + y * im->w);
        po = img_plane + y * stride;

        for (int x = 0; x < im->w; x++, pi += 4, po += bypp)
        {
            po[0] = pi[2];
            po[1] = pi[1];
            po[2] = pi[0];
            if (has_alpha)
                po[3] = pi[3];
        }
    }

    heif_context_encode_image(ctx, image, encoder, NULL, NULL);

    rc = _heif_write(ctx, im->fi->fp);

  quit:
    heif_image_release(image);
    heif_encoder_release(encoder);
    heif_context_free(ctx);

    return rc;
}

#if !LIBHEIF_HAVE_VERSION(1, 13, 0)

IMLIB_LOADER_KEEP(_formats, _load, _save);

#else

static void
_inex(int init)
{
    if (init)
        heif_init(NULL);
    else
        heif_deinit();
}

IMLIB_LOADER_INEX(_formats, _load, _save, _inex);

#endif
