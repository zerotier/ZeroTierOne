# ##############################################################################
# AUTOMATICALLY GENERATED FILE -- DO NOT EDIT.
#
# This file is generated automatically by libpqxx's template2mak.py script, and
# will be rewritten from time to time.
#
# If you modify this file, chances are your modifications will be lost.
#
# The template2mak.py script should be available in the tools directory of the
# libpqxx source archive.
#
# Generated from template './test/Makefile.am.template'.
# ##############################################################################
# Makefile.am is generated automatically for automake whenever test programs are
# added to libpqxx.

EXTRA_DIST = Makefile.am.template

# Use the serial test runner, so tests don't get run in parallel.  Otherwise,
# output from test failures will be hidden away in log files where we can't
# see them when running in a build slave.
AUTOMAKE_OPTIONS=serial-tests


AM_CPPFLAGS=-I$(top_builddir)/include -I$(top_srcdir)/include

# Override automatically generated list of default includes.  It contains only
# unnecessary entries, and incorrectly mentions include/pqxx directly.
DEFAULT_INCLUDES=

noinst_HEADERS = test_helpers.hxx

CLEANFILES=pqxxlo.txt
MAINTAINERCLEANFILES=Makefile.in

#TESTS_ENVIRONMENT=PGDATABASE=libpqxx
# PGDATABASE, PGHOST, PGPORT, PGUSER

runner_SOURCES = \
  test00.cxx \
  test01.cxx \
  test02.cxx \
  test04.cxx \
  test07.cxx \
  test10.cxx \
  test11.cxx \
  test13.cxx \
  test14.cxx \
  test16.cxx \
  test17.cxx \
  test18.cxx \
  test20.cxx \
  test21.cxx \
  test26.cxx \
  test29.cxx \
  test30.cxx \
  test32.cxx \
  test37.cxx \
  test39.cxx \
  test46.cxx \
  test56.cxx \
  test60.cxx \
  test61.cxx \
  test62.cxx \
  test69.cxx \
  test70.cxx \
  test71.cxx \
  test72.cxx \
  test74.cxx \
  test75.cxx \
  test76.cxx \
  test77.cxx \
  test78.cxx \
  test79.cxx \
  test82.cxx \
  test84.cxx \
  test87.cxx \
  test88.cxx \
  test89.cxx \
  test90.cxx \
  unit/test_array.cxx \
  unit/test_binarystring.cxx \
  unit/test_blob.cxx \
  unit/test_cancel_query.cxx \
  unit/test_column.cxx \
  unit/test_composite.cxx \
  unit/test_connection.cxx \
  unit/test_cursor.cxx \
  unit/test_encodings.cxx \
  unit/test_error_verbosity.cxx \
  unit/test_errorhandler.cxx \
  unit/test_escape.cxx \
  unit/test_exceptions.cxx \
  unit/test_field.cxx \
  unit/test_float.cxx \
  unit/test_largeobject.cxx \
  unit/test_nonblocking_connect.cxx \
  unit/test_notification.cxx \
  unit/test_pipeline.cxx \
  unit/test_prepared_statement.cxx \
  unit/test_range.cxx \
  unit/test_read_transaction.cxx \
  unit/test_result_iteration.cxx \
  unit/test_result_slicing.cxx \
  unit/test_row.cxx \
  unit/test_separated_list.cxx \
  unit/test_simultaneous_transactions.cxx \
  unit/test_sql_cursor.cxx \
  unit/test_stateless_cursor.cxx \
  unit/test_strconv.cxx \
  unit/test_stream_from.cxx \
  unit/test_stream_to.cxx \
  unit/test_string_conversion.cxx \
  unit/test_subtransaction.cxx \
  unit/test_test_helpers.cxx \
  unit/test_thread_safety_model.cxx \
  unit/test_time.cxx \
  unit/test_transaction.cxx \
  unit/test_transaction_base.cxx \
  unit/test_transaction_focus.cxx \
  unit/test_transactor.cxx \
  unit/test_type_name.cxx \
  unit/test_zview.cxx \
  runner.cxx

runner_LDADD = $(top_builddir)/src/libpqxx.la ${POSTGRES_LIB}

TESTS = runner
check_PROGRAMS = ${TESTS}
