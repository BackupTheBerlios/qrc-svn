#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([qrc],[0.34.1+svn],[gaymplugin@yahoogroups.com])
AC_CANONICAL_TARGET([])
AM_INIT_AUTOMAKE([1.9.5 no-dist-gzip dist-bzip2])

REQUIRED_PKG_CONFIG="0.15.0"
AC_SUBST(REQUIRED_PKG_CONFIG)

REQUIRED_GAIM="gaim >= 1.2 gaim < 2.0"
AC_SUBST(REQUIRED_GAIM)

REQUIRED_GTK="gtk+-2.0"
AC_SUBST(REQUIRED_GTK)

AC_CONFIG_SRCDIR([gaym/src/gaym.h])
AC_CONFIG_HEADER([config.h])

AC_DEFINE(GAIM_PLUGINS, 1, [Define if plugins are enabled.])

# Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"
AC_PROG_INSTALL
AC_PATH_PROG(PKG_CONFIG, [pkg-config], [no])

if test x$PKG_CONFIG = xno ; then
  AC_MSG_ERROR([*** pkg-config not found])
fi

# Build preferences


AC_ARG_ENABLE([display-options],
	[AS_HELP_STRING(--enable-display_options,build the bot-challenger plugin @<:@default=yes@:>@)],,
	[enable_display_options="no"])
	
AC_ARG_ENABLE([bot-challenger],
	[AS_HELP_STRING(--enable-bot-challenger,build the bot-challenger plugin @<:@default=yes@:>@)],,
	[enable_bot_challenger="yes"])

AC_ARG_ENABLE([gaym],
	[AS_HELP_STRING(--enable-gaym,build the gaym plugin @<:@default=yes@:>@)],,
	[enable_gaym="yes"])

AC_ARG_ENABLE([gaym-extras],
	[AS_HELP_STRING(--enable-gaym-extras,build the gaym-extras plugin @<:@default=yes@:>@)],,
	[enable_gaym_extras="yes"])

AM_CONDITIONAL([COND_BOT_CHALLENGER], [test "$enable_bot_challenger" = "yes"])
AM_CONDITIONAL([COND_DISPLAY_OPTIONS], [test "$enable_display_options" = "yes"])
AM_CONDITIONAL([COND_GAYM], [test "$enable_gaym" = "yes"])
AM_CONDITIONAL([COND_GAYM_EXTRAS], [test "$enable_gaym_extras" = "yes"])

if test "$enable_bot_challenger" != "yes" ; then
	enable_bot_challenger="no"
fi

if test "$enable_display_options" != "yes" ; then
	enable_display_options="no"
fi
if test "$enable_gaym" != "yes" ; then
	enable_gaym="no"
fi

if test "$enable_gaym_extras" != "yes" ; then
	enable_gaym_extras="no"
fi

# Checks for libraries.
AC_ARG_WITH([gaim-prefix],
	[AS_HELP_STRING(--with-gaim-prefix,specify the prefix where gaim is found)],
	PKG_CONFIG_PATH=$withval/lib/pkgconfig,[])
export PKG_CONFIG_PATH

PKG_PROG_PKG_CONFIG([$REQUIRED_PKG_CONFIG])
if test "$enable_gaym_extras" = "yes" ; then
	PKG_CHECK_MODULES([GTK], [$REQUIRED_GTK],
		[
		AC_SUBST(GTK_CFLAGS)
		AC_SUBST(GTK_LIBS)
		]
	)
fi
if test "$enable_display_options" = "yes" ; then
	PKG_CHECK_MODULES([GTK], [$REQUIRED_GTK],
		[
		AC_SUBST(GTK_CFLAGS)
		AC_SUBST(GTK_LIBS)
		]
	)
fi
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

case "$host" in
	*-*-mingw*)
	CFLAGS="$CFLAGS -O2 -Wall -mno-cygwin -mms-bitfields"
	LIBS="$LIBS -no-undefined -Wl,--enable-auto-image-base -lws2_32"
	;;
	*)
	;;
esac

if test "x$GCC" = "xyes"; then
	CFLAGS="$CFLAGS -Wall -g3"
fi
AC_SUBST(CFLAGS)


AC_CONFIG_FILES([Makefile
                 bot-challenger/Makefile
		 display-options/Makefile
                 gaym/Makefile
                 gaym/pixmaps/Makefile
                 gaym/src/Makefile
                 gaym-extras/Makefile
		 gaym-extras/src/Makefile
                 gaym-extras/pixmaps/Makefile
                 nsis/Makefile
		 nsis/locale/Makefile])
AC_OUTPUT

echo
echo $PACKAGE $VERSION
echo
echo Build bot-challenger plugin...... : $enable_bot_challenger
echo Build display_options plugin...... : $enable_display_options
echo Build gaym plugin................ : $enable_gaym
echo Build gaym-extras plugin......... : $enable_gaym_extras
echo
echo Installation directory........... : $GAIM_LIBDIR/gaim
echo
echo configure complete, now type \'make\'
echo
