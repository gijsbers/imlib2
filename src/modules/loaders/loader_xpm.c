#include "config.h"
#include "Imlib2_Loader.h"

#include <stdbool.h>

#define DBG_PFX "LDR-xpm"

static const char *const _formats[] = { "xpm" };

static struct {
    const char     *data, *dptr;
    unsigned int    size;
} mdata;

static void
mm_init(const void *src, unsigned int size)
{
    mdata.data = mdata.dptr = src;
    mdata.size = size;
}

static int
mm_getc(void)
{
    unsigned char   ch;

    if (mdata.dptr + 1 > mdata.data + mdata.size)
        return -1;              /* Out of data */

    ch = *mdata.dptr++;

    return ch;
}

static FILE    *rgb_txt = NULL;

static          uint32_t
xpm_parse_color(const char *color)
{
    char            buf[256];
    int             a, r, g, b;

    a = 0xff;
    r = g = b = 0;

    /* is a #ff00ff like color */
    if (color[0] == '#')
    {
        int             len;

        len = strlen(color) - 1;
        if (len < 96)
        {
            int             i;

            len /= 3;
            for (i = 0; i < len; i++)
                buf[i] = color[1 + i + (0 * len)];
            buf[i] = '\0';
            sscanf(buf, "%x", &r);
            for (i = 0; i < len; i++)
                buf[i] = color[1 + i + (1 * len)];
            buf[i] = '\0';
            sscanf(buf, "%x", &g);
            for (i = 0; i < len; i++)
                buf[i] = color[1 + i + (2 * len)];
            buf[i] = '\0';
            sscanf(buf, "%x", &b);
            if (len == 1)
            {
                r = (r << 4) | r;
                g = (g << 4) | g;
                b = (b << 4) | b;
            }
            else if (len > 2)
            {
                r >>= (len - 2) * 4;
                g >>= (len - 2) * 4;
                b >>= (len - 2) * 4;
            }
        }
        goto done;
    }

    if (!strcasecmp(color, "none"))
    {
        a = 0;
        goto done;
    }

    /* look in rgb txt database */
    if (!rgb_txt)
        rgb_txt = fopen(PACKAGE_DATA_DIR "/rgb.txt", "r");
    if (!rgb_txt)
        rgb_txt = fopen("/usr/share/X11/rgb.txt", "r");
    if (!rgb_txt)
        goto done;

    fseek(rgb_txt, 0, SEEK_SET);
    while (fgets(buf, sizeof(buf), rgb_txt))
    {
        if (buf[0] != '!')
        {
            int             rr, gg, bb;
            char            name[256];

            sscanf(buf, "%i %i %i %[^\n]", &rr, &gg, &bb, name);
            if (!strcasecmp(name, color))
            {
                r = rr;
                g = gg;
                b = bb;
                goto done;
            }
        }
    }

  done:
    return PIXEL_ARGB(a, r, g, b);
}

static void
xpm_parse_done(void)
{
    if (rgb_txt)
        fclose(rgb_txt);
    rgb_txt = NULL;
}

typedef struct {
    char            assigned;
    unsigned char   transp;
    char            str[6];
    uint32_t        pixel;
} cmap_t;

static int
xpm_cmap_sort(const void *a, const void *b)
{
    return strcmp(((const cmap_t *)a)->str, ((const cmap_t *)b)->str);
}

static          uint32_t
xpm_cmap_lookup(const cmap_t *cmap, int nc, int cpp, const char *s)
{
    int             i, i1, i2, x;

    i1 = 0;
    i2 = nc - 1;
    while (i1 < i2)
    {
        i = (i1 + i2) / 2;
        x = memcmp(s, cmap[i].str, cpp);
        if (x == 0)
            i1 = i2 = i;
        else if (x < 0)
            i2 = i - 1;
        else
            i1 = i + 1;
    }
    return cmap[i1].pixel;
}

