#include <gtest/gtest.h>

#include <Imlib2.h>
#include <fcntl.h>

int                 debug = 0;

#define D(...)  if (debug) printf(__VA_ARGS__)
#define D2(...) if (debug > 1) printf(__VA_ARGS__)

#define EXPECT_OK(x)  EXPECT_FALSE(x)
#define EXPECT_ERR(x) EXPECT_TRUE(x)

#define TOPDIR  	SRC_DIR
#define IMGDIR		TOPDIR "/test/images"
#define FILE_REF	"icon-64.png"

static const char  *const pfxs[] = {
   "argb",
   "bmp",
   "ff.bz2",                    // bz2
   "ff",
   "gif",
   "ico",
   "jpg.mp3",                   // id3
   "jpeg",
   "ilbm",                      // lbm
   "png",
   "pbm",                       // pnm
   "ppm",                       // pnm
   "tga",
   "tiff",
   "webp",
   "xbm",
   "xpm",
   "ff.gz",                     // zlib
};
#define N_PFX (sizeof(pfxs) / sizeof(char*))

static int
progress(Imlib_Image im, char percent, int update_x, int update_y,
         int update_w, int update_h)
{
   D2("%s: %3d%% %4d,%4d %4dx%4d\n",
      __func__, percent, update_x, update_y, update_w, update_h);

   return 1;                    /* Continue */
}

static void
image_free(Imlib_Image im)
{
   imlib_context_set_image(im);
   imlib_free_image_and_decache();
}

static void
test_load(void)
{
   char                filei[256];
   char                fileo[256];
   unsigned int        i, j;
   Imlib_Image         im;
   Imlib_Load_Error    lerr;
   FILE               *fp;
   int                 fd;
   int                 err;

   snprintf(filei, sizeof(filei), "%s/%s", IMGDIR, FILE_REF);
   D("Load '%s'\n", filei);
   im = imlib_load_image(filei);

   ASSERT_TRUE(im);

   image_free(im);

   for (i = 0; i < N_PFX; i++)
     {
        // Load files of all types
        snprintf(fileo, sizeof(fileo), "%s/%s.%s", IMGDIR, "icon-64", pfxs[i]);
        D("Load '%s'\n", fileo);
        im = imlib_load_image_with_error_return(fileo, &lerr);
        EXPECT_TRUE(im);
        EXPECT_EQ(lerr, 0);
        if (!im || lerr)
           D("Error %d im=%p loading '%s'\n", lerr, im, fileo);
        if (im)
           image_free(im);

        imlib_flush_loaders();

        if (strchr(pfxs[i], '.') == 0)
          {
             snprintf(filei, sizeof(filei), "%s.%s", "icon-64", pfxs[i]);
             for (j = 0; j < N_PFX; j++)
               {
                  // Load certain types pretending they are something else
                  snprintf(fileo, sizeof(fileo), "%s/%s.%s.%s", IMGDIR,
                           "icon-64", pfxs[i], pfxs[j]);
                  unlink(fileo);
                  symlink(filei, fileo);
                  D("Load incorrect suffix '%s'\n", fileo);
                  im = imlib_load_image_with_error_return(fileo, &lerr);
                  EXPECT_TRUE(im);
                  EXPECT_EQ(lerr, 0);
                  if (!im || lerr)
                     D("Error %d im=%p loading '%s'\n", lerr, im, fileo);
                  if (im)
                     image_free(im);
               }
          }

        // Empty files of all types
        snprintf(fileo, sizeof(fileo), "%s/%s.%s", IMGDIR, "empty", pfxs[i]);
        unlink(fileo);
        fp = fopen(fileo, "wb");
        fclose(fp);
        D("Load empty '%s'\n", fileo);
        im = imlib_load_image_with_error_return(fileo, &lerr);
        D("  err = %d\n", lerr);
        EXPECT_TRUE(lerr == IMLIB_LOAD_ERROR_UNKNOWN);

        // Non-existing files of all types
        snprintf(fileo, sizeof(fileo), "%s/%s.%s", IMGDIR, "nonex", pfxs[i]);
        unlink(fileo);
        symlink("non-existing", fileo);
        D("Load non-existing '%s'\n", fileo);
        im = imlib_load_image_with_error_return(fileo, &lerr);
        EXPECT_EQ(lerr, IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST);

        // Load via fd
        snprintf(fileo, sizeof(fileo), "%s/%s.%s", IMGDIR, "icon-64", pfxs[i]);
        fd = open(fileo, O_RDONLY);
        D("Load fd %d '%s'\n", fd, fileo);
        snprintf(fileo, sizeof(fileo), ".%s", pfxs[i]);
        im = imlib_load_image_fd(fd, pfxs[i]);
        EXPECT_TRUE(im);
        if (im)
           image_free(im);
        err = close(fd);
        EXPECT_TRUE(err != 0);
     }
}

TEST(LOAD, load_1)
{
   test_load();
}

TEST(LOAD, load_2)
{
   imlib_context_set_progress_function(progress);
   imlib_context_set_progress_granularity(10);
   test_load();
}

int
main(int argc, char **argv)
{
   const char         *s;

   ::testing::InitGoogleTest(&argc, argv);

   for (argc--, argv++; argc > 0; argc--, argv++)
     {
        s = argv[0];
        if (*s++ != '-')
           break;
        switch (*s)
          {
          case 'd':
             debug++;
             break;
          }
     }

   return RUN_ALL_TESTS();
}
