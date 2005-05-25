dnl Process this file with autoconf to produce a configure script.
AC_INIT([gaim-gaym], [0.21+20050513])
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)
dnl Checks for programs.

dnl Checks for libraries.
AM_PATH_GLIB_2_0
AC_SUBST(GLIB_CFLAGS)
dnl Checks for header files.
AC_HEADER_STDC
AC_DISABLE_STATIC
AM_DISABLE_STATIC

AC_PROG_LIBTOOL
AC_DEFINE(GAIM_PLUGINS, 1, [Define if plugins are enabled.])
AC_PREFIX_PROGRAM(gaim)
dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_CHECK_FUNCS(strdup strerror strstr strtoul)

AC_OUTPUT([Makefile src/Makefile])
