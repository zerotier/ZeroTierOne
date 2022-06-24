#include <pqxx/transaction>
#include <pqxx/transactor>

#include "test_helpers.hxx"

using namespace pqxx;


// Test program for libpqxx.  Verify abort behaviour of transactor.
//
// The program will attempt to add an entry to a table called "pqxxevents",
// with a key column called "year"--and then abort the change.
//
// Note for the superstitious: the numbering for this test program is pure
// coincidence.
namespace
{
// Let's take a boring year that is not going to be in the "pqxxevents" table
constexpr unsigned int BoringYear = 1977;


// Count events and specifically events occurring in Boring Year, leaving the
// former count in the result pair's first member, and the latter in second.
std::pair<int, int> count_events(connection &conn, std::string const &table)
{
  work tx{conn};
  std::string const count_query{"SELECT count(*) FROM " + table};
  return std::make_pair(
    tx.query_value<int>(count_query),
    tx.query_value<int>(count_query + " WHERE year=" + to_string(BoringYear)));
}


struct deliberate_error : std::exception
{};


void failed_insert(connection &C, std::string const &table)
{
  work tx(C);
  result R = tx.exec0(
    "INSERT INTO " + table + " VALUES (" + to_string(BoringYear) +
    ", "
    "'yawn')");

  PQXX_CHECK_EQUAL(R.affected_rows(), 1, "Bad affected_rows().");
  throw deliberate_error();
}


void test_013()
{
  connection conn;
  {
    work tx{conn};
    test::create_pqxxevents(tx);
    tx.commit();
  }

  std::string const Table{"pqxxevents"};

  auto const Before{
    perform([&conn, &Table] { return count_events(conn, Table); })};
  PQXX_CHECK_EQUAL(
    Before.second, 0,
    "Already have event for " + to_string(BoringYear) + "--can't test.");

  quiet_errorhandler d(conn);
  PQXX_CHECK_THROWS(
    perform([&conn, &Table] { failed_insert(conn, Table); }), deliberate_error,
    "Failing transactor failed to throw correct exception.");

  auto const After{
    perform([&conn, &Table] { return count_events(conn, Table); })};

  PQXX_CHECK_EQUAL(
    After.first, Before.first, "abort() didn't reset event count.");

  PQXX_CHECK_EQUAL(
    After.second, Before.second,
    "abort() didn't reset event count for " + to_string(BoringYear));
}


PQXX_REGISTER_TEST(test_013);
} // namespace
