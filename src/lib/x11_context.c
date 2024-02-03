#include "common.h"

#include <stdlib.h>
#include <X11/Xlib.h>

#include "x11_color.h"
#include "x11_context.h"
#include "x11_rgba.h"

static Context *context = NULL;
static int      max_context_count = 128;
static int      context_counter = 0;

void
__imlib_SetMaxContexts(int num)
{
    max_context_count = num;
    __imlib_FlushContexts();
}

int
__imlib_GetMaxContexts(void)
{
    return max_context_count;
}

void
__imlib_FlushContexts(void)
{
    Context        *ct, *ct_prev, *ct_next;

    for (ct = context, ct_prev = NULL; ct; ct = ct_next)
    {
        ct_next = ct->next;

        /* it hasn't been referenced in the last max_context_count references */
        /* thus old and getrid of it */
        if (ct->last_use < (context_counter - max_context_count))
        {
            if (ct_prev)
                ct_prev->next = ct->next;
            else
                context = ct->next;
            if (ct->palette)
            {
                int             i, num[] = { 256, 128, 64, 32, 16, 8, 1 };
                unsigned long   pixels[256];

                for (i = 0; i < num[ct->palette_type]; i++)
                    pixels[i] = (unsigned long)ct->palette[i];
                XFreeColors(ct->x11.dpy, ct->x11.cmap, pixels,
                            num[ct->palette_type], 0);

                free(ct->palette);
                free(ct->r_dither);
                free(ct->g_dither);
                free(ct->b_dither);
            }
            else if (ct->r_dither)
            {
                free(ct->r_dither);
                free(ct->g_dither);
                free(ct->b_dither);
            }
            free(ct);
        }
        else
        {
            ct_prev = ct;
        }
    }
}

Context        *
__imlib_FindContext(const ImlibContextX11 *x11)
{
    Context        *ct, *ct_prev;

    for (ct = context, ct_prev = NULL; ct; ct_prev = ct, ct = ct->next)
    {
        if ((ct->x11.dpy == x11->dpy) && (ct->x11.vis == x11->vis) &&
            (ct->x11.cmap == x11->cmap) && (ct->x11.depth == x11->depth))
        {
            if (ct_prev)
            {
                ct_prev->next = ct->next;
                ct->next = context;
                context = ct;
            }
            return ct;
        }
    }
    return NULL;
}

Context        *
__imlib_NewContext(const ImlibContextX11 *x11)
{
    Context        *ct;

    context_counter++;
    ct = malloc(sizeof(Context));
    ct->last_use = context_counter;
    ct->x11 = *x11;
    ct->next = NULL;

    if (x11->depth <= 8)
    {
        ct->palette = __imlib_AllocColorTable(x11, &ct->palette_type);
        ct->r_dither = malloc(sizeof(uint8_t) * DM_X * DM_Y * 256);
        ct->g_dither = malloc(sizeof(uint8_t) * DM_X * DM_Y * 256);
        ct->b_dither = malloc(sizeof(uint8_t) * DM_X * DM_Y * 256);
        __imlib_RGBA_init((void *)ct->r_dither, (void *)ct->g_dither,
                          (void *)ct->b_dither, x11->depth, ct->palette_type);
    }
    else
    {
        ct->palette = NULL;
        ct->palette_type = 0;
        if ((x11->depth > 8) && (x11->depth <= 16))
        {
            ct->r_dither = malloc(sizeof(uint16_t) * 4 * 4 * 256);
            ct->g_dither = malloc(sizeof(uint16_t) * 4 * 4 * 256);
            ct->b_dither = malloc(sizeof(uint16_t) * 4 * 4 * 256);
            __imlib_RGBA_init((void *)ct->r_dither, (void *)ct->g_dither,
                              (void *)ct->b_dither, x11->depth, 0);
        }
        else
        {
            ct->r_dither = NULL;
            ct->g_dither = NULL;
            ct->b_dither = NULL;
            __imlib_RGBA_init((void *)ct->r_dither, (void *)ct->g_dither,
                              (void *)ct->b_dither, x11->depth, 0);
        }
    }

    return ct;
}

Context        *
__imlib_GetContext(const ImlibContextX11 *x11)
{
    Context        *ct;

    ct = __imlib_FindContext(x11);
    if (ct)
    {
        ct->last_use = context_counter;
        return ct;
    }

    __imlib_FlushContexts();

    ct = __imlib_NewContext(x11);
    ct->next = context;
    context = ct;

    return ct;
}
