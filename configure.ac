AC_INIT([uredir], [3.1], [https://github.com/troglobit/uredir/issues])
AM_INIT_AUTOMAKE([1.11 foreign])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([uredir.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_INSTALL
AC_HEADER_STDC

# Check for required libraries
PKG_CHECK_MODULES([uev],  [libuev >= 2.1.1])

AC_OUTPUT
