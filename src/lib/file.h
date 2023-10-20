#ifndef __FILE_H
#define __FILE_H 1

#include <stdint.h>
#include <sys/stat.h>

char               *__imlib_FileKey(const char *file);
char               *__imlib_FileRealFile(const char *file);

const char         *__imlib_FileExtension(const char *file);

uint64_t            __imlib_StatModDate(const struct stat *st);

static inline int
__imlib_StatIsFile(const struct stat *st)
{
   return S_ISREG(st->st_mode);
}

static inline int
__imlib_StatIsDir(const struct stat *st)
{
   return S_ISDIR(st->st_mode);
}

int                 __imlib_FileIsFile(const char *s);

uint64_t            __imlib_FileModDate(const char *s);
uint64_t            __imlib_FileModDateFd(int fd);

char              **__imlib_FileDir(const char *dir, int *num);
void                __imlib_FileFreeDirList(char **l, int num);

int                 __imlib_ItemInList(char **list, int size, char *item);

char              **__imlib_PathToFilters(void);
char              **__imlib_PathToLoaders(void);
char              **__imlib_ModulesList(char **path, int *num_ret);
char               *__imlib_ModuleFind(char **path, const char *name);

#include <stdio.h>
FILE               *__imlib_FileOpen(const char *path, const char *mode,
                                     struct stat *st);

#endif