static int
xpm_parse_cmap_line(const char *line, int len, int cpp, cmap_t *cme)
{
    char            s[256], tag[256], col[256];
    int             i, nr;
    bool            is_tag, is_col, is_eol, hascolor;

    if (len < cpp)
        return -1;

    is_tag = is_col = is_eol = false;
    hascolor = false;
    tag[0] = '\0';
    col[0] = '\0';

    strncpy(cme->str, line, cpp);

    for (i = cpp; i < len;)
    {
        s[0] = '\0';
        nr = 0;
        sscanf(&line[i], "%255s %n", s, &nr);
        i += nr;
        is_tag = !strcmp(s, "c") || !strcmp(s, "m") || !strcmp(s, "s") ||
            !strcmp(s, "g4") || !strcmp(s, "g");
        is_eol = i >= len;

        if (!is_tag)
        {
            /* Not tag - append to value */
            if (col[0])
            {
                if (strlen(col) < (sizeof(col) - 2))
                    strcat(col, " ");
                else
                    return -1;
            }
            if (strlen(col) + strlen(s) < (sizeof(col) - 1))
                strcat(col, s);
        }

        if ((is_tag || is_eol) && col[0])
        {
            /* Next tag or end of line - process color */
            is_col = !strcmp(tag, "c");
            if ((is_col || !cme->assigned) && !hascolor)
            {
                cme->pixel = xpm_parse_color(col);
                cme->assigned = 1;
                cme->transp = cme->pixel == 0x00000000;
                if (is_col)
                    hascolor = true;
                DL(" Coltbl tag='%s' col='%s' hasc=%d tr=%d %08x\n",
                   tag, col, hascolor, cme->transp, cme->pixel);
            }

            /* Starting new tag */
            strcpy(tag, s);
            col[0] = '\0';
        }
    }

    return 0;
}

