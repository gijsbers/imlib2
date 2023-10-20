#include <gtest/gtest.h>

#include <Imlib2.h>
#include <zlib.h>

#include "config.h"
#include "test.h"

#define FILE_REF1	"icon-64"       // RGB
#define FILE_REF2	"xeyes" // ARGB (shaped)

typedef struct {
   double              ang;
   unsigned int        crc[4];
} tv_t;

typedef struct {
   const char         *file;
   const tv_t         *tv;      // Test values
} td_t;

/**INDENT-OFF**/
static const tv_t tv1[] = {
//                  No MMX                   MMX
//              -aa         +aa         -aa         +aa
  {   0., { 1846436624, 3612624604, 1846436624,  536176561 }},
  {  45., { 1979789244, 1737563695, 1979789244, 3607723297 }},
  { -45., { 2353730795, 1790877659, 2353730795, 1363960023 }},
};
static const tv_t tv2[] = {
//                  No MMX                   MMX
//              -aa         +aa         -aa         +aa
  {   0., { 3781676802, 1917972659, 3781676802, 1382339688 }},
  {  45., { 1660877013, 3746858440, 1660877013, 2023517531 }},
  { -45., { 1613585557, 2899764477, 1613585557, 4239058833 }},
};
#define N_VAL (sizeof(tv1) / sizeof(tv_t))

static const td_t   td[] = {
   { FILE_REF1, tv1 },
   { FILE_REF2, tv2 },
};
/**INDENT-ON**/

static void
test_rotate(int no, int aa)
{
   const td_t         *ptd;
   char                filei[256];
   char                fileo[256];

// int                 wi, hi;
   int                 wo, ho;
   unsigned int        i, ic, crc;
   Imlib_Image         imi, imo;
   Imlib_Load_Error    lerr;
   unsigned char      *data;

   ptd = &td[no];

   ic = aa;                     // CRC index
#ifdef DO_MMX_ASM
   // Hmm.. MMX functions appear to produce a slightly different result
   if (!getenv("IMLIB2_ASM_OFF"))
      ic += 2;
#endif

   snprintf(filei, sizeof(filei), "%s/%s.png", IMG_SRC, ptd->file);
   D("Load '%s'\n", filei);
   imi = imlib_load_image(filei);
   ASSERT_TRUE(imi);

   imlib_context_set_anti_alias(aa);

   for (i = 0; i < N_VAL; i++)
     {
        imlib_context_set_image(imi);
        //wi = imlib_image_get_width();
        //hi = imlib_image_get_height();

        D("Rotate %f\n", ptd->tv[i].ang);
        imo = imlib_create_rotated_image(ptd->tv[i].ang);
        ASSERT_TRUE(imo);

        imlib_context_set_image(imo);

        wo = imlib_image_get_width();
        ho = imlib_image_get_height();

        snprintf(fileo, sizeof(fileo), "%s/rotate-%s-%dx%d-%d.%s",
                 IMG_GEN, ptd->file, wo, ho, i, "png");
        imlib_image_set_format("png");
        D("Save '%s'\n", fileo);
        imlib_save_image_with_error_return(fileo, &lerr);
        EXPECT_EQ(lerr, 0);
        if (lerr)
           D("Error %d saving '%s'\n", lerr, fileo);

        data = (unsigned char *)imlib_image_get_data_for_reading_only();
        crc = crc32(0, data, wo * ho * sizeof(DATA32));
        EXPECT_EQ(crc, ptd->tv[i].crc[ic]);

        imlib_context_set_image(imo);
        imlib_free_image_and_decache();
     }

   imlib_context_set_image(imi);
   imlib_free_image_and_decache();
}

TEST(ROTAT, rotate_1_aa)
{
   test_rotate(0, 1);
}

TEST(ROTAT, rotate_1_noaa)
{
   test_rotate(0, 0);
}

TEST(ROTAT, rotate_2_aa)
{
   test_rotate(1, 1);
}

TEST(ROTAT, rotate_2_noaa)
{
   test_rotate(1, 0);
}
