#include "common.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "asm_c.h"
#include "image.h"
#include "scale.h"

#undef DO_MMX_ASM               // __imlib_Scale_mmx_AARGBA() is broken

/*\ NB: If you change this, don't forget asm_scale.S \*/
struct _imlib_scale_info {
    int            *xpoints;
    int            *ypoints;
    int            *xapoints;
    int            *yapoints;
    int             xup_yup;
    uint32_t       *pix_assert;
};

#define INV_XAP                   (256 - xapoints[x])
#define XAP                       (xapoints[x])
#define INV_YAP                   (256 - yapoints[dyy + y])
#define YAP                       (yapoints[dyy + y])

static int     *
__imlib_CalcPoints(int sw, int dw_, int b1, int b2, bool aa, int up)
{
    int            *p, i;
    int             val, inc, dw, ss, dd, corr;

    dw = (dw_ >= 0) ? dw_ : -dw_;

    p = malloc(dw * sizeof(int));
    if (!p)
        return NULL;

    val = MIN(sw, dw);
    inc = b1 + b2;
    if (val < inc)
    {
        b1 = (val * b1 + inc / 2) / inc;
        b2 = val - b1;
    }

    /* Border 1 */
    for (i = 0; i < b1; i++)
        p[i] = i;

    /* Center */
    ss = sw - (b1 + b2);
    dd = dw - (b1 + b2);
    if (dd > 0)
    {
        if (aa && dd > 1)
        {
            corr = (up) ? 1 : 0;
            val = b1 << 16;
            inc = ((ss - corr) << 16) / (dd - corr);
            for (; i < dw - b2; i++)
            {
                p[i] = val >> 16;
                val += inc;
            }
        }
        else
        {
            for (i = 0; i < dd; i++)
                p[b1 + i] = b1 + (i * ss) / dd;
            i = dw - b2;
        }
    }

    /* Border 2 */
    val = sw - b2;
    for (; i < dw; i++)
        p[i] = val++;

    if (dw_ < 0)
        for (i = dw / 2; --i >= 0;)
        {
            int             tmp = p[i];

            p[i] = p[dw - i - 1];
            p[dw - i - 1] = tmp;
        }

    return p;
}

static int     *
__imlib_CalcApoints(int sw, int dw_, int b1, int b2, int up)
{
    int            *p, i;
    int             val, inc, dw, ss, dd, corr;

    dw = (dw_ >= 0) ? dw_ : -dw_;

    p = malloc(dw * sizeof(int));
    if (!p)
        return NULL;

    val = MIN(sw, dw);
    inc = b1 + b2;
    if (val < inc)
    {
        b1 = (val * b1 + inc / 2) / inc;
        b2 = val - b1;
    }

    ss = sw - (b1 + b2);
    dd = dw - (b1 + b2);
    if (up)
    {
        /* Scaling up */

        /* Border 1 */
        for (i = 0; i < b1; i++)
            p[i] = 0;

        /* Center */
        if (dd > 0)
        {
            corr = (dd > 1) ? 1 : 0;
            ss -= corr;
            dd -= corr;
            val = 0;
            inc = (ss << 16) / dd;
            for (; i < dw - b2; i++)
            {
                p[i] = (val >> 8) - ((val >> 8) & 0xffffff00);
                val += inc;
            }
        }

        /* Border 2 */
        for (; i < dw; i++)
            p[i] = 0;
    }
    else
    {
        /* Scaling down */

        /* Border 1 */
        for (i = 0; i < b1; i++)
            p[i] = (1 << (16 + 14)) + (1 << 14);

        /* Center */
        if (dd > 0)
        {
            int             ap, Cp;

            val = 0;
            inc = (ss << 16) / dd;
            Cp = ((dd << 14) / ss) + 1;
            for (; i < dw - b2; i++)
            {
                ap = ((0x100 - ((val >> 8) & 0xff)) * Cp) >> 8;
                p[i] = ap | (Cp << 16);
                val += inc;
            }
        }

        /* Border 2 */
        for (; i < dw; i++)
            p[i] = (1 << (16 + 14)) + (1 << 14);
    }

    if (dw_ < 0)
    {
        for (i = dw / 2; --i >= 0;)
        {
            int             tmp = p[i];

            p[i] = p[dw - i - 1];
            p[dw - i - 1] = tmp;
        }
    }

    return p;
}

