#include <pqxx/nontransaction>
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
constexpr int BoringYear{1977};

std::pair<int, int> count_events(connection &conn, std::string const &table)
{
  std::string const count_query{"SELECT count(*) FROM " + table};
  work tx{conn};
  return std::make_pair(
    tx.query_value<int>(count_query),
    tx.query_value<int>(count_query + " WHERE year=" + to_string(BoringYear)));
}


struct deliberate_error : std::exception
{};


void test_032()
{
  connection conn;
  {
    nontransaction tx{conn};
    test::create_pqxxevents(tx);
  }

  std::string const Table{"pqxxevents"};

  std::pair<int, int> const Before{
    perform([&conn, &Table] { return count_events(conn, Table); })};
  PQXX_CHECK_EQUAL(
    Before.second, 0,
    "Already have event for " + to_string(BoringYear) + ", cannot test.");

  {
    quiet_errorhandler d(conn);
    PQXX_CHECK_THROWS(
      perform([&conn, &Table] {
        work{conn}.exec0(
          "INSERT INTO " + Table + " VALUES (" + to_string(BoringYear) +
          ", "
          "'yawn')");
        throw deliberate_error();
      }),
      deliberate_error,
      "Did not get expected exception from failing transactor.");
  }

  std::pair<int, int> const After{
    perform([&conn, &Table] { return count_events(conn, Table); })};

  PQXX_CHECK_EQUAL(After.first, Before.first, "Event count changed.");
  PQXX_CHECK_EQUAL(
    After.second, Before.second,
    "Event count for " + to_string(BoringYear) + " changed.");
}


PQXX_REGISTER_TEST(test_032);
} // namespace
