EXTRA_DIST = PROTOCOL Makefile.mingw

pkgdir = $(libdir)/gaim
gaimheaders = ${includedir}/gaim
GAYMSOURCES = weblogin.c gaym.c parse.c cmds.c msgs.c gaym.h dcc_send.c helpers.c

AM_CFLAGS = $(st)

libgaym_la_LDFLAGS = -module -avoid-version


st =
pkg_LTLIBRARIES = libgaym.la
noinst_LIBRARIES =

libgaym_la_SOURCES = $(GAYMSOURCES)

#endif

AM_CPPFLAGS = \
	-I$(gaimheaders)\
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
