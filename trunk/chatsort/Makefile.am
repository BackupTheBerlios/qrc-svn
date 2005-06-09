pkgdir = \
	$(GAIM_LIBDIR)/gaim

GAYMSOURCES = \
	chatsort.c

AM_CFLAGS = \
	$(st)

libchatsort_la_LDFLAGS = \
	-module \
	-avoid-version \
	$(GTK_LIBS) \
	$(GAIM_LIBS)

pkg_LTLIBRARIES = \
	libchatsort.la

libchatsort_la_SOURCES = \
	$(GAYMSOURCES)

AM_CPPFLAGS = \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GAIM_CFLAGS)
