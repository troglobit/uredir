AC_INIT([uredir], [3.0-beta1], [https://github.com/troglobit/uredir/issues])
AM_INIT_AUTOMAKE([1.11 foreign])

AC_CONFIG_SRCDIR([uredir.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_INSTALL
AC_HEADER_STDC

AC_CHECK_HEADER([ev.h],
	AC_CHECK_LIB([ev], [ev_loop_new],,
		AC_MSG_ERROR("libev not found")),
	AC_MSG_ERROR("ev.h not found"))

AC_OUTPUT