static int
_load(ImlibImage *im, int load_data)
{
    int             rc, err;
    uint32_t       *ptr;
    int             pc, c, i, j, w, h, ncolors, cpp;
    int             context, len;
    char           *line;
    int             lsz = 256;
    cmap_t         *cmap;
    short           lookup[128 - 32][128 - 32];
    int             count, pixels;
    int             last_row = 0;
    bool            comment, quote, backslash, transp;

    rc = LOAD_FAIL;
    line = NULL;
    cmap = NULL;

    if (!memmem(im->fi->fdata,
                im->fi->fsize <= 256 ? im->fi->fsize : 256, " XPM */", 7))
        goto quit;

    rc = LOAD_BADIMAGE;         /* Format accepted */

    mm_init(im->fi->fdata, im->fi->fsize);

    j = 0;
    w = 10;
    h = 10;
    transp = false;
    ncolors = 0;
    cpp = 0;
    ptr = NULL;
    c = ' ';
    comment = quote = backslash = false;
    context = 0;
    pixels = 0;
    count = 0;
    line = malloc(lsz);
    if (!line)
        QUIT_WITH_RC(LOAD_OOM);
    len = 0;

    memset(lookup, 0, sizeof(lookup));
    for (;;)
    {
        pc = c;
        c = mm_getc();
        if (c < 0)
            break;

        if (!quote)
        {
            if ((pc == '/') && (c == '*'))
                comment = true;
            else if ((pc == '*') && (c == '/') && (comment))
                comment = false;
        }

        if (comment)
            continue;

        /* Scan in line from XPM file */
        if (!quote)
        {
            /* Waiting for start quote */
            if (c != '"')
                continue;
            /* Got start quote */
            quote = true;
            len = 0;
            continue;
        }

        if (c != '"')
        {
            /* Waiting for end quote */
            if (c < 32)
                c = 32;
            else if (c > 127)
                c = 127;

            if (c == '\\')
            {
                if (!backslash)
                {
                    line[len++] = c;
                    backslash = true;
                }
                else
                {
                    backslash = false;
                }
            }
            else
            {
                line[len++] = c;
                backslash = false;
            }

            if (len >= lsz)
            {
                char           *nline;

                lsz += 256;
                nline = realloc(line, lsz);
                if (!nline)
                    QUIT_WITH_RC(LOAD_OOM);
                line = nline;
            }
            continue;
        }

        /* Got end quote */
        line[len] = '\0';
        quote = false;

        if (context == 0)
        {
            /* Header */
            DL("Header line: '%s'\n", line);
            sscanf(line, "%i %i %i %i", &w, &h, &ncolors, &cpp);
            D("Header: WxH=%dx%d ncol=%d cpp=%d\n", w, h, ncolors, cpp);
            if ((ncolors > 32766) || (ncolors < 1))
            {
                E("XPM files with colors > 32766 or < 1 not supported\n");
                goto quit;
            }
            if ((cpp > 5) || (cpp < 1))
            {
                E("XPM files with characters per pixel > 5 or < 1 not supported\n");
                goto quit;
            }
            if (!IMAGE_DIMENSIONS_OK(w, h))
            {
                E("Invalid image dimension: %dx%d\n", w, h);
                goto quit;
            }
            im->w = w;
            im->h = h;

            cmap = calloc(ncolors, sizeof(cmap_t));
            if (!cmap)
                QUIT_WITH_RC(LOAD_OOM);

            pixels = w * h;

            j = 0;
            context = 1;
        }
        else if (context == 1)
        {
            /* Color Table */
            DL("Coltbl line: '%s'\n", line);

            err = xpm_parse_cmap_line(line, len, cpp, &cmap[j]);
            if (err)
                goto quit;

            if (cmap[j].transp)
                transp = true;

            j++;
            if (j < ncolors)
                continue;

            /* Got all colors */
#define LU(c0, c1) lookup[(int)(c0 - ' ')][(int)(c1 - ' ')]

            if (cpp == 1)
                for (i = 0; i < ncolors; i++)
                    LU(cmap[i].str[0], ' ') = i;
            else if (cpp == 2)
                for (i = 0; i < ncolors; i++)
                    LU(cmap[i].str[0], cmap[i].str[1]) = i;
            else
                qsort(cmap, ncolors, sizeof(cmap_t), xpm_cmap_sort);

            im->has_alpha = transp;

            if (!load_data)
                QUIT_WITH_RC(LOAD_SUCCESS);

            ptr = __imlib_AllocateData(im);
            if (!ptr)
                QUIT_WITH_RC(LOAD_OOM);

            context = 2;
        }
        else
        {
            /* Image Data */
            DL("Data   line: '%s'\n", line);

            if (cpp == 1)
            {
                for (i = 0; count < pixels && i <= len - cpp; i += cpp, count++)
                    *ptr++ = cmap[LU(line[i], ' ')].pixel;
            }
            else if (cpp == 2)
            {
                for (i = 0; count < pixels && i <= len - cpp; i += cpp, count++)
                    *ptr++ = cmap[LU(line[i], line[i + 1])].pixel;
            }
            else
            {
                for (i = 0; count < pixels && i <= len - cpp; i += cpp, count++)
                    *ptr++ = xpm_cmap_lookup(cmap, ncolors, cpp, &line[i]);
            }

            i = count / w;
            if (im->lc && i > last_row)
            {
                if (__imlib_LoadProgressRows(im, last_row, i - last_row))
                    QUIT_WITH_RC(LOAD_BREAK);

                last_row = i;
            }

            if (count >= pixels)
                break;
        }
    }

    if (!im->data || !cmap)
        goto quit;

    for (; count < pixels; count++)
    {
        /* Fill in missing pixels
         * (avoid working with uninitialized data in bad xpms) */
        im->data[count] = cmap[0].pixel;
    }

    rc = LOAD_SUCCESS;

  quit:
    free(cmap);
    free(line);

    xpm_parse_done();

    return rc;
}

IMLIB_LOADER(_formats, _load, NULL);
