pkgdir = \
	$(GAIM_LIBDIR)/gaim

GAYMSOURCES = \
	display-options.c
	
AM_CFLAGS = \
	$(st)

libdisplay_options_la_LDFLAGS = \
	-module \
	-avoid-version \
	$(GTK_LIBS) \
	$(GAIM_LIBS)

pkg_LTLIBRARIES = \
	libdisplay-options.la

libdisplay_options_la_SOURCES = \
	$(GAYMSOURCES)

AM_CPPFLAGS = \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GAIM_CFLAGS)
	
