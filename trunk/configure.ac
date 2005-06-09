#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([qrc],[0.33.0+svn],[gaymplugin@yahoogroups.com])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([1.9.5])

REQUIRED_GAIM="gaim >= 1.2 gaim < 2.0"
AC_SUBST(REQUIRED_GAIM)

AC_CONFIG_SRCDIR([gaym/src/gaym.h])
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

AM_PATH_GLIB_2_0(2.0.0,,AC_MSG_ERROR([
*** GLib 2.0 is required to build the QRC plugins; please make sure you
*** have the GLib development headers installed. The latest version of
*** GLib is always available at http://www.gtk.org/.]))
AC_SUBST(GLIB_LIBS)
AC_SUBST(GLIB_CFLAGS)

AC_ARG_ENABLE([chatsort],AC_HELP_STRING([--enable-chatsort],[build the chatsort plugin @<:@default=no@:>@]),[want_chatsort="yes"],)

AC_ARG_ENABLE([gaym],AC_HELP_STRING([--enable-gaym],[build the gaym plugin @<:@default=yes@:>@]),,[want_gaym="yes"])

AC_ARG_ENABLE([privacy],AC_HELP_STRING([--enable-privacy],[build the privacy plugin @<:@default=no@:>@]),[want_privacy="yes"],)

AC_ARG_ENABLE([roombrowse],AC_HELP_STRING([--enable-roombrowse],[build the roombrowse plugin @<:@default=no@:>@]),[want_roombrowse="yes"],)

AM_CONDITIONAL([COND_CHATSORT], [test "$want_chatsort" = "yes"])
AM_CONDITIONAL([COND_GAYM], [test "$want_gaym" = "yes"])
AM_CONDITIONAL([COND_PRIVACY], [test "$want_privacy" = "yes"])
AM_CONDITIONAL([COND_ROOMBROWSE], [test "$want_roombrowse" = "yes"])

if test "$want_chatsort" != "yes" ; then
	want_chatsort="no"
fi

if test "$want_gaym" != "yes" ; then
	want_gaym="no"
fi

if test "$want_privacy" != "yes" ; then
	want_privacy="no"
fi

if test "$want_roombrowse" != "yes" ; then
	want_roombrowse="no"
fi

if test "$want_chatsort" = "yes" -o "$want_roombrowse" = "yes" ; then

AM_PATH_GTK_2_0(2.0.0,,AC_MSG_ERROR([
*** GTK+ 2.0 is required to build the roombrowse and chatsort QRC
*** plugins; please make sure you have the GTK+ development headers
*** installed. The latest version of GTK+ is always available at
*** http://www.gtk.org/.]))
AC_SUBST(GTK_LIBS)
AC_SUBST(GTK_CFLAGS)

fi

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
                 chatsort/Makefile
                 gaym/Makefile
                 gaym/pixmaps/Makefile
                 gaym/src/Makefile
                 nsis/Makefile
		 nsis/locale/Makefile
                 privacy/Makefile
                 roombrowse/Makefile])
AC_OUTPUT

echo
echo $PACKAGE $VERSION
echo
echo Build chatsort plugin............ : $want_chatsort
echo Build gaym plugin................ : $want_gaym
echo Build privacy plugin............. : $want_privacy
echo Build roombrowse plugin.......... : $want_roombrowse
echo
echo Installation directory........... : $GAIM_LIBDIR/gaim
echo
echo configure complete, now type \'make\'
echo
