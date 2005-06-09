EXTRA_DIST = \
	Makefile.mingw

pkgdir = \
	$(GAIM_LIBDIR)/gaim

GAYMSOURCES = \
	chatsort.c

AM_CFLAGS = \
	$(st)

libchatsort_la_LDFLAGS = \
	-module \
	-avoid-version \
	$(GAIM_LIBS)

pkg_LTLIBRARIES = \
	libchatsort.la

libchatsort_la_SOURCES = \
	$(GAYMSOURCES)

AM_CPPFLAGS = \
	$(GAIM_CFLAGS) \
	$(DEBUG_CFLAGS)
