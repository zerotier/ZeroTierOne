lib_LTLIBRARIES = libpqxx.la
libpqxx_la_SOURCES = \
	array.cxx \
	binarystring.cxx \
	blob.cxx \
	connection.cxx \
	cursor.cxx \
	encodings.cxx \
	errorhandler.cxx \
	except.cxx \
	field.cxx \
	largeobject.cxx \
	notification.cxx \
	params.cxx \
	pipeline.cxx \
	result.cxx \
	robusttransaction.cxx \
	sql_cursor.cxx \
	strconv.cxx \
	stream_from.cxx \
	stream_to.cxx \
	subtransaction.cxx \
	time.cxx \
	transaction.cxx \
	transaction_base.cxx \
	row.cxx \
	util.cxx \
	version.cxx \
	wait.cxx

libpqxx_version = -release $(PQXX_ABI)

libpqxx_la_LDFLAGS = $(libpqxx_version) \
	-rpath $(libdir) \
	${POSTGRES_LIB}

AM_CPPFLAGS = \
    -I$(top_srcdir)/include -I$(top_builddir)/include ${POSTGRES_INCLUDE}

# Override automatically generated list of default includes.  It contains only
# unnecessary entries, and incorrectly mentions include/pqxx directly.
DEFAULT_INCLUDES=

MAINTAINERCLEANFILES=Makefile.in
