#include "config.h"
#include "Imlib2_Loader.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

// decoder code taken from commit 8431e3f7:
// https://codeberg.org/NRK/slashtmp/src/branch/master/compression/qoi-dec.c
// encoder code taken from commit d81a0c4c:
// https://codeberg.org/NRK/slashtmp/src/branch/master/compression/qoi-enc.c
//
// simple qoi decoder: https://qoiformat.org/
//
// This is free and unencumbered software released into the public domain.
// For more information, please refer to <https://unlicense.org/>
#define QOIDEC_API static
#define QOIENC_API static
#define DBG_PFX "LDR-qoi"
#if IMLIB2_DEBUG
#define QOIDEC_ASSERT(X)   do { if (!(X)) D("%d: %s\n", __LINE__, #X); } while (0)
#define QOIENC_ASSERT(X)   QOIDEC_ASSERT(X)
#else
#define QOIDEC_ASSERT(X)
#define QOIENC_ASSERT(X)
#endif

static const char *const _formats[] = { "qoi" };

// API

typedef struct {
    uint32_t        w, h;
    // this is where the decoded ARGB32 pixels will be put.
    // to be allocated by the caller by at least `.data_size` bytes before calling qoi_dec().
    uint32_t       *data;
    ptrdiff_t       npixels, data_size;
    uint8_t         channels, colorspace;

    // private
    const uint8_t  *p, *end;
} QoiDecCtx;

typedef enum {
    QOIDEC_OK,
    QOIDEC_NOT_QOI, QOIDEC_CORRUPTED, QOIDEC_ZERO_DIM, QOIDEC_TOO_LARGE,
} QoiDecStatus;

QOIDEC_API QoiDecStatus qoi_dec_init(QoiDecCtx * ctx, const void *buffer,
                                     ptrdiff_t size);
QOIDEC_API QoiDecStatus qoi_dec(QoiDecCtx * ctx);

// implementation

QOIDEC_API      QoiDecStatus
qoi_dec_init(QoiDecCtx *ctx, const void *buffer, ptrdiff_t size)
{
    static const char magic[] = "qoif";

    QOIDEC_ASSERT(size >= 0);

    memset(ctx, 0, sizeof(QoiDecCtx));

    ctx->p = buffer;
    ctx->end = ctx->p + size;
    if (size < 14 || memcmp(ctx->p, magic, sizeof(magic) - 1) != 0)
        return QOIDEC_NOT_QOI;
    ctx->p += 4;

    ctx->w = (uint32_t) ctx->p[0] << 24 | ctx->p[1] << 16 |
        ctx->p[2] << 8 | ctx->p[3];
    ctx->p += 4;
    ctx->h = (uint32_t) ctx->p[0] << 24 | ctx->p[1] << 16 |
        ctx->p[2] << 8 | ctx->p[3];
    ctx->p += 4;
    if (ctx->w == 0 || ctx->h == 0)
    {
        return QOIDEC_ZERO_DIM;
    }
    if ((PTRDIFF_MAX / 4) / ctx->w < ctx->h)
    {
        return QOIDEC_TOO_LARGE;
    }
    ctx->npixels = (ptrdiff_t)ctx->w * ctx->h;
    ctx->data_size = ctx->npixels * 4;

    ctx->channels = *ctx->p++;
    ctx->colorspace = *ctx->p++;
    if (!(ctx->channels == 3 || ctx->channels == 4) ||
        ctx->colorspace & ~0x1u || ctx->end - ctx->p < 8 /* end marker */ )
    {
        return QOIDEC_CORRUPTED;
    }

    return QOIDEC_OK;
}

