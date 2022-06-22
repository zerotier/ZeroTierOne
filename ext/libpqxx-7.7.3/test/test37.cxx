#include <pqxx/nontransaction>
#include <pqxx/robusttransaction>
#include <pqxx/transactor>

#include "test_helpers.hxx"

using namespace pqxx;


// Test program for libpqxx.  Verify abort behaviour of RobustTransaction.
//
// The program will attempt to add an entry to a table called "pqxxevents",
// with a key column called "year"--and then abort the change.
namespace
{
// Let's take a boring year that is not going to be in the "pqxxevents" table
constexpr int BoringYear{1977};

// Count events and specifically events occurring in Boring Year, leaving the
// former count in the result pair's first member, and the latter in second.
std::pair<int, int> count_events(connection &conn, std::string const &table)
{
  std::string const count_query{"SELECT count(*) FROM " + table};
  nontransaction tx{conn};
  return std::make_pair(
    tx.query_value<int>(count_query),
    tx.query_value<int>(count_query + " WHERE year=" + to_string(BoringYear)));
}


struct deliberate_error : std::exception
{};


void test_037()
{
  connection conn;
  {
    nontransaction tx{conn};
    test::create_pqxxevents(tx);
  }

  std::string const Table{"pqxxevents"};

  auto const Before{
    perform([&conn, &Table] { return count_events(conn, Table); })};
  PQXX_CHECK_EQUAL(
    Before.second, 0,
    "Already have event for " + to_string(BoringYear) + ", cannot test.");

  {
    quiet_errorhandler d(conn);
    PQXX_CHECK_THROWS(
      perform([&conn, &Table] {
        robusttransaction<> tx{conn};
        tx.exec0(
          "INSERT INTO " + Table + " VALUES (" + to_string(BoringYear) +
          ", "
          "'yawn')");

        throw deliberate_error();
      }),
      deliberate_error,
      "Did not get expected exception from failing transactor.");
  }

  auto const After{
    perform([&conn, &Table] { return count_events(conn, Table); })};

  PQXX_CHECK_EQUAL(After.first, Before.first, "Number of events changed.");
  PQXX_CHECK_EQUAL(
    After.second, Before.second,
    "Number of events for " + to_string(BoringYear) + " changed.");
}


PQXX_REGISTER_TEST(test_037);
} // namespace
