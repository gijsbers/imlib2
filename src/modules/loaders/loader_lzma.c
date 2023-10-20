#include "loader_common.h"

#include <lzma.h>

#define OUTBUF_SIZE 16484

static int
uncompress_file(const void *fdata, unsigned int fsize, int dest)
{
   int                 ok;
   lzma_stream         strm = LZMA_STREAM_INIT;
   lzma_ret            ret;
   uint8_t             outbuf[OUTBUF_SIZE];
   ssize_t             bytes;

   ok = 0;

   ret = lzma_auto_decoder(&strm, UINT64_MAX, 0);
   if (ret != LZMA_OK)
      return ok;

   strm.next_in = fdata;
   strm.avail_in = fsize;

   for (;;)
     {
        strm.next_out = outbuf;
        strm.avail_out = sizeof(outbuf);

        ret = lzma_code(&strm, 0);

        if (ret != LZMA_OK && ret != LZMA_STREAM_END)
           goto quit;

        bytes = sizeof(outbuf) - strm.avail_out;
        if (write(dest, outbuf, bytes) != bytes)
           goto quit;

        if (ret == LZMA_STREAM_END)
           break;
     }

   ok = 1;

 quit:
   lzma_end(&strm);

   return ok;
}

static const char  *const list_formats[] = { "xz", "lzma" };

int
load2(ImlibImage * im, int load_data)
{

   return decompress_load(im, load_data, list_formats, ARRAY_SIZE(list_formats),
                          uncompress_file);
}

void
formats(ImlibLoader * l)
{
   __imlib_LoaderSetFormats(l, list_formats, ARRAY_SIZE(list_formats));
}
