#include <gtest/gtest.h>

#include <Imlib2.h>

#include "config.h"
#include "test.h"

#define EXPECT_OK(x)  EXPECT_FALSE(x)
#define EXPECT_ERR(x) EXPECT_TRUE(x)

#define FILE_REF1	"icon-64"       // RGB
#define FILE_REF2	"xeyes" // ARGB (shaped)

static const char  *const pfxs[] = {
   "argb",
   "bmp",
   "ff",
// "gif",
// "ico",
   "jpeg",
// "lbm",
   "png",
   "pnm",
   "tga",
   "tiff",
   "webp",
   "xbm",
// "xpm",
};
#define N_PFX (sizeof(pfxs) / sizeof(char*))

static int
progress(Imlib_Image im, char percent, int update_x, int update_y,
         int update_w, int update_h)
{
   D("%s: %3d%% %4d,%4d %4dx%4d\n",
     __func__, percent, update_x, update_y, update_w, update_h);

   return 1;                    /* Continue */
}

static void
test_save(const char *file, int prog)
{
   char                filei[256];
   char                fileo[256];
   unsigned int        i;
   int                 w, h;
   Imlib_Image         im, im1, im2, im3;
   Imlib_Load_Error    lerr;

   if (prog)
     {
        imlib_context_set_progress_function(progress);
        imlib_context_set_progress_granularity(10);
     }

   snprintf(filei, sizeof(filei), "%s/%s.png", IMG_SRC, file);
   D("Load '%s'\n", filei);
   im = imlib_load_image(filei);
   ASSERT_TRUE(im);
   if (!im)
     {
        printf("Error loading '%s'\n", filei);
        exit(0);
     }

   imlib_context_set_image(im);
   w = imlib_image_get_width();
   h = imlib_image_get_height();
   im1 = imlib_create_cropped_scaled_image(0, 0, w, h, w + 1, h + 1);
   im2 = imlib_create_cropped_scaled_image(0, 0, w, h, w + 2, h + 2);
   im3 = imlib_create_cropped_scaled_image(0, 0, w, h, w + 3, h + 3);

   ASSERT_TRUE(im1);
   ASSERT_TRUE(im2);
   ASSERT_TRUE(im3);

   for (i = 0; i < N_PFX; i++)
     {
        imlib_context_set_image(im);
        imlib_image_set_format(pfxs[i]);
        w = imlib_image_get_width();
        h = imlib_image_get_height();
        snprintf(fileo, sizeof(fileo), "%s/save-%s-%dx%d.%s",
                 IMG_GEN, file, w, h, pfxs[i]);
        D("Save '%s'\n", fileo);
        imlib_save_image_with_error_return(fileo, &lerr);
        EXPECT_EQ(lerr, 0);
        if (lerr)
           D("Error %d saving '%s'\n", lerr, fileo);

        imlib_context_set_image(im1);
        imlib_image_set_format(pfxs[i]);
        w = imlib_image_get_width();
        h = imlib_image_get_height();
        snprintf(fileo, sizeof(fileo), "%s/save-%s-%dx%d.%s",
                 IMG_GEN, file, w, h, pfxs[i]);
        D("Save '%s'\n", fileo);
        imlib_save_image_with_error_return(fileo, &lerr);
        EXPECT_EQ(lerr, 0);
        if (lerr)
           D("Error %d saving '%s'\n", lerr, fileo);

        imlib_context_set_image(im2);
        imlib_image_set_format(pfxs[i]);
        w = imlib_image_get_width();
        h = imlib_image_get_height();
        snprintf(fileo, sizeof(fileo), "%s/save-%s-%dx%d.%s",
                 IMG_GEN, file, w, h, pfxs[i]);
        D("Save '%s'\n", fileo);
        imlib_save_image_with_error_return(fileo, &lerr);
        EXPECT_EQ(lerr, 0);
        if (lerr)
           D("Error %d saving '%s'\n", lerr, fileo);

        imlib_context_set_image(im3);
        imlib_image_set_format(pfxs[i]);
        w = imlib_image_get_width();
        h = imlib_image_get_height();
        snprintf(fileo, sizeof(fileo), "%s/save-%s-%dx%d.%s",
                 IMG_GEN, file, w, h, pfxs[i]);
        D("Save '%s'\n", fileo);
        imlib_save_image_with_error_return(fileo, &lerr);
        EXPECT_EQ(lerr, 0);
        if (lerr)
           D("Error %d saving '%s'\n", lerr, fileo);
     }

   imlib_context_set_image(im);
   imlib_free_image_and_decache();
   imlib_context_set_image(im1);
   imlib_free_image_and_decache();
   imlib_context_set_image(im2);
   imlib_free_image_and_decache();
   imlib_context_set_image(im3);
   imlib_free_image_and_decache();
}

TEST(SAVE, save_1_rgb)
{
   imlib_context_set_progress_function(NULL);

   test_save(FILE_REF1, 0);
}

TEST(SAVE, save_2_rgb)
{
   test_save(FILE_REF1, 1);
}

TEST(SAVE, save_1_argb)
{
   imlib_context_set_progress_function(NULL);

   test_save(FILE_REF2, 0);
}

TEST(SAVE, save_2_argb)
{
   test_save(FILE_REF2, 1);
}
