EXTRA_DIST = \
	adapter_minimal.cc \
	adapter_passthru.cc \
	adapter_captivating.cc \
	adapter_modifying.cc

lib_LTLIBRARIES = \
	ecap_adapter_minimal.la \
	ecap_adapter_passthru.la \
	ecap_adapter_captivating.la \
	ecap_adapter_modifying.la

noinst_HEADERS = \
	james_ecap.h \
	\
	autoconf.h 

# minimal
ecap_adapter_minimal_la_SOURCES = adapter_minimal.cc
ecap_adapter_minimal_la_LDFLAGS = -module -avoid-version $(libecap_LIBS) -lmysqlpp -lmysqlclient

# passthru
ecap_adapter_passthru_la_SOURCES = adapter_passthru.cc
ecap_adapter_passthru_la_LDFLAGS = -module -avoid-version $(libecap_LIBS)

# modifying
ecap_adapter_modifying_la_SOURCES = adapter_modifying.cc
ecap_adapter_modifying_la_LDFLAGS = -module -avoid-version $(libecap_LIBS)

# captivating
ecap_adapter_captivating_la_SOURCES = adapter_captivating.cc
ecap_adapter_captivating_la_LDFLAGS = -module -avoid-version $(libecap_LIBS) -lmysqlpp -lmysqlclient

# -shared -export-dynamic -Wl,-soname,ecap_noop_adapter.so

DISTCLEANFILES = \
        autoconf.h

AM_CPPFLAGS = -I$(top_srcdir)/src $(libecap_CFLAGS)

