dnl Copyright (C) 2023 Kim Woelders
dnl This code is public domain and can be freely used or copied.

dnl Macro to define __PACKED__ if supported and --enable-packing is given

dnl Usage: EC_C_PACKING()

AC_DEFUN([EC_C_PACKING], [
  AC_ARG_ENABLE([packing],
    [AS_HELP_STRING([--enable-packing],
                    [enable packing structures for unaligned access @<:@default=no@:>@])],,
    [enable_packing=no])

  if test "x$enable_packing" = "xyes"; then
    if test -n "$GCC"; then
      AC_DEFINE(__PACKED__, __attribute__((packed)), [Use struct packing for unaligned access])
    else
      AC_MSG_ERROR([Struct packing was requested but no method is known])
    fi
  else
    AC_DEFINE(__PACKED__, , [Not using struct packing for unaligned access])
  fi
])
