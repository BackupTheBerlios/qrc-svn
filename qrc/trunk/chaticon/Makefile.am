pkgdir = \
	$(GAIM_LIBDIR)/gaim

GAYMSOURCES = \
	chaticon.c

AM_CFLAGS = \
	$(st)

libchaticon_la_LDFLAGS = \
	-module \
	-avoid-version \
	$(GTK_LIBS) \
	$(GAIM_LIBS)

pkg_LTLIBRARIES = \
	libchaticon.la

libchaticon_la_SOURCES = \
	$(GAYMSOURCES)

AM_CPPFLAGS = \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GAIM_CFLAGS)