ImlibScaleInfo *
__imlib_FreeScaleInfo(ImlibScaleInfo *isi)
{
    if (isi)
    {
        free(isi->xpoints);
        free(isi->ypoints);
        free(isi->xapoints);
        free(isi->yapoints);
        free(isi);
    }
    return NULL;
}

ImlibScaleInfo *
__imlib_CalcScaleInfo(ImlibImage *im, int sw, int sh, int dw, int dh, bool aa)
{
    ImlibScaleInfo *isi;
    int             scw, sch;

    scw = dw * im->w / sw;
    sch = dh * im->h / sh;

    isi = malloc(sizeof(ImlibScaleInfo));
    if (!isi)
        return NULL;
    memset(isi, 0, sizeof(ImlibScaleInfo));

    isi->pix_assert = im->data + im->w * im->h;

    isi->xup_yup = (abs(dw) >= sw) + ((abs(dh) >= sh) << 1);

    isi->xpoints = __imlib_CalcPoints(im->w, scw,
                                      im->border.left, im->border.right,
                                      aa, isi->xup_yup & 1);
    if (!isi->xpoints)
        goto bail;

    isi->ypoints = __imlib_CalcPoints(im->h, sch,
                                      im->border.top, im->border.bottom,
                                      aa, isi->xup_yup & 2);
    if (!isi->ypoints)
        goto bail;

    if (aa)
    {
        isi->xapoints = __imlib_CalcApoints(im->w, scw,
                                            im->border.left, im->border.right,
                                            isi->xup_yup & 1);
        if (!isi->xapoints)
            goto bail;

        isi->yapoints = __imlib_CalcApoints(im->h, sch,
                                            im->border.top, im->border.bottom,
                                            isi->xup_yup & 2);
        if (!isi->yapoints)
            goto bail;
    }

    return isi;

  bail:
    return __imlib_FreeScaleInfo(isi);
}

/* scale by pixel sampling only */
static void
__imlib_ScaleSampleRGBA(ImlibScaleInfo *isi, uint32_t *srce, uint32_t *dest,
                        int dxx, int dyy, int dx, int dy, int dw, int dh,
                        int dow, int sow)
{
    uint32_t       *sptr, *dptr;
    int             x, y, end;
    int            *ypoints = isi->ypoints;
    int            *xpoints = isi->xpoints;

    /* whats the last pixel on the line so we stop there */
    end = dxx + dw;

    for (y = 0; y < dh; y++)
    {
        /* get the pointer to the start of the destination scanline */
        dptr = dest + dx + (y + dy) * dow;
        /* calculate the source line we'll scan from */
        sptr = srce + ypoints[dyy + y] * sow;
        /* go thru the scanline and copy across */
        for (x = dxx; x < end; x++)
            *dptr++ = sptr[xpoints[x]];
    }
}

/* FIXME: NEED to optimise ScaleAARGBA - currently its "ok" but needs work*/

