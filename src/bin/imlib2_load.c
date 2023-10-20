#include "config.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if USE_MONOTONIC_CLOCK
#include <time.h>
#else
#include <sys/time.h>
#endif

#ifndef X_DISPLAY_MISSING
#define X_DISPLAY_MISSING
#endif
#include <Imlib2.h>

#define PROG_NAME "imlib2_load"

static char         progress_called;
static FILE        *fout;

#define Vprintf(fmt...)  if (verbose)      fprintf(fout, fmt)
#define V2printf(fmt...) if (verbose >= 2) fprintf(fout, fmt)

#define HELP \
   "Usage:\n" \
   "  imlib2_load [OPTIONS] FILE...\n" \
   "OPTIONS:\n" \
   "  -e  : Break on error\n" \
   "  -f  : Load with imlib_load_image_fd()\n" \
   "  -i  : Load with imlib_load_image_immediately()\n" \
   "  -n N: Reeat load N times\n" \
   "  -p  : Check that progress is called\n" \
   "  -v  : Increase verbosity\n" \
   "  -x  : Print to stderr\n"

static void
usage(void)
{
   printf(HELP);
}

static unsigned int
time_us(void)
{
#if USE_MONOTONIC_CLOCK
   struct timespec     ts;

   clock_gettime(CLOCK_MONOTONIC, &ts);

   return (unsigned int)(ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
#else
   struct timeval      timev;

   gettimeofday(&timev, NULL);

   return (unsigned int)(timev.tv_sec * 1000000 + timev.tv_usec);
#endif
}

static Imlib_Image *
image_load_fd(const char *file)
{
   Imlib_Image        *im;
   int                 fd;
   const char         *ext;

   ext = strchr(file, '.');
   if (ext)
      ext += 1;
   else
      ext = file;

   fd = open(file, O_RDONLY);
   im = imlib_load_image_fd(fd, ext);

   return im;
}

static int
progress(Imlib_Image im, char percent, int update_x, int update_y,
         int update_w, int update_h)
{
   progress_called = 1;
   return 1;                    /* Continue */
}

int
main(int argc, char **argv)
{
   int                 opt;
   Imlib_Image         im;
   Imlib_Load_Error    lerr;
   unsigned int        t0;
   char                nbuf[4096];
   int                 frame;
   int                 verbose;
   bool                check_progress;
   int                 break_on_error;
   bool                show_time;
   int                 load_cnt, cnt;
   bool                load_fd;
   bool                load_now;
   bool                load_nodata;

   fout = stdout;
   verbose = 0;
   check_progress = false;
   break_on_error = 0;
   show_time = false;
   load_cnt = 1;
   load_fd = false;
   load_now = false;
   load_nodata = false;

   while ((opt = getopt(argc, argv, "efijn:pvx")) != -1)
     {
        switch (opt)
          {
          case 'e':
             break_on_error += 1;
             break;
          case 'f':
             load_fd = true;
             break;
          case 'i':
             load_now = true;
             break;
          case 'j':
             load_nodata = true;
             break;
          case 'n':
             load_cnt = atoi(optarg);
             show_time = true;
             verbose = 1;
             break;
          case 'p':
             check_progress = true;
             verbose = 1;
             break;
          case 'v':
             verbose += 1;
             break;
          case 'x':
             fout = stderr;
             break;
          }
     }

   argc -= optind;
   argv += optind;

   if (argc <= 0)
     {
        usage();
        return 1;
     }

   if (check_progress)
     {
        imlib_context_set_progress_function(progress);
        imlib_context_set_progress_granularity(10);
     }

   t0 = 0;

   for (; argc > 0; argc--, argv++)
     {
        progress_called = 0;

        Vprintf("Loading image: '%s'\n", argv[0]);

        if (show_time)
           t0 = time_us();

        for (cnt = 0; cnt < load_cnt; cnt++)
          {
             lerr = 0;

             if (check_progress)
                im = imlib_load_image_with_error_return(argv[0], &lerr);
             else if (load_fd)
                im = image_load_fd(argv[0]);
             else if (load_now)
                im = imlib_load_image_immediately(argv[0]);
             else
               {
                  frame = -1;
                  sscanf(argv[0], "%[^%]%%%d", nbuf, &frame);

                  if (frame >= 0)
                     im = imlib_load_image_frame(nbuf, frame);
                  else
                     im = imlib_load_image(argv[0]);
               }

             if (!im)
               {
                  fprintf(fout, "*** Error %d loading image: %s\n",
                          lerr, argv[0]);
                  if (break_on_error & 2)
                     goto quit;
                  goto next;
               }

             imlib_context_set_image(im);
             V2printf("Image  WxH=%dx%d\n",
                      imlib_image_get_width(), imlib_image_get_height());

             if (!check_progress && !load_now && !load_nodata)
                imlib_image_get_data();

             imlib_free_image_and_decache();
          }

        if (show_time)
           printf("Elapsed time: %.3f ms\n", 1e-3 * (time_us() - t0));

        if (check_progress && !progress_called)
          {
             fprintf(fout, "*** No progress during image load\n");
             if (break_on_error & 1)
                goto quit;
          }
      next:
        ;
     }
 quit:

   return 0;
}
