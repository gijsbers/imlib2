#include "config.h"
#include "Imlib2_Loader.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

// decoder code taken from commit 8691a3ba:
// https://codeberg.org/NRK/slashtmp/src/branch/master/compression/qoi-dec.c
//
// simple qoi decoder: https://qoiformat.org/
//
// This is free and unencumbered software released into the public domain.
// For more information, please refer to <https://unlicense.org/>
#define QOIDEC_API static
#define DBG_PFX "LDR-qoi"
#if IMLIB2_DEBUG
#define QOIDEC_ASSERT(X)   do { if (!(X)) D("%d: %s\n", __LINE__, #X); } while (0)
#else
#define QOIDEC_ASSERT(X)
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
    static const uint8_t magic[4] = "qoif";

    QOIDEC_ASSERT(size >= 0);

    memset(ctx, 0, sizeof(QoiDecCtx));

    ctx->p = buffer;
    ctx->end = ctx->p + size;
    if (size < 14 || memcmp(ctx->p, magic, sizeof(magic)) != 0)
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
    uint8_t         lop = -1;
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
                goto no_write;
                break;
            case 0x0:
                if (op == lop)
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
      no_write:
        lop = op;
    }
    if (end - p < (int)sizeof(eof) || memcmp(p, eof, sizeof(eof)) != 0)
        return QOIDEC_CORRUPTED;

    return QOIDEC_OK;
}

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

IMLIB_LOADER(_formats, _load, NULL);