/* scale by area sampling */
static void
__imlib_ScaleAARGBA(ImlibScaleInfo *isi, uint32_t *srce, uint32_t *dest,
                    int dxx, int dyy, int dx, int dy, int dw, int dh, int dow,
                    int sow)
{
    uint32_t       *sptr, *dptr;
    int             x, y, end;
    int            *ypoints;
    int            *xpoints;
    int            *xapoints;
    int            *yapoints;

#ifdef DO_MMX_ASM
    if (__imlib_do_asm())
    {
        __imlib_Scale_mmx_AARGBA(isi, dest, dxx, dyy, dx, dy, dw, dh, dow, sow);
        return;
    }
#endif

    ypoints = isi->ypoints;
    xpoints = isi->xpoints;
    xapoints = isi->xapoints;
    yapoints = isi->yapoints;

    end = dxx + dw;
    if (isi->xup_yup == 3)
    {
        /* Scaling up both ways */

        for (y = 0; y < dh; y++)
        {
            /* calculate the source line we'll scan from */
            sptr = srce + ypoints[dyy + y] * sow;
            dptr = dest + dx + (y + dy) * dow;
            if (YAP > 0)
            {
                for (x = dxx; x < end; x++)
                {
                    int             r, g, b, a;
                    int             rr, gg, bb, aa;
                    uint32_t       *pix;

                    pix = sptr + xpoints[x];
                    if (XAP > 0)
                    {
                        r = R_VAL(pix) * INV_XAP;
                        g = G_VAL(pix) * INV_XAP;
                        b = B_VAL(pix) * INV_XAP;
                        a = A_VAL(pix) * INV_XAP;
                        pix++;
                        r += R_VAL(pix) * XAP;
                        g += G_VAL(pix) * XAP;
                        b += B_VAL(pix) * XAP;
                        a += A_VAL(pix) * XAP;
                        pix += sow;
                        rr = R_VAL(pix) * XAP;
                        gg = G_VAL(pix) * XAP;
                        bb = B_VAL(pix) * XAP;
                        aa = A_VAL(pix) * XAP;
                        pix--;
                        rr += R_VAL(pix) * INV_XAP;
                        gg += G_VAL(pix) * INV_XAP;
                        bb += B_VAL(pix) * INV_XAP;
                        aa += A_VAL(pix) * INV_XAP;
                        r = ((rr * YAP) + (r * INV_YAP)) >> 16;
                        g = ((gg * YAP) + (g * INV_YAP)) >> 16;
                        b = ((bb * YAP) + (b * INV_YAP)) >> 16;
                        a = ((aa * YAP) + (a * INV_YAP)) >> 16;
                        *dptr++ = PIXEL_ARGB(a, r, g, b);
                    }
                    else
                    {
                        r = R_VAL(pix) * INV_YAP;
                        g = G_VAL(pix) * INV_YAP;
                        b = B_VAL(pix) * INV_YAP;
                        a = A_VAL(pix) * INV_YAP;
                        pix += sow;
                        r += R_VAL(pix) * YAP;
                        g += G_VAL(pix) * YAP;
                        b += B_VAL(pix) * YAP;
                        a += A_VAL(pix) * YAP;
                        r >>= 8;
                        g >>= 8;
                        b >>= 8;
                        a >>= 8;
                        *dptr++ = PIXEL_ARGB(a, r, g, b);
                    }
                }
            }
            else
            {
                for (x = dxx; x < end; x++)
                {
                    int             r, g, b, a;
                    uint32_t       *pix;

                    pix = sptr + xpoints[x];
                    if (XAP > 0)
                    {
                        r = R_VAL(pix) * INV_XAP;
                        g = G_VAL(pix) * INV_XAP;
                        b = B_VAL(pix) * INV_XAP;
                        a = A_VAL(pix) * INV_XAP;
                        pix++;
                        r += R_VAL(pix) * XAP;
                        g += G_VAL(pix) * XAP;
                        b += B_VAL(pix) * XAP;
                        a += A_VAL(pix) * XAP;
                        r >>= 8;
                        g >>= 8;
                        b >>= 8;
                        a >>= 8;
                        *dptr++ = PIXEL_ARGB(a, r, g, b);
                    }
                    else
                    {
                        *dptr++ = *pix;
                    }
                }
            }
        }
    }
    else if (isi->xup_yup == 1)
    {
        /* Scaling down vertically */

        int             Cy, j;
        uint32_t       *pix;
        int             r, g, b, a, rr, gg, bb, aa;
        int             yap;

        for (y = 0; y < dh; y++)
        {
            Cy = YAP >> 16;
            yap = YAP & 0xffff;

            sptr = srce + ypoints[dyy + y] * sow;
            dptr = dest + dx + (y + dy) * dow;
            for (x = dxx; x < end; x++)
            {
                pix = sptr + xpoints[x];
                r = (R_VAL(pix) * yap) >> 10;
                g = (G_VAL(pix) * yap) >> 10;
                b = (B_VAL(pix) * yap) >> 10;
                a = (A_VAL(pix) * yap) >> 10;
                for (j = (1 << 14) - yap; j > Cy; j -= Cy)
                {
                    pix += sow;
                    r += (R_VAL(pix) * Cy) >> 10;
                    g += (G_VAL(pix) * Cy) >> 10;
                    b += (B_VAL(pix) * Cy) >> 10;
                    a += (A_VAL(pix) * Cy) >> 10;
                }
                if (j > 0)
                {
                    pix += sow;
                    r += (R_VAL(pix) * j) >> 10;
                    g += (G_VAL(pix) * j) >> 10;
                    b += (B_VAL(pix) * j) >> 10;
                    a += (A_VAL(pix) * j) >> 10;
                }
                assert(pix < isi->pix_assert);
                if (XAP > 0)
                {
                    pix = sptr + xpoints[x] + 1;
                    rr = (R_VAL(pix) * yap) >> 10;
                    gg = (G_VAL(pix) * yap) >> 10;
                    bb = (B_VAL(pix) * yap) >> 10;
                    aa = (A_VAL(pix) * yap) >> 10;
                    for (j = (1 << 14) - yap; j > Cy; j -= Cy)
                    {
                        pix += sow;
                        rr += (R_VAL(pix) * Cy) >> 10;
                        gg += (G_VAL(pix) * Cy) >> 10;
                        bb += (B_VAL(pix) * Cy) >> 10;
                        aa += (A_VAL(pix) * Cy) >> 10;
                    }
                    if (j > 0)
                    {
                        pix += sow;
                        rr += (R_VAL(pix) * j) >> 10;
                        gg += (G_VAL(pix) * j) >> 10;
                        bb += (B_VAL(pix) * j) >> 10;
                        aa += (A_VAL(pix) * j) >> 10;
                    }
                    assert(pix < isi->pix_assert);
                    r = r * INV_XAP;
                    g = g * INV_XAP;
                    b = b * INV_XAP;
                    a = a * INV_XAP;
                    r = (r + ((rr * XAP))) >> 12;
                    g = (g + ((gg * XAP))) >> 12;
                    b = (b + ((bb * XAP))) >> 12;
                    a = (a + ((aa * XAP))) >> 12;
                }
                else
                {
                    r >>= 4;
                    g >>= 4;
                    b >>= 4;
                    a >>= 4;
                }
                *dptr++ = PIXEL_ARGB(a, r, g, b);
            }
        }
    }
    else if (isi->xup_yup == 2)
    {
        /* Scaling down horizontally */

        int             Cx, j;
        uint32_t       *pix;
        int             r, g, b, a, rr, gg, bb, aa;
        int             xap;

        for (y = 0; y < dh; y++)
        {
            sptr = srce + ypoints[dyy + y] * sow;
            dptr = dest + dx + (y + dy) * dow;
            for (x = dxx; x < end; x++)
            {
                Cx = XAP >> 16;
                xap = XAP & 0xffff;

                pix = sptr + xpoints[x];
                r = (R_VAL(pix) * xap) >> 10;
                g = (G_VAL(pix) * xap) >> 10;
                b = (B_VAL(pix) * xap) >> 10;
                a = (A_VAL(pix) * xap) >> 10;
                for (j = (1 << 14) - xap; j > Cx; j -= Cx)
                {
                    pix++;
                    r += (R_VAL(pix) * Cx) >> 10;
                    g += (G_VAL(pix) * Cx) >> 10;
                    b += (B_VAL(pix) * Cx) >> 10;
                    a += (A_VAL(pix) * Cx) >> 10;
                }
                if (j > 0)
                {
                    pix++;
                    r += (R_VAL(pix) * j) >> 10;
                    g += (G_VAL(pix) * j) >> 10;
                    b += (B_VAL(pix) * j) >> 10;
                    a += (A_VAL(pix) * j) >> 10;
                }
                assert(pix < isi->pix_assert);
                if (YAP > 0)
                {
                    pix = sptr + xpoints[x] + sow;
                    rr = (R_VAL(pix) * xap) >> 10;
                    gg = (G_VAL(pix) * xap) >> 10;
                    bb = (B_VAL(pix) * xap) >> 10;
                    aa = (A_VAL(pix) * xap) >> 10;
                    for (j = (1 << 14) - xap; j > Cx; j -= Cx)
                    {
                        pix++;
                        rr += (R_VAL(pix) * Cx) >> 10;
                        gg += (G_VAL(pix) * Cx) >> 10;
                        bb += (B_VAL(pix) * Cx) >> 10;
                        aa += (A_VAL(pix) * Cx) >> 10;
                    }
                    if (j > 0)
                    {
                        pix++;
                        rr += (R_VAL(pix) * j) >> 10;
                        gg += (G_VAL(pix) * j) >> 10;
                        bb += (B_VAL(pix) * j) >> 10;
                        aa += (A_VAL(pix) * j) >> 10;
                    }
                    assert(pix < isi->pix_assert);
                    r = r * INV_YAP;
                    g = g * INV_YAP;
                    b = b * INV_YAP;
                    a = a * INV_YAP;
                    r = (r + ((rr * YAP))) >> 12;
                    g = (g + ((gg * YAP))) >> 12;
                    b = (b + ((bb * YAP))) >> 12;
                    a = (a + ((aa * YAP))) >> 12;
                }
                else
                {
                    r >>= 4;
                    g >>= 4;
                    b >>= 4;
                    a >>= 4;
                }
                *dptr++ = PIXEL_ARGB(a, r, g, b);
            }
        }
    }
    else
    {
        /* Scaling down horizontally & vertically */

        int             Cx, Cy, i, j;
        uint32_t       *pix;
        int             a, r, g, b, ax, rx, gx, bx;
        int             xap, yap;

        for (y = 0; y < dh; y++)
        {
            Cy = YAP >> 16;
            yap = YAP & 0xffff;

            dptr = dest + dx + (y + dy) * dow;
            for (x = dxx; x < end; x++)
            {
                Cx = XAP >> 16;
                xap = XAP & 0xffff;

                sptr = srce + ypoints[dyy + y] * sow + xpoints[x];
                pix = sptr;
                sptr += sow;
                rx = (R_VAL(pix) * xap) >> 9;
                gx = (G_VAL(pix) * xap) >> 9;
                bx = (B_VAL(pix) * xap) >> 9;
                ax = (A_VAL(pix) * xap) >> 9;
                for (i = (1 << 14) - xap; i > Cx; i -= Cx)
                {
                    pix++;
                    rx += (R_VAL(pix) * Cx) >> 9;
                    gx += (G_VAL(pix) * Cx) >> 9;
                    bx += (B_VAL(pix) * Cx) >> 9;
                    ax += (A_VAL(pix) * Cx) >> 9;
                }
                if (i > 0)
                {
                    pix++;
                    rx += (R_VAL(pix) * i) >> 9;
                    gx += (G_VAL(pix) * i) >> 9;
                    bx += (B_VAL(pix) * i) >> 9;
                    ax += (A_VAL(pix) * i) >> 9;
                }

                r = (rx * yap) >> 14;
                g = (gx * yap) >> 14;
                b = (bx * yap) >> 14;
                a = (ax * yap) >> 14;

                for (j = (1 << 14) - yap; j > Cy; j -= Cy)
                {
                    pix = sptr;
                    sptr += sow;
                    rx = (R_VAL(pix) * xap) >> 9;
                    gx = (G_VAL(pix) * xap) >> 9;
                    bx = (B_VAL(pix) * xap) >> 9;
                    ax = (A_VAL(pix) * xap) >> 9;
                    for (i = (1 << 14) - xap; i > Cx; i -= Cx)
                    {
                        pix++;
                        rx += (R_VAL(pix) * Cx) >> 9;
                        gx += (G_VAL(pix) * Cx) >> 9;
                        bx += (B_VAL(pix) * Cx) >> 9;
                        ax += (A_VAL(pix) * Cx) >> 9;
                    }
                    if (i > 0)
                    {
                        pix++;
                        rx += (R_VAL(pix) * i) >> 9;
                        gx += (G_VAL(pix) * i) >> 9;
                        bx += (B_VAL(pix) * i) >> 9;
                        ax += (A_VAL(pix) * i) >> 9;
                    }

                    r += (rx * Cy) >> 14;
                    g += (gx * Cy) >> 14;
                    b += (bx * Cy) >> 14;
                    a += (ax * Cy) >> 14;
                }
                if (j > 0)
                {
                    pix = sptr;
                    rx = (R_VAL(pix) * xap) >> 9;
                    gx = (G_VAL(pix) * xap) >> 9;
                    bx = (B_VAL(pix) * xap) >> 9;
                    ax = (A_VAL(pix) * xap) >> 9;
                    for (i = (1 << 14) - xap; i > Cx; i -= Cx)
                    {
                        pix++;
                        rx += (R_VAL(pix) * Cx) >> 9;
                        gx += (G_VAL(pix) * Cx) >> 9;
                        bx += (B_VAL(pix) * Cx) >> 9;
                        ax += (A_VAL(pix) * Cx) >> 9;
                    }
                    if (i > 0)
                    {
                        pix++;
                        rx += (R_VAL(pix) * i) >> 9;
                        gx += (G_VAL(pix) * i) >> 9;
                        bx += (B_VAL(pix) * i) >> 9;
                        ax += (A_VAL(pix) * i) >> 9;
                    }

                    r += (rx * j) >> 14;
                    g += (gx * j) >> 14;
                    b += (bx * j) >> 14;
                    a += (ax * j) >> 14;
                }

                R_VAL(dptr) = r >> 5;
                G_VAL(dptr) = g >> 5;
                B_VAL(dptr) = b >> 5;
                A_VAL(dptr) = a >> 5;
                dptr++;
            }
        }
    }
}

