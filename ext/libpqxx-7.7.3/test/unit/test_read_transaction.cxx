#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
void test_read_transaction()
{
  pqxx::connection conn;
  pqxx::read_transaction tx{conn};
  PQXX_CHECK_EQUAL(
    tx.exec("SELECT 1")[0][0].as<int>(), 1,
    "Bad result from read transaction.");

  PQXX_CHECK_THROWS(
    tx.exec("CREATE TABLE should_not_exist(x integer)"), pqxx::sql_error,
    "Read-only transaction allows database to be modified.");
}


PQXX_REGISTER_TEST(test_read_transaction);
} // namespace