QOIDEC_API      QoiDecStatus
qoi_dec(QoiDecCtx *ctx)
{
    typedef struct {
        uint8_t         b, g, r, a;
    } Clr;
    Clr             t[64] = { 0 };
    Clr             l = {.a = 0xFF };
    static const uint8_t eof[8] = {[7] = 0x1 };
    const uint8_t  *p = ctx->p, *end = ctx->end;

    QOIDEC_ASSERT(ctx->data != NULL);
    QOIDEC_ASSERT(ctx->p != NULL && ctx->end != NULL);
    QOIDEC_ASSERT(ctx->end - ctx->p >= 8);
    QOIDEC_ASSERT(ctx->p[-14] == 'q' && ctx->p[-13] == 'o');
    QOIDEC_ASSERT(ctx->p[-12] == 'i' && ctx->p[-11] == 'f');

    if ((*p >> 6) == 0x3 && (*p & 0x3F) < 62)
    {                           // ref: https://github.com/phoboslab/qoi/issues/258
        t[(0xFF * 11) % 64] = l;
    }
    for (ptrdiff_t widx = 0; widx < ctx->npixels;)
    {
        uint32_t        c;
        uint8_t         tmp, op;
        int             dg;

        QOIDEC_ASSERT(p <= end);
        if (end - p < 8)
        {
            return QOIDEC_CORRUPTED;
        }
        op = *p++;
        switch (op)
        {
        case 0xFF:
            l.r = *p++;
            l.g = *p++;
            l.b = *p++;
            l.a = *p++;
            break;
        case 0xFE:
            l.r = *p++;
            l.g = *p++;
            l.b = *p++;
            break;
        default:
            switch (op >> 6)
            {
            case 0x3:
                tmp = (op & 0x3F) + 1;
                if (ctx->npixels - widx < tmp)
                {
                    return QOIDEC_CORRUPTED;
                }
                c = (uint32_t) l.a << 24 | l.r << 16 | l.g << 8 | l.b;
                for (int k = 0; k < tmp; ++k)
                {
                    ctx->data[widx++] = c;
                }
                goto next_iter;
                break;
            case 0x0:
                if (op == *p)
                {
                    return QOIDEC_CORRUPTED;    // seriously?
                }
                l = t[op & 0x3F];
                goto no_table;
                break;
            case 0x1:
                l.r += ((op >> 4) & 0x3) - 2;
                l.g += ((op >> 2) & 0x3) - 2;
                l.b += ((op >> 0) & 0x3) - 2;
                break;
            case 0x2:
                tmp = *p++;
                QOIDEC_ASSERT((tmp >> 8) == 0);
                dg = (op & 0x3F) - 32;
                l.r += dg + ((tmp >> 4) - 8);
                l.g += dg;
                l.b += dg + ((tmp & 0xF) - 8);
                break;
            }
            break;
        }

        t[(l.r * 3 + l.g * 5 + l.b * 7 + l.a * 11) % 64] = l;
      no_table:
        ctx->data[widx++] = (uint32_t) l.a << 24 | l.r << 16 | l.g << 8 | l.b;
      next_iter:
        (void)0;                /* no-op */
    }
    if (end - p < (int)sizeof(eof) || memcmp(p, eof, sizeof(eof)) != 0)
        return QOIDEC_CORRUPTED;

    return QOIDEC_OK;
}

//////////// END QoiDec library

typedef struct {
    uint8_t         data[15];
    int8_t          len;
} QoiEncResult;

typedef struct {
    uint32_t        prev;
    uint8_t         run:7;
    uint8_t         no_alpha:1;
    uint32_t        dict[64];
} QoiEncCtx;

enum { QOIENC_NO_ALPHA = 1 << 0, QOIENC_IS_SRGB = 1 << 1 };

/// ctx is assumed to be zero initialized by the caller
QOIENC_API      QoiEncResult
qoi_enc_init(QoiEncCtx *ctx, uint32_t width, uint32_t height, uint32_t flags)
{
    QoiEncResult    res = {.len = 14 };
    uint8_t        *p = res.data;

    QOIENC_ASSERT((flags & ~UINT32_C(3)) == 0);
    QOIENC_ASSERT(ctx->run == 0 && ctx->dict[63] == 0x0);

    *p++ = 'q';
    *p++ = 'o';
    *p++ = 'i';
    *p++ = 'f';
    *p++ = (uint8_t) (width >> 24);
    *p++ = (uint8_t) (width >> 16);
    *p++ = (uint8_t) (width >> 8);
    *p++ = (uint8_t) (width >> 0);
    *p++ = (uint8_t) (height >> 24);
    *p++ = (uint8_t) (height >> 16);
    *p++ = (uint8_t) (height >> 8);
    *p++ = (uint8_t) (height >> 0);
    *p++ = (flags & QOIENC_NO_ALPHA) ? 3 : 4;
    *p++ = (flags & QOIENC_IS_SRGB) ? 0 : 1;
    ctx->prev = UINT32_C(0xFF) << 24;
    ctx->no_alpha = !!(flags & QOIENC_NO_ALPHA);

    return res;
}