/* scale by area sampling - IGNORE the ALPHA byte*/
static void
__imlib_ScaleAARGB(ImlibScaleInfo *isi, uint32_t *srce, uint32_t *dest,
                   int dxx, int dyy, int dx, int dy, int dw, int dh, int dow,
                   int sow)
{
    uint32_t       *sptr, *dptr;
    int             x, y, end;
    int            *ypoints;
    int            *xpoints;
    int            *xapoints;
    int            *yapoints;

#ifdef DO_MMX_ASM
    if (__imlib_do_asm())
    {
        __imlib_Scale_mmx_AARGBA(isi, dest, dxx, dyy, dx, dy, dw, dh, dow, sow);
        return;
    }
#endif

    ypoints = isi->ypoints;
    xpoints = isi->xpoints;
    xapoints = isi->xapoints;
    yapoints = isi->yapoints;

    end = dxx + dw;
    if (isi->xup_yup == 3)
    {
        /* Scaling up both ways */

        for (y = 0; y < dh; y++)
        {
            /* calculate the source line we'll scan from */
            sptr = srce + ypoints[dyy + y] * sow;
            dptr = dest + dx + (y + dy) * dow;
            if (YAP > 0)
            {
                for (x = dxx; x < end; x++)
                {
                    int             r, g, b;
                    int             rr, gg, bb;
                    uint32_t       *pix;

                    pix = sptr + xpoints[x];
                    if (XAP > 0)
                    {
                        r = R_VAL(pix) * INV_XAP;
                        g = G_VAL(pix) * INV_XAP;
                        b = B_VAL(pix) * INV_XAP;
                        pix++;
                        r += R_VAL(pix) * XAP;
                        g += G_VAL(pix) * XAP;
                        b += B_VAL(pix) * XAP;
                        pix += sow;
                        rr = R_VAL(pix) * XAP;
                        gg = G_VAL(pix) * XAP;
                        bb = B_VAL(pix) * XAP;
                        pix--;
                        rr += R_VAL(pix) * INV_XAP;
                        gg += G_VAL(pix) * INV_XAP;
                        bb += B_VAL(pix) * INV_XAP;
                        r = ((rr * YAP) + (r * INV_YAP)) >> 16;
                        g = ((gg * YAP) + (g * INV_YAP)) >> 16;
                        b = ((bb * YAP) + (b * INV_YAP)) >> 16;
                        *dptr++ = PIXEL_ARGB(0xff, r, g, b);
                    }
                    else
                    {
                        r = R_VAL(pix) * INV_YAP;
                        g = G_VAL(pix) * INV_YAP;
                        b = B_VAL(pix) * INV_YAP;
                        pix += sow;
                        r += R_VAL(pix) * YAP;
                        g += G_VAL(pix) * YAP;
                        b += B_VAL(pix) * YAP;
                        r >>= 8;
                        g >>= 8;
                        b >>= 8;
                        *dptr++ = PIXEL_ARGB(0xff, r, g, b);
                    }
                }
            }
            else
            {
                for (x = dxx; x < end; x++)
                {
                    int             r = 0, g = 0, b = 0;
                    uint32_t       *pix;

                    pix = sptr + xpoints[x];
                    if (XAP > 0)
                    {
                        r = R_VAL(pix) * INV_XAP;
                        g = G_VAL(pix) * INV_XAP;
                        b = B_VAL(pix) * INV_XAP;
                        pix++;
                        r += R_VAL(pix) * XAP;
                        g += G_VAL(pix) * XAP;
                        b += B_VAL(pix) * XAP;
                        r >>= 8;
                        g >>= 8;
                        b >>= 8;
                        *dptr++ = PIXEL_ARGB(0xff, r, g, b);
                    }
                    else
                    {
                        *dptr++ = *pix;
                    }
                }
            }
        }
    }
    else if (isi->xup_yup == 1)
    {
        /* Scaling down vertically */

        int             Cy, j;
        uint32_t       *pix;
        int             r, g, b, rr, gg, bb;
        int             yap;

        for (y = 0; y < dh; y++)
        {
            Cy = YAP >> 16;
            yap = YAP & 0xffff;

            sptr = srce + ypoints[dyy + y] * sow;
            dptr = dest + dx + (y + dy) * dow;
            for (x = dxx; x < end; x++)
            {
                pix = sptr + xpoints[x];
                r = (R_VAL(pix) * yap) >> 10;
                g = (G_VAL(pix) * yap) >> 10;
                b = (B_VAL(pix) * yap) >> 10;
                for (j = (1 << 14) - yap; j > Cy; j -= Cy)
                {
                    pix += sow;
                    r += (R_VAL(pix) * Cy) >> 10;
                    g += (G_VAL(pix) * Cy) >> 10;
                    b += (B_VAL(pix) * Cy) >> 10;
                }
                if (j > 0)
                {
                    pix += sow;
                    r += (R_VAL(pix) * j) >> 10;
                    g += (G_VAL(pix) * j) >> 10;
                    b += (B_VAL(pix) * j) >> 10;
                }
                if (XAP > 0)
                {
                    pix = sptr + xpoints[x] + 1;
                    rr = (R_VAL(pix) * yap) >> 10;
                    gg = (G_VAL(pix) * yap) >> 10;
                    bb = (B_VAL(pix) * yap) >> 10;
                    for (j = (1 << 14) - yap; j > Cy; j -= Cy)
                    {
                        pix += sow;
                        rr += (R_VAL(pix) * Cy) >> 10;
                        gg += (G_VAL(pix) * Cy) >> 10;
                        bb += (B_VAL(pix) * Cy) >> 10;
                    }
                    if (j > 0)
                    {
                        pix += sow;
                        rr += (R_VAL(pix) * j) >> 10;
                        gg += (G_VAL(pix) * j) >> 10;
                        bb += (B_VAL(pix) * j) >> 10;
                    }
                    r = r * INV_XAP;
                    g = g * INV_XAP;
                    b = b * INV_XAP;
                    r = (r + ((rr * XAP))) >> 12;
                    g = (g + ((gg * XAP))) >> 12;
                    b = (b + ((bb * XAP))) >> 12;
                }
                else
                {
                    r >>= 4;
                    g >>= 4;
                    b >>= 4;
                }
                *dptr++ = PIXEL_ARGB(0xff, r, g, b);
            }
        }
    }
    else if (isi->xup_yup == 2)
    {
        /* Scaling down horizontally */

        int             Cx, j;
        uint32_t       *pix;
        int             r, g, b, rr, gg, bb;
        int             xap;

        for (y = 0; y < dh; y++)
        {
            sptr = srce + ypoints[dyy + y] * sow;
            dptr = dest + dx + (y + dy) * dow;
            for (x = dxx; x < end; x++)
            {
                Cx = XAP >> 16;
                xap = XAP & 0xffff;

                pix = sptr + xpoints[x];
                r = (R_VAL(pix) * xap) >> 10;
                g = (G_VAL(pix) * xap) >> 10;
                b = (B_VAL(pix) * xap) >> 10;
                for (j = (1 << 14) - xap; j > Cx; j -= Cx)
                {
                    pix++;
                    r += (R_VAL(pix) * Cx) >> 10;
                    g += (G_VAL(pix) * Cx) >> 10;
                    b += (B_VAL(pix) * Cx) >> 10;
                }
                if (j > 0)
                {
                    pix++;
                    r += (R_VAL(pix) * j) >> 10;
                    g += (G_VAL(pix) * j) >> 10;
                    b += (B_VAL(pix) * j) >> 10;
                }
                if (YAP > 0)
                {
                    pix = sptr + xpoints[x] + sow;
                    rr = (R_VAL(pix) * xap) >> 10;
                    gg = (G_VAL(pix) * xap) >> 10;
                    bb = (B_VAL(pix) * xap) >> 10;
                    for (j = (1 << 14) - xap; j > Cx; j -= Cx)
                    {
                        pix++;
                        rr += (R_VAL(pix) * Cx) >> 10;
                        gg += (G_VAL(pix) * Cx) >> 10;
                        bb += (B_VAL(pix) * Cx) >> 10;
                    }
                    if (j > 0)
                    {
                        pix++;
                        rr += (R_VAL(pix) * j) >> 10;
                        gg += (G_VAL(pix) * j) >> 10;
                        bb += (B_VAL(pix) * j) >> 10;
                    }
                    r = r * INV_YAP;
                    g = g * INV_YAP;
                    b = b * INV_YAP;
                    r = (r + ((rr * YAP))) >> 12;
                    g = (g + ((gg * YAP))) >> 12;
                    b = (b + ((bb * YAP))) >> 12;
                }
                else
                {
                    r >>= 4;
                    g >>= 4;
                    b >>= 4;
                }
                *dptr++ = PIXEL_ARGB(0xff, r, g, b);
            }
        }
    }
    else
    {
        /* Scaling down horizontally & vertically */

        int             Cx, Cy, i, j;
        uint32_t       *pix;
        int             r, g, b, rx, gx, bx;
        int             xap, yap;

        for (y = 0; y < dh; y++)
        {
            Cy = YAP >> 16;
            yap = YAP & 0xffff;

            dptr = dest + dx + (y + dy) * dow;
            for (x = dxx; x < end; x++)
            {
                Cx = XAP >> 16;
                xap = XAP & 0xffff;

                sptr = srce + ypoints[dyy + y] * sow + xpoints[x];
                pix = sptr;
                sptr += sow;
                rx = (R_VAL(pix) * xap) >> 9;
                gx = (G_VAL(pix) * xap) >> 9;
                bx = (B_VAL(pix) * xap) >> 9;
                for (i = (1 << 14) - xap; i > Cx; i -= Cx)
                {
                    pix++;
                    rx += (R_VAL(pix) * Cx) >> 9;
                    gx += (G_VAL(pix) * Cx) >> 9;
                    bx += (B_VAL(pix) * Cx) >> 9;
                }
                if (i > 0)
                {
                    pix++;
                    rx += (R_VAL(pix) * i) >> 9;
                    gx += (G_VAL(pix) * i) >> 9;
                    bx += (B_VAL(pix) * i) >> 9;
                }

                r = (rx * yap) >> 14;
                g = (gx * yap) >> 14;
                b = (bx * yap) >> 14;

                for (j = (1 << 14) - yap; j > Cy; j -= Cy)
                {
                    pix = sptr;
                    sptr += sow;
                    rx = (R_VAL(pix) * xap) >> 9;
                    gx = (G_VAL(pix) * xap) >> 9;
                    bx = (B_VAL(pix) * xap) >> 9;
                    for (i = (1 << 14) - xap; i > Cx; i -= Cx)
                    {
                        pix++;
                        rx += (R_VAL(pix) * Cx) >> 9;
                        gx += (G_VAL(pix) * Cx) >> 9;
                        bx += (B_VAL(pix) * Cx) >> 9;
                    }
                    if (i > 0)
                    {
                        pix++;
                        rx += (R_VAL(pix) * i) >> 9;
                        gx += (G_VAL(pix) * i) >> 9;
                        bx += (B_VAL(pix) * i) >> 9;
                    }

                    r += (rx * Cy) >> 14;
                    g += (gx * Cy) >> 14;
                    b += (bx * Cy) >> 14;
                }
                if (j > 0)
                {
                    pix = sptr;
                    rx = (R_VAL(pix) * xap) >> 9;
                    gx = (G_VAL(pix) * xap) >> 9;
                    bx = (B_VAL(pix) * xap) >> 9;
                    for (i = (1 << 14) - xap; i > Cx; i -= Cx)
                    {
                        pix++;
                        rx += (R_VAL(pix) * Cx) >> 9;
                        gx += (G_VAL(pix) * Cx) >> 9;
                        bx += (B_VAL(pix) * Cx) >> 9;
                    }
                    if (i > 0)
                    {
                        pix++;
                        rx += (R_VAL(pix) * i) >> 9;
                        gx += (G_VAL(pix) * i) >> 9;
                        bx += (B_VAL(pix) * i) >> 9;
                    }

                    r += (rx * j) >> 14;
                    g += (gx * j) >> 14;
                    b += (bx * j) >> 14;
                }

                R_VAL(dptr) = r >> 5;
                G_VAL(dptr) = g >> 5;
                B_VAL(dptr) = b >> 5;
                dptr++;
            }
        }
    }
}

void
__imlib_Scale(ImlibScaleInfo *isi, bool aa, bool alpha,
              uint32_t *srce, uint32_t *dest, int dxx, int dyy, int dx,
              int dy, int dw, int dh, int dow, int sow)
{
    if (aa)
    {
        if (alpha)
            __imlib_ScaleAARGBA(isi, srce, dest, dxx, dyy, dx, dy, dw, dh,
                                dow, sow);
        else
            __imlib_ScaleAARGB(isi, srce, dest, dxx, dyy, dx, dy, dw, dh,
                               dow, sow);
    }
    else
        __imlib_ScaleSampleRGBA(isi, srce, dest, dxx, dyy, dx, dy, dw, dh,
                                dow, sow);
}
