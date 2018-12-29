## Copyright (c) 2001
## The libglademm development team.


if BUILD_NOT_MINGW32
pkglib_LTLIBRARIES = libaeskulap-glade.la
else
lib_LTLIBRARIES = libaeskulap-glade.la
endif

libaeskulap_glade_la_SOURCES = \
	init.cc  \
	init.h  \
	variablesmap.cc  \
	variablesmap.h  \
	wrap_init.cc  \
	wrap_init.h  \
	xml.cc  \
	xml.h  \
	xml_p.h

libaeskulap_glade_la_LIBADD = \
	$(GLADE_LIBS) \
	$(GTKMM_LIBS)
	

libaeskulap_glade_la_LDFLAGS = \
	$(LDFLAGS_AESKULAP_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(GLADE_CFLAGS) \
	$(GTKMM_CFLAGS)
