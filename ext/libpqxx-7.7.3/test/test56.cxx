#include <pqxx/transaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Simple test program for libpqxx.  Issue invalid query and handle error.
namespace
{
void test_056()
{
  connection conn;
  work tx{conn};
  quiet_errorhandler d(conn);

  PQXX_CHECK_THROWS(
    tx.exec("DELIBERATELY INVALID TEST QUERY..."), sql_error,
    "SQL syntax error did not raise expected exception.");
}


PQXX_REGISTER_TEST(test_056);
} // namespace
