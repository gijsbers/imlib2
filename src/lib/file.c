#include "common.h"

#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "debug.h"
#include "file.h"

#define DBG_PFX "FILE"
#define DP(fmt...) DC(DBG_FILE, fmt)

char               *
__imlib_FileKey(const char *file)
{
   const char         *p;

   for (p = file;;)
     {
        p = strchr(p, ':');
        if (!p)
           break;
        p++;
        if (*p == '\0')
           break;
        if (*p != ':')          /* :: Drive spec? */
           return strdup(p);
        p++;
     }

   return NULL;
}

char               *
__imlib_FileRealFile(const char *file)
{
   char               *newfile;

   DP("%s: '%s'\n", __func__, file);

   newfile = malloc(strlen(file) + 1);
   if (!newfile)
      return NULL;
   newfile[0] = 0;
   {
      char               *p1, *p2;

      p1 = (char *)file;
      p2 = newfile;
      while (p1[0])
        {
           if (p1[0] == ':')
             {
                if (p1[1] == ':')
                  {
                     p2[0] = ':';
                     p2++;
                     p1++;
                  }
                else
                  {
                     p2[0] = 0;
                     return newfile;
                  }
             }
           else
             {
                p2[0] = p1[0];
                p2++;
             }
           p1++;
        }
      p2[0] = p1[0];
   }
   return newfile;
}

const char         *
__imlib_FileExtension(const char *file)
{
   const char         *p, *s;
   int                 ch;

   DP("%s: '%s'\n", __func__, file);

   if (!file)
      return NULL;

   for (p = s = file; (ch = *s) != 0; s++)
     {
        if (ch == '.' || ch == '/')
           p = s + 1;
     }
   return *p != '\0' ? p : NULL;
}

static int
__imlib_FileStat(const char *file, struct stat *st)
{
   DP("%s: '%s'\n", __func__, file);

   if ((!file) || (!*file))
      return -1;

   return stat(file, st);
}

int
__imlib_FileIsFile(const char *s)
{
   struct stat         st;

   DP("%s: '%s'\n", __func__, s);

   if (__imlib_FileStat(s, &st))
      return 0;

   return (S_ISREG(st.st_mode)) ? 1 : 0;
}

#define STONS 1000000000ULL

uint64_t
__imlib_StatModDate(const struct stat *st)
{
   uint64_t            mtime_ns, ctime_ns;

#if HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
   mtime_ns = st->st_mtim.tv_sec * STONS + st->st_mtim.tv_nsec;
   ctime_ns = st->st_ctim.tv_sec * STONS + st->st_ctim.tv_nsec;
#elif HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC
   mtime_ns = st->st_mtimespec.tv_sec * STONS + st->st_mtimespec.tv_nsec;
   ctime_ns = st->st_ctimespec.tv_sec * STONS + st->st_ctimespec.tv_nsec;
#else
   mtime_ns = st->st_mtime;
   ctime_ns = st->st_ctime;
#endif

   return (mtime_ns > ctime_ns) ? mtime_ns : ctime_ns;
}

uint64_t
__imlib_FileModDate(const char *s)
{
   struct stat         st;

   DP("%s: '%s'\n", __func__, s);

   if (__imlib_FileStat(s, &st))
      return 0;

   return __imlib_StatModDate(&st);
}

uint64_t
__imlib_FileModDateFd(int fd)
{
   struct stat         st;

   if (fstat(fd, &st) < 0)
      return 0;

   return __imlib_StatModDate(&st);
}

char              **
__imlib_FileDir(const char *dir, int *num)
{
   int                 i, dirlen;
   int                 done = 0;
   DIR                *dirp;
   char              **names;
   struct dirent      *dp;

   if ((!dir) || (!*dir))
      return NULL;

   dirp = opendir(dir);
   if (!dirp)
      return NULL;

   /* count # of entries in dir (worst case) */
   for (dirlen = 0; readdir(dirp) != NULL; dirlen++)
      ;
   if (!dirlen)
     {
        closedir(dirp);
        return NULL;
     }

   names = malloc(dirlen * sizeof(char *));
   if (!names)
     {
        closedir(dirp);
        return NULL;
     }

   rewinddir(dirp);
   for (i = 0; i < dirlen;)
     {
        dp = readdir(dirp);
        if (!dp)
           break;

        if ((strcmp(dp->d_name, ".")) && (strcmp(dp->d_name, "..")))
          {
             names[i] = strdup(dp->d_name);
             i++;
          }
     }
   closedir(dirp);

   if (i < dirlen)
      dirlen = i;               /* dir got shorter... */
   *num = dirlen;

   /* do a simple bubble sort here to alphanumberic it */
   while (!done)
     {
        done = 1;
        for (i = 0; i < dirlen - 1; i++)
          {
             if (strcmp(names[i], names[i + 1]) > 0)
               {
                  char               *tmp;

                  tmp = names[i];
                  names[i] = names[i + 1];
                  names[i + 1] = tmp;
                  done = 0;
               }
          }
     }

   return names;
}

void
__imlib_FileFreeDirList(char **l, int num)
{
   if (!l)
      return;
   while (num--)
      free(l[num]);
   free(l);
}

int
__imlib_ItemInList(char **list, int size, char *item)
{
   int                 i;

   if (!list)
      return 0;
   if (!item)
      return 0;

   for (i = 0; i < size; i++)
     {
        if (!strcmp(list[i], item))
           return 1;
     }
   return 0;
}

#include <errno.h>

FILE               *
__imlib_FileOpen(const char *path, const char *mode, struct stat *st)
{
   FILE               *fp;

   for (;;)
     {
        fp = fopen(path, mode);
        if (fp)
           break;
        if (errno != EINTR)
           break;
     }

   /* Only stat if all is good and we want to read */
   if (!fp || !st)
      goto done;
   if (mode[0] == 'w')
      goto done;

   if (fstat(fileno(fp), st) < 0)
     {
        fclose(fp);
        fp = NULL;
     }

 done:
   return fp;
}
