# Required initializer
AC_PREREQ(2.59)
AC_INIT(tsb-c, 0.1.0, ron@pedde.com)
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_C_INLINE

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

PKG_PROG_PKG_CONFIG

ALL_CFLAGS="-std=c99 -D_GNU_SOURCE"

AC_ARG_ENABLE(debug, [  --enable-debug                Enable debugging switches],
                       [ case "${enableval}" in
                         yes) DEBUG_CFLAGS="-O0 -fstack-protector-all -Wall -g";;
                         no) DEBUG_CPPFLAGS="-DNDEBUG";;
                         *) AC_MSG_ERROR(bad value ${enableval} for --enable-debug);;
                         esac ],
                         DEBUG_CFLAGS="-O0 -fstack-protector-all -Wall -g")

AC_C_CONST
AC_OUTPUT(Makefile src/Makefile)
