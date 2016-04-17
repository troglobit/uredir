AC_INIT([uredir], [1.0], [https://github.com/troglobit/uredir/issues])
AM_INIT_AUTOMAKE([1.11 foreign])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([uredir.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_INSTALL

AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h stdlib.h sys/socket.h netinet/in.h)
AC_CHECK_FUNCS(getopt)

AC_OUTPUT
