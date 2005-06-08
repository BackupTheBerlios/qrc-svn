EXTRA_DIST = PROTOCOL Makefile.mingw

pkgdir = $(libdir)/gaim

CHATSORTSOURCES = roombrowse.c

AM_CFLAGS = $(st)

libroombrowse_la_LDFLAGS = -module -avoid-version

pkg_LTLIBRARIES = libroombrowse.la

libroombrowse_la_SOURCES = $(CHATSORTSOURCES)

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	-I/usr/include/gaim
