#include "common.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include <math.h>

#include "blend.h"
#include "font.h"
#include "image.h"
#include "rgbadraw.h"
#include "rotate.h"

extern FT_Library ft_lib;

Imlib_Font_Glyph *
__imlib_font_cache_glyph_get(ImlibFont *fn, FT_UInt index)
{
    Imlib_Font_Glyph *fg;
    char            key[6];
    FT_Error        error;

    key[0] = ((index) & 0x7f) + 1;
    key[1] = ((index >> 7) & 0x7f) + 1;
    key[2] = ((index >> 14) & 0x7f) + 1;
    key[3] = ((index >> 21) & 0x7f) + 1;
    key[4] = ((index >> 28) & 0x0f) + 1;
    key[5] = 0;

    fg = __imlib_hash_find(fn->glyphs, key);
    if (fg)
        return fg;

    error = FT_Load_Glyph(fn->ft.face, index, FT_LOAD_NO_BITMAP);
    if (error)
        return NULL;

    fg = malloc(sizeof(Imlib_Font_Glyph));
    if (!fg)
        return NULL;
    memset(fg, 0, sizeof(Imlib_Font_Glyph));

    error = FT_Get_Glyph(fn->ft.face->glyph, &(fg->glyph));
    if (error)
    {
        free(fg);
        return NULL;
    }
    if (fg->glyph->format != ft_glyph_format_bitmap)
    {
        error = FT_Glyph_To_Bitmap(&(fg->glyph), ft_render_mode_normal, 0, 1);
        if (error)
        {
            FT_Done_Glyph(fg->glyph);
            free(fg);
            return NULL;
        }
    }
    fg->glyph_out = (FT_BitmapGlyph) fg->glyph;

    fn->glyphs = __imlib_hash_add(fn->glyphs, key, fg);
    return fg;
}

void
__imlib_render_str(ImlibImage *im, ImlibFont *fn, int drx, int dry,
                   const char *text, uint32_t pixel, int dir, double angle,
                   int *retw, int *reth, int blur,
                   int *nextx, int *nexty, ImlibOp op, int clx, int cly,
                   int clw, int clh)
{
    int             w, h, ascent;
    ImlibImage     *im2;
    int             nx, ny;

    __imlib_font_query_advance(fn, text, &w, NULL);
    h = __imlib_font_max_ascent_get(fn) - __imlib_font_max_descent_get(fn);

    /* TODO check if this is the right way of rendering. Esp for huge sizes */
    im2 = __imlib_CreateImage(w, h, NULL, 1);
    if (!im2)
        return;

    im2->has_alpha = 1;

    ascent = __imlib_font_max_ascent_get(fn);

    nx = ny = 0;
    __imlib_font_draw(im2, pixel, fn, 0, ascent, text, &nx, &ny, 0, 0, w, h);

    /* OK, now we have small ImlibImage with text rendered, 
     * have to blend it on im */

    if (blur > 0)
        __imlib_BlurImage(im2, blur);

    switch (dir)
    {
    case 0:                    /* to right */
        angle = 0.0;
        break;
    case 1:                    /* to left */
        angle = 0.0;
        __imlib_FlipImageBoth(im2);
        break;
    case 2:                    /* to down */
        angle = 0.0;
        __imlib_FlipImageDiagonal(im2, 1);
        break;
    case 3:                    /* to up */
        angle = 0.0;
        __imlib_FlipImageDiagonal(im2, 2);
        break;
    default:
        break;
    }
    if (angle == 0.0)
    {
        __imlib_BlendImageToImage(im2, im, 0, 1,
                                  im->has_alpha, 0, 0,
                                  im2->w, im2->h, drx, dry, im2->w, im2->h,
                                  NULL, op, clx, cly, clw, clh);
    }
    else
    {
        int             xx, yy;
        double          sa, ca;

        sa = sin(angle);
        ca = cos(angle);
        xx = drx;
        yy = dry;
        if (sa > 0.0)
            xx += sa * im2->h;
        else
            yy -= sa * im2->w;
        if (ca < 0.0)
        {
            xx -= ca * im2->w;
            yy -= ca * im2->h;
        }
        __imlib_BlendImageToImageSkewed(im2, im, 1, 1,
                                        im->has_alpha, 0,
                                        0, im2->w, im2->h, xx, yy, (w * ca),
                                        (w * sa), 0, 0, NULL, op, clx, cly, clw,
                                        clh);
    }

    __imlib_FreeImage(im2);

    /* finally deal with return values */
    switch (dir)
    {
    case 0:
    case 1:
        if (retw)
            *retw = w;
        if (reth)
            *reth = h;
        if (nextx)
            *nextx = nx;
        if (nexty)
            *nexty = ny;
        break;
    case 2:
    case 3:
        if (retw)
            *retw = h;
        if (reth)
            *reth = w;
        if (nextx)
            *nextx = ny;
        if (nexty)
            *nexty = nx;
        break;
    case 4:
        {
            double          sa, ca;
            double          x1, x2, xt;
            double          y1, y2, yt;

            sa = sin(angle);
            ca = cos(angle);

            x1 = x2 = 0.0;
            xt = ca * w;
            if (xt < x1)
                x1 = xt;
            if (xt > x2)
                x2 = xt;
            xt = -(sa * h);
            if (xt < x1)
                x1 = xt;
            if (xt > x2)
                x2 = xt;
            xt = ca * w - sa * h;
            if (xt < x1)
                x1 = xt;
            if (xt > x2)
                x2 = xt;
            w = (int)(x2 - x1);

            y1 = y2 = 0.0;
            yt = sa * w;
            if (yt < y1)
                y1 = yt;
            if (yt > y2)
                y2 = yt;
            yt = ca * h;
            if (yt < y1)
                y1 = yt;
            if (yt > y2)
                y2 = yt;
            yt = sa * w + ca * h;
            if (yt < y1)
                y1 = yt;
            if (yt > y2)
                y2 = yt;
            h = (int)(y2 - y1);
        }
        if (retw)
            *retw = w;
        if (reth)
            *reth = h;
        if (nextx)
            *nextx = nx;
        if (nexty)
            *nexty = ny;
        break;
    default:
        break;
    }

    /* TODO this function is purely my art -- check once more */
}

