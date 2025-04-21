/*
 * Convert images between formats, using Imlib2's API.
 * Defaults to jpg's.
 */
#include "config.h"
#ifndef X_DISPLAY_MISSING
#define X_DISPLAY_MISSING
#endif
#include <Imlib2.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "prog_util.h"

#define PIXEL_A(argb)  (((argb) >> 24) & 0xff)
#define PIXEL_R(argb)  (((argb) >> 16) & 0xff)
#define PIXEL_G(argb)  (((argb) >>  8) & 0xff)
#define PIXEL_B(argb)  (((argb)      ) & 0xff)

#define DEBUG 0
#if DEBUG
#define Dprintf(fmt...)  printf(fmt)
#else
#define Dprintf(fmt...)
#endif

#define HELP \
   "Usage:\n" \
   "  imlib2_conv [OPTIONS] [ input-file output-file[.fmt] ]\n" \
   "    <fmt> defaults to jpg if not provided.\n" \
   "\n" \
   "OPTIONS:\n" \
   "  -h            : Show this help\n" \
   "  -b 0xRRGGBB   : Render on solid background before saving\n" \
   "  -i key=value  : Attach tag with integer value for saver\n" \
   "  -j key=string : Attach tag with string value for saver\n" \
   "  -g WxH        : Specify output image size\n"

static void
usage(void)
{
    printf(HELP);
}

static void
data_free_cb(void *im, void *data)
{
    Dprintf("%s: im=%p data=%p\n", __func__, im, data);
    free(data);
}

/*
 * Attach tag = key/value pair to current image
 */
static void
data_attach(int type, char *arg)
{
    char           *p;

    p = strchr(arg, '=');
    if (!p)
        return;                 /* No value - just ignore */

    *p++ = '\0';

    switch (type)
    {
    default:
        break;                  /* Should not be possible - ignore */
    case 'i':                  /* Integer parameter */
        Dprintf("%s: Set '%s' = %d\n", __func__, arg, atoi(p));
        imlib_image_attach_data_value(arg, NULL, atoi(p), NULL);
        break;
    case 'j':                  /* String parameter */
        p = strdup(p);
        Dprintf("%s: Set '%s' = '%s' (%p)\n", __func__, arg, p, p);
        imlib_image_attach_data_value(arg, p, 0, data_free_cb);
        break;
    }
}

int
main(int argc, char **argv)
{
    int             opt, err;
    const char     *fin, *fout;
    int             wo, ho;
    unsigned int    bgcol;
    char           *dot;
    Imlib_Image     im;
    int             cnt, save_cnt;
    bool            show_time;
    unsigned int    t0;
    double          dt;

    wo = ho = 0;
    bgcol = 0x80000000;
    show_time = false;
    save_cnt = 1;
    t0 = 0;

    while ((opt = getopt(argc, argv, "b:hi:j:g:n:")) != -1)
    {
        switch (opt)
        {
        default:
        case 'h':
            usage();
            exit(0);
        case 'b':
            sscanf(optarg, "%x", &bgcol);
            break;
        case 'i':
        case 'j':
            break;              /* Ignore this time around */
        case 'g':
            sscanf(optarg, "%ux%u", &wo, &ho);
            break;
        case 'n':
            save_cnt = atoi(optarg);
            show_time = true;
            break;
        }
    }

    if (argc - optind < 2)
    {
        usage();
        exit(1);
    }

    fin = argv[optind];
    fout = argv[optind + 1];

    im = imlib_load_image_with_errno_return(fin, &err);
    if (!im)
    {
        fprintf(stderr, "*** Error %d:'%s' loading image: '%s'\n",
                err, imlib_strerror(err), fin);
        return 1;
    }

    Dprintf("%s: im=%p\n", __func__, im);
    imlib_context_set_image(im);

    if (wo != 0 || ho != 0)
    {
        Imlib_Image     im2;
        int             wi, hi;
        wi = imlib_image_get_width();
        hi = imlib_image_get_height();
        im2 = imlib_create_cropped_scaled_image(0, 0, wi, hi, wo, ho);
        if (!im2)
        {
            fprintf(stderr, "*** Error: Failed to scale image\n");
            return 1;
        }
#if DEBUG
        imlib_free_image_and_decache();
#endif
        imlib_context_set_image(im2);
        im = im2;
    }

    wo = imlib_image_get_width();
    ho = imlib_image_get_height();

    if (bgcol != 0x80000000)
    {
        Imlib_Image     im2;
        im2 = imlib_create_image(wo, ho);
        if (!im2)
        {
            fprintf(stderr, "*** Error: Failed to create background image\n");
            return 1;
        }
        imlib_context_set_image(im2);
        imlib_context_set_color(PIXEL_R(bgcol), PIXEL_G(bgcol), PIXEL_B(bgcol),
                                255);
        imlib_image_fill_rectangle(0, 0, wo, ho);
        imlib_blend_image_onto_image(im, 1, 0, 0, wo, wo, 0, 0, wo, wo);
    }

    /* Re-parse options to attach parameters to be used by savers */
    optind = 1;
    while ((opt = getopt(argc, argv, "b:hi:j:g:n:")) != -1)
    {
        switch (opt)
        {
        default:
            break;
        case 'i':              /* Attach integer parameter */
        case 'j':              /* Attach string parameter */
            data_attach(opt, optarg);
            break;
        }
    }

    /* hopefully the last one will be the one we want.. */
    dot = strrchr(fout, '.');

    /* if there's a format, snarf it and set the format. */
    if (dot && *(dot + 1))
        imlib_image_set_format(dot + 1);
    else
        imlib_image_set_format("jpg");

    if (show_time)
        t0 = time_us();

    for (cnt = 0; cnt < save_cnt; cnt++)
    {
        imlib_save_image_with_errno_return(fout, &err);
        if (err)
            fprintf(stderr, "*** Error %d:'%s' saving image: '%s'\n",
                    err, imlib_strerror(err), fout);
    }

    if (show_time)
    {
        dt = 1e-3 * (time_us() - t0);
        printf("Elapsed time: %.3f ms (%.3f ms per save)\n", dt, dt / save_cnt);
    }

#if DEBUG
    imlib_free_image_and_decache();
#endif

    return err;
}
