EXTRA_DIST = PROTOCOL Makefile.mingw

pkgdir = $(libdir)/gaim

CHATSORTSOURCES = chatsort.c

AM_CFLAGS = $(st)

libchatsort_la_LDFLAGS = -module -avoid-version

pkg_LTLIBRARIES = libchatsort.la

libchatsort_la_SOURCES = $(CHATSORTSOURCES)

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	-I/usr/include/gaim
