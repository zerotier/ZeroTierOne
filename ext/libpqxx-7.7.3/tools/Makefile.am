EXTRA_DIST = \
	extract_version \
	lint \
	rmlo.cxx \
	splitconfig \
	template2mak.py \
	pqxxthreadsafety.cxx

AM_CPPFLAGS=-I$(top_builddir)/include -I$(top_srcdir)/include ${POSTGRES_INCLUDE}
# Override automatically generated list of default includes.  It contains only
# unnecessary entries, and incorrectly mentions include/pqxx directly.
DEFAULT_INCLUDES=

noinst_PROGRAMS = rmlo pqxxthreadsafety

rmlo_SOURCES = rmlo.cxx
rmlo_LDADD = $(top_builddir)/src/libpqxx.la ${POSTGRES_LIB}

pqxxthreadsafety_SOURCES = pqxxthreadsafety.cxx
pqxxthreadsafety_LDADD = $(top_builddir)/src/libpqxx.la ${POSTGRES_LIB}
