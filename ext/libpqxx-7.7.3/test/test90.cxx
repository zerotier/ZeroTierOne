#include <pqxx/connection>

#include "test_helpers.hxx"

// Test program for libpqxx.  Test adorn_name.

namespace
{
void test_090()
{
  pqxx::connection conn;

  // Test connection's adorn_name() function for uniqueness
  std::string const nametest{"basename"};

  PQXX_CHECK_NOT_EQUAL(
    conn.adorn_name(nametest), conn.adorn_name(nametest),
    "\"Unique\" names are not unique.");
}
} // namespace


PQXX_REGISTER_TEST(test_090);
