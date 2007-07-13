pkgdir = \
	$(GAIM_LIBDIR)/gaim

GAYMSOURCES = \
	gaym-extras.c chatsort.c bio-popups.c chaticon.c gaym-extras.h roombrowse.c

AM_CFLAGS = \
	$(st)

libgaym_extras_la_LDFLAGS = \
	-module \
	-avoid-version \
	$(GTK_LIBS) \
	$(GAIM_LIBS)

pkg_LTLIBRARIES = \
	libgaym-extras.la

libgaym_extras_la_SOURCES = \
	$(GAYMSOURCES)

AM_CPPFLAGS = \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	-DGAIM_DATADIR=\"$(datadir)\" \
	$(GAIM_CFLAGS)
	
