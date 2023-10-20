#include "loader_common.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <webp/decode.h>
#include <webp/encode.h>

static const char  *
webp_strerror(VP8StatusCode code)
{
   switch (code)
     {
     case VP8_STATUS_OK:
        return "No Error";
     case VP8_STATUS_OUT_OF_MEMORY:
        return "Out of memory";
     case VP8_STATUS_INVALID_PARAM:
        return "Invalid API parameter";
     case VP8_STATUS_BITSTREAM_ERROR:
        return "Bitstream Error";
     case VP8_STATUS_UNSUPPORTED_FEATURE:
        return "Unsupported Feature";
     case VP8_STATUS_SUSPENDED:
        return "Suspended";
     case VP8_STATUS_USER_ABORT:
        return "User abort";
     case VP8_STATUS_NOT_ENOUGH_DATA:
        return "Not enough data/truncated file";
     default:
        return "Unknown error";
     }
}

char
load(ImlibImage * im, ImlibProgressFunction progress,
     char progress_granularity, char immediate_load)
{
   uint8_t            *encoded_data;
   struct stat         stats;
   int                 encoded_fd;
   WebPBitstreamFeatures features;
   VP8StatusCode       vp8return;

   if (stat(im->real_file, &stats) < 0)
     {
        return 0;
     }

   encoded_fd = open(im->real_file, O_RDONLY);
   if (encoded_fd < 0)
     {
        return 0;
     }

   encoded_data = malloc(stats.st_size);

   if (encoded_data == NULL)
     {
        close(encoded_fd);
        return 0;
     }

   if (read(encoded_fd, encoded_data, stats.st_size) < stats.st_size)
     {
        free(encoded_data);
        close(encoded_fd);
        return 0;
     }
   close(encoded_fd);

   if (WebPGetInfo(encoded_data, stats.st_size, &im->w, &im->h) == 0)
     {
        free(encoded_data);
        return 0;
     }

   if (!IMAGE_DIMENSIONS_OK(im->w, im->h))
     {
        free(encoded_data);
        return 0;
     }

   vp8return = WebPGetFeatures(encoded_data, stats.st_size, &features);
   if (vp8return != VP8_STATUS_OK)
     {
        fprintf(stderr, "%s: Error reading file header: %s\n",
                im->real_file, webp_strerror(vp8return));
        free(encoded_data);
        return 0;
     }

   if (features.has_alpha == 0)
      UNSET_FLAG(im->flags, F_HAS_ALPHA);
   else
      SET_FLAG(im->flags, F_HAS_ALPHA);

   if (im->loader || immediate_load || progress)
     {
        size_t              webp_buffer_size = sizeof(DATA32) * im->w * im->h;

        __imlib_AllocateData(im);
        if (WebPDecodeBGRAInto(encoded_data, stats.st_size,
                               (uint8_t *) im->data, webp_buffer_size,
                               im->w * 4) == NULL)
          {
             free(encoded_data);
             __imlib_FreeData(im);
             return 0;
          }

        if (progress)
           progress(im, 100, 0, 0, im->w, im->h);
     }

   free(encoded_data);
   return 1;                    /* Success */
}

char
save(ImlibImage * im, ImlibProgressFunction progress, char progress_granularity)
{
   int                 encoded_fd;
   ImlibImageTag      *quality_tag;
   float               quality;
   uint8_t            *encoded_data;
   ssize_t             encoded_size;

   if (!im->data)
      return 0;

   encoded_fd = open(im->real_file,
                     O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

   if (encoded_fd < 0)
     {
        perror(im->real_file);
        return 0;
     }

   quality = 75;
   quality_tag = __imlib_GetTag(im, "quality");
   if (quality_tag)
     {
        quality = quality_tag->val;
        if (quality < 0)
          {
             fprintf(stderr,
                     "Warning: 'quality' setting %f too low for WebP, using 0\n",
                     quality);
             quality = 0;
          }

        if (quality > 100)
          {
             fprintf(stderr,
                     "Warning, 'quality' setting %f too high for WebP, using 100\n",
                     quality);
             quality = 100;
          }
     }

   encoded_size = WebPEncodeBGRA((uint8_t *) im->data, im->w, im->h,
                                 im->w * 4, quality, &encoded_data);

   if (write(encoded_fd, encoded_data, encoded_size) < encoded_size)
     {
        close(encoded_fd);
        WebPFree(encoded_data);
        perror(im->real_file);
        return 0;
     }

   close(encoded_fd);
   WebPFree(encoded_data);
   return 1;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "webp" };
   int                 i;

   l->num_formats = sizeof(list_formats) / sizeof(char *);
   l->formats = malloc(sizeof(char *) * l->num_formats);

   for (i = 0; i < l->num_formats; i++)
      l->formats[i] = strdup(list_formats[i]);
}
