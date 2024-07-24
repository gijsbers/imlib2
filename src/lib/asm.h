#ifndef __ASM_H
#define __ASM_H

#define PR_(sym) __##sym

#if defined(__GNUC__) && (__GNUC__ >= 4)
#define HIDDEN_(sym) .hidden PR_(sym)
#else
#define HIDDEN_(sym)
#endif

#define FN_(sym) \
    .global PR_(sym); \
    HIDDEN_(sym); \
    .type PR_(sym),@function;
#define SIZE(sym) \
    .size PR_(sym),.-PR_(sym); \
    .align 8;

#define ENDBR_
#ifdef __CET__
#ifdef __has_include
#if __has_include(<cet.h>)
#include <cet.h>
#undef  ENDBR_
#define ENDBR_ _CET_ENDBR
#endif
#endif
#endif

#endif                          /* __ASM_H */
