pkgdir = \
	$(GAIM_LIBDIR)/gaim

GAYMSOURCES = \
	botfilter.c \
	botfilter.h \
	cmds.c \
	gaym.c \
	gaym.h \
	gayminfo.c \
	gayminfo.h \
	gaympriv.c \
	gaympriv.h \
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
	$(DEBUG_CFLAGS) \
	$(GAIM_CFLAGS) \
	$(GLIB_CFLAGS) \
	-DGAIM_PLUGINS
