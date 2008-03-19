pkgdir = \
	$(PURPLE_LIBDIR)/purple-2

GAYMSOURCES = \
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
	weblogin.c \
	url_fetch.c

AM_CFLAGS = \
	$(st)

libgaym_la_LDFLAGS = \
	-module \
	-avoid-version \
	$(PURPLE_LIBS)

pkg_LTLIBRARIES = \
	libgaym.la

libgaym_la_SOURCES = \
	$(GAYMSOURCES)

AM_CPPFLAGS = \
	$(DEBUG_CFLAGS) \
	$(PURPLE_CFLAGS) \
	$(GLIB_CFLAGS) \
	-DPURPLE_PLUGINS
