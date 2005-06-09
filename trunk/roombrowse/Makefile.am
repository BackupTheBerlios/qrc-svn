pkgdir = \
	$(GAIM_LIBDIR)/gaim

GAYMSOURCES = \
	roombrowse.c

AM_CFLAGS = \
	$(st)

libroombrowse_la_LDFLAGS = \
	-module \
	-avoid-version \
	$(GTK_LIBS) \
	$(GAIM_LIBS)

pkg_LTLIBRARIES = \
	libroombrowse.la

libroombrowse_la_SOURCES = \
	$(GAYMSOURCES)

AM_CPPFLAGS = \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GAIM_CFLAGS)
