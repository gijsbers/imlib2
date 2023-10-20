#include "loader_common.h"

int
decompress_load(ImlibImage * im, int load_data, const char *const *pext,
                int next, imlib_decompress_load_f * fdec)
{
   int                 rc, i;
   ImlibLoader        *loader;
   int                 dest, res;
   const char         *s, *p, *q;
   char                tmp[] = "/tmp/imlib2_loader_dec-XXXXXX";
   char               *real_ext;
   void               *fdata;

   rc = LOAD_FAIL;

   /* make sure this file ends in a known name and that there's another ext
    * (e.g. "foo.png.bz2") */
   for (p = s = im->real_file, q = NULL; *s; s++)
     {
        if (*s != '.' && *s != '/')
           continue;
        q = p;
        p = s + 1;
     }

   if (!q)
      return rc;

   res = 0;
   for (i = 0; i < next; i++)
     {
        if (strcasecmp(p, pext[i]))
           continue;
        res = 1;
        break;
     }
   if (!res)
      return rc;

   if (!(real_ext = strndup(q, p - q - 1)))
      return LOAD_OOM;

   loader = __imlib_FindBestLoader(NULL, real_ext, 0);
   free(real_ext);
   if (!loader)
      return rc;

   fdata = mmap(NULL, im->fsize, PROT_READ, MAP_SHARED, fileno(im->fp), 0);
   if (fdata == MAP_FAILED)
      return LOAD_BADFILE;

   if ((dest = mkstemp(tmp)) < 0)
      QUIT_WITH_RC(LOAD_OOM);

   res = fdec(fdata, im->fsize, dest);

   close(dest);

   if (res)
      rc = __imlib_LoadEmbedded(loader, im, tmp, load_data);

   unlink(tmp);

 quit:
   munmap(fdata, im->fsize);

   return rc;
}
