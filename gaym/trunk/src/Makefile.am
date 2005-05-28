EXTRA_DIST = PROTOCOL Makefile.mingw

pkgdir = $(GAIM_LIBDIR)/gaim

GAYMSOURCES = weblogin.c gaym.c parse.c cmds.c msgs.c gaym.h dcc_send.c helpers.c

AM_CFLAGS = $(st)

libgaym_la_LDFLAGS = -module -avoid-version $(GAIM_LIBS)

pkg_LTLIBRARIES = libgaym.la

libgaym_la_SOURCES = $(GAYMSOURCES)

AM_CPPFLAGS = \
	$(GAIM_CFLAGS) \
	$(DEBUG_CFLAGS)
