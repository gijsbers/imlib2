#ifndef COMPRESSION_H
#define COMPRESSION_H 1

typedef int     (imlib_decompress_load_f) (const void *fdata,
                                           unsigned int fsize, int dest);

int             decompress_load(ImlibImage * im, int load_data,
                                const char *const *pext, int next,
                                imlib_decompress_load_f * fdec);

#endif                          /* COMPRESSION_H */
