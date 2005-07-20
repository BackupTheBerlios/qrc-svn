pkgdir = \
	$(GAIM_LIBDIR)/gaim

GAYMSOURCES = \
	bot-challenger.c

AM_CFLAGS = \
	$(st)

libbot_challenger_la_LDFLAGS = \
	-module \
	-avoid-version \
	$(GAIM_LIBS)

pkg_LTLIBRARIES = \
	libbot-challenger.la

libbot_challenger_la_SOURCES = \
	$(GAYMSOURCES)

AM_CPPFLAGS = \
	$(DEBUG_CFLAGS) \
	$(GAIM_CFLAGS)
