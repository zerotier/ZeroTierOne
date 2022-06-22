#include <pqxx/stream_from>
#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
auto make_focus(pqxx::dbtransaction &tx)
{
  return pqxx::stream_from::query(tx, "SELECT * from generate_series(1, 10)");
}


void test_cannot_run_statement_during_focus()
{
  pqxx::connection conn;
  pqxx::transaction tx{conn};
  tx.exec("SELECT 1");
  auto focus{make_focus(tx)};
  PQXX_CHECK_THROWS(
    tx.exec("SELECT 1"), pqxx::usage_error,
    "Command during focus did not throw expected error.");
}


void test_cannot_run_prepared_statement_during_focus()
{
  pqxx::connection conn;
  conn.prepare("foo", "SELECT 1");
  pqxx::transaction tx{conn};
  tx.exec_prepared("foo");
  auto focus{make_focus(tx)};
  PQXX_CHECK_THROWS(
    tx.exec_prepared("foo"), pqxx::usage_error,
    "Prepared statement during focus did not throw expected error.");
}

void test_cannot_run_params_statement_during_focus()
{
  pqxx::connection conn;
  pqxx::transaction tx{conn};
  tx.exec_params("select $1", 10);
  auto focus{make_focus(tx)};
  PQXX_CHECK_THROWS(
    tx.exec_params("select $1", 10), pqxx::usage_error,
    "Parameterized statement during focus did not throw expected error.");
}


PQXX_REGISTER_TEST(test_cannot_run_statement_during_focus);
PQXX_REGISTER_TEST(test_cannot_run_prepared_statement_during_focus);
PQXX_REGISTER_TEST(test_cannot_run_params_statement_during_focus);
} // namespace
