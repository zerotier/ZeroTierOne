#include <pqxx/transaction>

#include "../test_helpers.hxx"

extern "C"
{
#include <libpq-fe.h>
}

namespace
{
void test_error_verbosity()
{
  PQXX_CHECK_EQUAL(
    static_cast<int>(pqxx::error_verbosity::terse),
    static_cast<int>(PQERRORS_TERSE),
    "error_verbosity enum should match PGVerbosity.");
  PQXX_CHECK_EQUAL(
    static_cast<int>(pqxx::error_verbosity::normal),
    static_cast<int>(PQERRORS_DEFAULT),
    "error_verbosity enum should match PGVerbosity.");
  PQXX_CHECK_EQUAL(
    static_cast<int>(pqxx::error_verbosity::verbose),
    static_cast<int>(PQERRORS_VERBOSE),
    "error_verbosity enum should match PGVerbosity.");

  pqxx::connection conn;
  pqxx::work tx{conn};
  conn.set_verbosity(pqxx::error_verbosity::terse);
  tx.exec1("SELECT 1");
  conn.set_verbosity(pqxx::error_verbosity::verbose);
  tx.exec1("SELECT 2");
}


PQXX_REGISTER_TEST(test_error_verbosity);
} // namespace
