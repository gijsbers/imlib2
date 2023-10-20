#include "loader_common.h"

#include <bzlib.h>

#define OUTBUF_SIZE 16384
#define INBUF_SIZE 1024

static int
uncompress_file(FILE * fp, int dest)
{
   BZFILE             *bf;
   DATA8               outbuf[OUTBUF_SIZE];
   int                 bytes, error, ret = 1;

   bf = BZ2_bzReadOpen(&error, fp, 0, 0, NULL, 0);

   if (error != BZ_OK)
     {
        BZ2_bzReadClose(NULL, bf);
        return 0;
     }

   while (1)
     {
        bytes = BZ2_bzRead(&error, bf, &outbuf, OUTBUF_SIZE);

        if (error == BZ_OK || error == BZ_STREAM_END)
           if (write(dest, outbuf, bytes) != bytes)
              break;

        if (error == BZ_STREAM_END)
           break;
        else if (error != BZ_OK)
          {
             ret = 0;
             break;
          }
     }

   BZ2_bzReadClose(&error, bf);

   return ret;
}

int
load2(ImlibImage * im, int load_data)
{
   ImlibLoader        *loader;
   int                 dest, res;
   const char         *s, *p, *q;
   char                tmp[] = "/tmp/imlib2_loader_bz2-XXXXXX";
   char               *real_ext;

   /* make sure this file ends in ".bz2" and that there's another ext
    * (e.g. "foo.png.bz2") */
   for (p = s = im->real_file, q = NULL; *s; s++)
     {
        if (*s != '.' && *s != '/')
           continue;
        q = p;
        p = s + 1;
     }
   if (!q || strcasecmp(p, "bz2"))
      return 0;

   if (!(real_ext = strndup(q, p - q - 1)))
      return 0;

   loader = __imlib_FindBestLoaderForFormat(real_ext, 0);
   free(real_ext);
   if (!loader)
      return 0;

   if ((dest = mkstemp(tmp)) < 0)
      return 0;

   res = uncompress_file(im->fp, dest);
   close(dest);

   if (!res)
      goto quit;

   res = __imlib_LoadEmbedded(loader, im, tmp, load_data);

 quit:
   unlink(tmp);

   return res;
}

void
formats(ImlibLoader * l)
{
   static const char  *const list_formats[] = { "bz2" };
   __imlib_LoaderSetFormats(l, list_formats, ARRAY_SIZE(list_formats));
}
