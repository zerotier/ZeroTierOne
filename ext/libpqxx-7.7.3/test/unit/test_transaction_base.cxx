#include <pqxx/stream_to>
#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
void test_exec0(pqxx::transaction_base &trans)
{
  pqxx::result E{trans.exec0("SELECT * FROM pg_tables WHERE 0 = 1")};
  PQXX_CHECK(std::empty(E), "Nonempty result from exec0.");

  PQXX_CHECK_THROWS(
    trans.exec0("SELECT 99"), pqxx::unexpected_rows,
    "Nonempty exec0 result did not throw unexpected_rows.");
}


void test_exec1(pqxx::transaction_base &trans)
{
  pqxx::row R{trans.exec1("SELECT 99")};
  PQXX_CHECK_EQUAL(std::size(R), 1, "Wrong size result from exec1.");
  PQXX_CHECK_EQUAL(R.front().as<int>(), 99, "Wrong result from exec1.");

  PQXX_CHECK_THROWS(
    trans.exec1("SELECT * FROM pg_tables WHERE 0 = 1"), pqxx::unexpected_rows,
    "Empty exec1 result did not throw unexpected_rows.");
  PQXX_CHECK_THROWS(
    trans.exec1("SELECT * FROM generate_series(1, 2)"), pqxx::unexpected_rows,
    "Two-row exec1 result did not throw unexpected_rows.");
}


void test_exec_n(pqxx::transaction_base &trans)
{
  pqxx::result R{trans.exec_n(3, "SELECT * FROM generate_series(1, 3)")};
  PQXX_CHECK_EQUAL(std::size(R), 3, "Wrong result size from exec_n.");

  PQXX_CHECK_THROWS(
    trans.exec_n(2, "SELECT * FROM generate_series(1, 3)"),
    pqxx::unexpected_rows,
    "exec_n did not throw unexpected_rows for an undersized result.");
  PQXX_CHECK_THROWS(
    trans.exec_n(4, "SELECT * FROM generate_series(1, 3)"),
    pqxx::unexpected_rows,
    "exec_n did not throw unexpected_rows for an oversized result.");
}


void test_query_value(pqxx::connection &conn)
{
  pqxx::work tx{conn};

  PQXX_CHECK_EQUAL(
    tx.query_value<int>("SELECT 84 / 2"), 42,
    "Got wrong value from query_value.");
  PQXX_CHECK_THROWS(
    tx.query_value<int>("SAVEPOINT dummy"), pqxx::unexpected_rows,
    "Got field when none expected.");
  PQXX_CHECK_THROWS(
    tx.query_value<int>("SELECT generate_series(1, 2)"), pqxx::unexpected_rows,
    "Failed to fail for multiple rows.");
  PQXX_CHECK_THROWS(
    tx.query_value<int>("SELECT 1, 2"), pqxx::usage_error,
    "No error for too many fields.");
  PQXX_CHECK_THROWS(
    tx.query_value<int>("SELECT 3.141"), pqxx::conversion_error,
    "Got int field from float string.");
}


void test_transaction_base()
{
  pqxx::connection conn;
  {
    pqxx::work tx{conn};
    test_exec_n(tx);
    test_exec0(tx);
    test_exec1(tx);
  }
  test_query_value(conn);
}


void test_transaction_for_each()
{
  constexpr auto query{
    "SELECT i, concat('x', (2*i)::text) "
    "FROM generate_series(1, 3) AS i "
    "ORDER BY i"};
  pqxx::connection conn;
  pqxx::work tx{conn};
  std::string ints;
  std::string strings;
  tx.for_each(query, [&ints, &strings](int i, std::string const &s) {
    ints += pqxx::to_string(i) + " ";
    strings += s + " ";
  });
  PQXX_CHECK_EQUAL(ints, "1 2 3 ", "Unexpected int sequence.");
  PQXX_CHECK_EQUAL(strings, "x2 x4 x6 ", "Unexpected string sequence.");
}


PQXX_REGISTER_TEST(test_transaction_base);
PQXX_REGISTER_TEST(test_transaction_for_each);
} // namespace
