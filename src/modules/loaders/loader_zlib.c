#include "loader_common.h"

#include <zlib.h>

#define OUTBUF_SIZE 16484

static int
uncompress_file(const void *fdata, unsigned int fsize, int dest)
{
   int                 ok;
   z_stream            strm = { 0 };;
   unsigned char       outbuf[OUTBUF_SIZE];
   int                 ret = 1, bytes;

   ok = 0;

   ret = inflateInit2(&strm, 15 + 32);
   if (ret != Z_OK)
      return ok;

   strm.next_in = (void *)fdata;
   strm.avail_in = fsize;

   for (;;)
     {
        strm.next_out = outbuf;
        strm.avail_out = sizeof(outbuf);

        ret = inflate(&strm, 0);

        if (ret != Z_OK && ret != Z_STREAM_END)
           goto quit;

        bytes = sizeof(outbuf) - strm.avail_out;
        if (write(dest, outbuf, bytes) != bytes)
           goto quit;

        if (ret == Z_STREAM_END)
           break;
     }

   ok = 1;

 quit:
   inflateEnd(&strm);

   return ret;
}

static const char  *const list_formats[] = { "gz" };

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
