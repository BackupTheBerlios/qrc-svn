EXTRA_DIST = PROTOCOL Makefile.mingw

pkgdir = $(libdir)/gaim

GAYMSOURCES = weblogin.c gaym.c parse.c cmds.c msgs.c gaym.h dcc_send.c helpers.c

AM_CFLAGS = $(st)

libgaym_la_LDFLAGS = -module -avoid-version

if STATIC_GAYM

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES = libgaym.a
pkg_LTLIBRARIES =

libgaym_a_SOURCES = $(GAYMSOURCES)
libgaym_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES = libgaym.la
noinst_LIBRARIES =

libgaym_la_SOURCES = $(GAYMSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
