#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([gaim-gaym],[0.33.0+svn],[gaymplugin@yahoogroups.com])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([1.9.5])

REQUIRED_GAIM="gaim >= 1.2 gaim < 2.0"
AC_SUBST(REQUIRED_GAIM)

AC_CONFIG_SRCDIR([src/gaym.h])
AC_CONFIG_HEADER([config.h])

AC_DEFINE(GAIM_PLUGINS, 1, [Define if plugins are enabled.])

# Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"
AC_PROG_INSTALL

# Checks for libraries.
PKG_CHECK_MODULES([GAIM], [$REQUIRED_GAIM],
	[
	AC_SUBST(GAIM_CFLAGS)
	AC_SUBST(GAIM_LIBS)
	]
)
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

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([gethostname inet_ntoa memmove strcasecmp strchr strdup strerror strpbrk strrchr strstr strtoul])

if test "x$GCC" = "xyes"; then
	CFLAGS="$CFLAGS -Wall -g3"
fi
AC_SUBST(CFLAGS)

AC_CONFIG_FILES([Makefile
                 nsis/Makefile
		 nsis/locale/Makefile
                 pixmaps/Makefile
                 src/Makefile])
AC_OUTPUT

echo
echo $PACKAGE $VERSION will be installed in $GAIM_LIBDIR/gaim
echo
echo configure complete, now type \'make\'
echo
