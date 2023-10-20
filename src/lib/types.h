#ifndef TYPES_H
#define TYPES_H 1

#include <stdbool.h>
#include <stdint.h>

typedef struct _ImlibLoader ImlibLoader;

typedef struct _ImlibImage ImlibImage;
typedef unsigned int ImlibImageFlags;

typedef int         (*ImlibProgressFunction)(ImlibImage * im, char percent,
                                             int update_x, int update_y,
                                             int update_w, int update_h);

typedef int         ImlibOp;

typedef struct _ImlibColorModifier ImlibColorModifier;

typedef struct _ImlibUpdate ImlibUpdate;

#endif /* TYPES_H */
