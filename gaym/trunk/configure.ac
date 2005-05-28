dnl Process this file with autoconf to produce a configure script.
AC_INIT([gaim-gaym], [0.21+20050513])
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)

REQUIRED_GAIM="gaim >= 1.2 gaim < 2.0"
AC_SUBST(REQUIRED_GAIM)

dnl Checks for header files.
AC_HEADER_STDC
AC_DISABLE_STATIC
AM_DISABLE_STATIC

AC_PROG_LIBTOOL
AC_DEFINE(GAIM_PLUGINS, 1, [Define if plugins are enabled.])

dnl gaim required at the indicated version
dnl set location of gaim headers, data, and libs
PKG_CHECK_MODULES([GAIM], [$REQUIRED_GAIM],
	[
	AC_SUBST(GAIM_CFLAGS)
	AC_SUBST(GAIM_LIBS)
	]
)

dnl hack because PKG_CHECK_MODULES only sets CFLAGS and LIBS
dnl this sets GAIM_DATADIR and GAIM_LIBDIR
AC_ARG_VAR([GAIM_DATADIR], [datadir of GAIM, overriding pkg-config])dnl
AC_ARG_VAR([GAIM_LIBDIR], [libdir of GAIM, overriding pkg-config])dnl
AC_CACHE_CHECK([for GAIM][_DATADIR], [pkg_cv_][GAIM][_DATADIR],
	[_PKG_CONFIG([GAIM][_DATADIR], [variable=datadir], [gaim])])
AC_CACHE_CHECK([for GAIM][_LIBDIR], [pkg_cv_][GAIM][_LIBDIR],
	[_PKG_CONFIG([GAIM][_LIBDIR], [variable=libdir], [gaim])])
GAIM[]_DATADIR=$pkg_cv_[]GAIM[]_DATADIR
GAIM[]_LIBDIR=$pkg_cv_[]GAIM[]_LIBDIR
AC_SUBST(GAIM_DATADIR)
AC_SUBST(GAIM_LIBDIR)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_CHECK_FUNCS(strdup strerror strstr strtoul)

AC_OUTPUT([Makefile src/Makefile pixmaps/Makefile])