QOIENC_API      QoiEncResult
qoi_enc(QoiEncCtx *ctx, uint32_t pixel)
{
    enum {
        OP_RUN = 0xC0, OP_IDX = 0x0, OP_DIFF = 0x40, OP_LUMA = 0x80,
        OP_RGB = 0xFE, OP_RGBA = 0xFF
    };

    QoiEncResult    res = { 0 };
    uint8_t        *p = res.data;

    if (ctx->no_alpha)
        pixel |= UINT32_C(0xFF) << 24;

    if (pixel == ctx->prev)
    {
        if (++ctx->run == 62)
        {
            *p++ = OP_RUN | (ctx->run - 1);
            ctx->run = 0;
        }
        return (res.len = (int8_t) (p - res.data)), res;
    }

    if (ctx->run > 0)
    {
        *p++ = OP_RUN | (ctx->run - 1);
        ctx->run = 0;
    }
    uint32_t        prev = ctx->prev;
    ctx->prev = pixel;

    uint32_t        a = ((pixel >> 24) & 0xFF), r = ((pixel >> 16) & 0xFF),
        g = ((pixel >> 8) & 0xFF), b = ((pixel >> 0) & 0xFF);
    uint32_t        idx = (r * 3 + g * 5 + b * 7 + a * 11) & 63;
    if (ctx->dict[idx] == pixel)
    {
        *p++ = OP_IDX | idx;
        return (res.len = (int8_t) (p - res.data)), res;
    }
    ctx->dict[idx] = pixel;

    int8_t          dr = (int8_t) (r - ((prev >> 16) & 0xFF));
    int8_t          dg = (int8_t) (g - ((prev >> 8) & 0xFF));
    int8_t          db = (int8_t) (b - ((prev >> 0) & 0xFF));
    int8_t          dr_g = dr - dg;
    int8_t          db_g = db - dg;
    if (a != (prev >> 24))
    {
        *p++ = OP_RGBA;
        *p++ = r;
        *p++ = g;
        *p++ = b;
        *p++ = a;
    }
    else if (dr >= -2 && dg >= -2 && db >= -2 && dr <= 1 && dg <= 1 && db <= 1)
    {
        *p++ = OP_DIFF | (((dr + 2) & 0x3) << 4) |
            (((dg + 2) & 0x3) << 2) | (((db + 2) & 0x3) << 0);
    }
    else if (dg >= -32 && dg < 32 &&
             dr_g >= -8 && dr_g < 8 && db_g >= -8 && db_g < 8)
    {
        *p++ = OP_LUMA | ((dg + 32) & 0x3F);
        *p++ = ((dr_g + 8) << 4) | (db_g + 8);
    }
    else
    {
        *p++ = OP_RGB;
        *p++ = r;
        *p++ = g;
        *p++ = b;
    }
    return (res.len = (int8_t) (p - res.data)), res;
}

QOIENC_API      QoiEncResult
qoi_enc_finish(QoiEncCtx *ctx)
{
    QoiEncResult    res = { 0 };
    uint8_t        *p = res.data;
    if (ctx->run > 0)
        *p++ = 0xC0 | (ctx->run - 1);
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x0;
    *p++ = 0x1;
    return (res.len = (int8_t) (p - res.data)), res;
}

//////////// END QoiEnc library

static int
_load(ImlibImage *im, int load_data)
{
    QoiDecCtx       qoi;

    if (qoi_dec_init(&qoi, im->fi->fdata, im->fi->fsize) != QOIDEC_OK)
        return LOAD_FAIL;

    im->w = qoi.w;
    im->h = qoi.h;
    im->has_alpha = qoi.channels == 4;
    if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
        return LOAD_BADIMAGE;
    if (!load_data)
        return LOAD_SUCCESS;

    if (!__imlib_AllocateData(im))
        return LOAD_OOM;
    qoi.data = im->data;
    if (qoi_dec(&qoi) != QOIDEC_OK)
        return LOAD_BADIMAGE;

    if (im->lc)
        __imlib_LoadProgressRows(im, 0, im->h);

    return LOAD_SUCCESS;
}

static int
_out(FILE *f, QoiEncResult res)
{
    return fwrite(res.data, 1, res.len, f) == (size_t)res.len;
}

static int
_save(ImlibImage *im)
{
    FILE           *f = im->fi->fp;
    QoiEncCtx       ctx[1] = { 0 };
    int             flags = 0;
    int             i, j;
    int             w = im->w, h = im->h;
    uint32_t       *imdata = im->data;

    if (!im->has_alpha)
        flags |= QOIENC_NO_ALPHA;

    if (!_out(f, qoi_enc_init(ctx, w, h, flags)))
        return LOAD_BADFILE;

    for (i = 0; i < h; ++i)
    {
        for (j = 0; j < w; ++j)
        {
            if (!_out(f, qoi_enc(ctx, imdata[i * w + j])))
                return LOAD_BADFILE;
        }

        if (im->lc && __imlib_LoadProgressRows(im, i, 1))
            return LOAD_BREAK;
    }

    if (!_out(f, qoi_enc_finish(ctx)))
        return LOAD_BADFILE;

    return LOAD_SUCCESS;
}

IMLIB_LOADER(_formats, _load, _save);
