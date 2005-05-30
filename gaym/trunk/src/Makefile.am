EXTRA_DIST = \
	Makefile.mingw

pkgdir = \
	$(GAIM_LIBDIR)/gaim

GAYMSOURCES = \
	cmds.c \
	dcc_send.c \
	gaym.c \
	gaym.h \
	helpers.c \
	helpers.h \
	msgs.c \
	parse.c \
	weblogin.c 

AM_CFLAGS = \
	$(st)

libgaym_la_LDFLAGS = \
	-module \
	-avoid-version \
	$(GAIM_LIBS)

pkg_LTLIBRARIES = \
	libgaym.la

libgaym_la_SOURCES = \
	$(GAYMSOURCES)

AM_CPPFLAGS = \
	$(GAIM_CFLAGS) \
	$(DEBUG_CFLAGS)