void
__imlib_font_draw(ImlibImage *dst, uint32_t col, ImlibFont *fn, int x, int y,
                  const char *text, int *nextx, int *nexty, int clx, int cly,
                  int clw, int clh)
{
    int             pen_x, pen_y;
    int             chr;
    int             ext_x, ext_y, ext_w, ext_h;
    uint32_t       *im;
    int             im_w, im_h;
    int             lut[256];
    int             ii;

    im = dst->data;
    im_w = dst->w;
    im_h = dst->h;

    ext_x = 0;
    ext_y = 0;
    ext_w = im_w;
    ext_h = im_h;

    if (clw)
    {
        ext_x = clx;
        ext_y = cly;
        ext_w = clw;
        ext_h = clh;
    }
    if (ext_x < 0)
    {
        ext_w += ext_x;
        ext_x = 0;
    }
    if (ext_y < 0)
    {
        ext_h += ext_y;
        ext_y = 0;
    }
    if ((ext_x + ext_w) > im_w)
        ext_w = im_w - ext_x;
    if ((ext_y + ext_h) > im_h)
        ext_h = im_h - ext_y;

    if (ext_w <= 0)
        return;
    if (ext_h <= 0)
        return;

    for (ii = 0; ii < 256; ii++)
    {
        lut[ii] = (col & 0x00ffffff);   /* TODO check endianess */
        lut[ii] |= ((((ii + 1) * (col >> 24)) >> 8) << 24);
    }

    pen_x = x << 8;
    pen_y = y << 8;
    for (chr = 0; text[chr];)
    {
        FT_UInt         index;
        Imlib_Font_Glyph *fg;
        int             chr_x, chr_y, kern;

        fg = __imlib_font_get_next_glyph(fn, text, &chr, &index, &kern);
        if (!fg)
            break;
        pen_x += kern;
        if (fg == IMLIB_GLYPH_NONE)
            continue;

        chr_x = (pen_x + (fg->glyph_out->left << 8)) >> 8;
        chr_y = (pen_y + (fg->glyph_out->top << 8)) >> 8;

        if (chr_x < (ext_x + ext_w))
        {
            uint8_t        *data;
            int             i, j, w, h;

            data = fg->glyph_out->bitmap.buffer;
            j = fg->glyph_out->bitmap.pitch;
            w = fg->glyph_out->bitmap.width;
            if (j < w)
                j = w;
            h = fg->glyph_out->bitmap.rows;
            if ((fg->glyph_out->bitmap.pixel_mode == ft_pixel_mode_grays) &&
                (fg->glyph_out->bitmap.num_grays == 256))
            {
                if ((j > 0) && (chr_x + w > ext_x))
                {
                    for (i = 0; i < h; i++)
                    {
                        int             dx, dy;
                        int             in_x, in_w;

                        in_x = 0;
                        in_w = 0;
                        dx = chr_x;
                        dy = y - (chr_y - i - y);
                        if ((dx < (ext_x + ext_w)) && (dy >= (ext_y)) &&
                            (dy < (ext_y + ext_h)))
                        {
                            if (dx + w > (ext_x + ext_w))
                                in_w += (dx + w) - (ext_x + ext_w);
                            if (dx < ext_x)
                            {
                                in_w += ext_x - dx;
                                in_x = ext_x - dx;
                                dx = ext_x;
                            }
                            if (in_w < w)
                            {
                                uint8_t        *src_ptr;
                                uint32_t       *dst_ptr;
                                uint32_t       *dst_end_ptr;

                                src_ptr = data + (i * j) + in_x;
                                dst_ptr = im + (dy * im_w) + dx;
                                dst_end_ptr = dst_ptr + w - in_w;

                                while (dst_ptr < dst_end_ptr)
                                {
                                    /* FIXME Oops! change this op */
                                    if (!*dst_ptr)
                                        *dst_ptr = lut[(unsigned char)*src_ptr];
                                    else if (*src_ptr)
                                    {
                                        /* very rare case - I've never seen symbols 
                                         * overlapped by kerning */
                                        int             tmp;

                                        tmp =
                                            (*dst_ptr >> 24) +
                                            (lut
                                             [(unsigned char)*src_ptr] >> 24);
                                        tmp = (tmp > 256) ? 256 : tmp;
                                        *dst_ptr &= 0x00ffffff;
                                        *dst_ptr |= (tmp << 24);
                                    }

                                    dst_ptr++;
                                    src_ptr++;
                                }
                            }
                        }
                    }
                }
            }
        }
        else
            break;
        pen_x += fg->glyph->advance.x >> 8;
    }

    if (nextx)
        *nextx = (pen_x >> 8) - x;
    if (nexty)
        *nexty = __imlib_font_get_line_advance(fn);
}
