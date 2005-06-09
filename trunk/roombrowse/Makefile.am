EXTRA_DIST = \
	Makefile.mingw

pkgdir = \
	$(GAIM_LIBDIR)/gaim

GAYMSOURCES = \
	roombrowse.c

AM_CFLAGS = \
	$(st)

libroombrowse_la_LDFLAGS = \
	-module \
	-avoid-version \
	$(GAIM_LIBS)

pkg_LTLIBRARIES = \
	libroombrowse.la

libroombrowse_la_SOURCES = \
	$(GAYMSOURCES)

AM_CPPFLAGS = \
	$(GAIM_CFLAGS) \
	$(DEBUG_CFLAGS)
