#include <pqxx/nontransaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Test: nontransaction changes are committed immediately.
namespace
{
int BoringYear{1977};


void test_039()
{
  connection conn;
  nontransaction tx1{conn};
  test::create_pqxxevents(tx1);
  std::string const Table{"pqxxevents"};

  // Verify our start condition before beginning: there must not be a 1977
  // record already.
  result R(tx1.exec(
    "SELECT * FROM " + Table +
    " "
    "WHERE year=" +
    to_string(BoringYear)));

  PQXX_CHECK_EQUAL(
    std::size(R), 0,
    "Already have a row for " + to_string(BoringYear) + ", cannot test.");

  // (Not needed, but verify that clear() works on empty containers)
  R.clear();
  PQXX_CHECK(std::empty(R), "Result is non-empty after clear().");

  // OK.  Having laid that worry to rest, add a record for 1977.
  tx1.exec0(
    "INSERT INTO " + Table +
    " VALUES"
    "(" +
    to_string(BoringYear) +
    ","
    "'Yawn'"
    ")");

  // Abort tx1.  Since tx1 is a nontransaction, which provides only the
  // transaction class interface without providing any form of transactional
  // integrity, this is not going to undo our work.
  tx1.abort();

  // Verify that our record was added, despite the Abort()
  nontransaction tx2(conn, "tx2");
  R = tx2.exec(
    "SELECT * FROM " + Table +
    " "
    "WHERE year=" +
    to_string(BoringYear));
  PQXX_CHECK_EQUAL(std::size(R), 1, "Unexpected result size.");

  PQXX_CHECK(R.capacity() >= std::size(R), "Result's capacity is too small.");

  R.clear();
  PQXX_CHECK(std::empty(R), "result::clear() is broken.");

  // Now remove our record again
  tx2.exec0(
    "DELETE FROM " + Table +
    " "
    "WHERE year=" +
    to_string(BoringYear));

  tx2.commit();

  // And again, verify results
  nontransaction tx3(conn, "tx3");

  R = tx3.exec(
    "SELECT * FROM " + Table +
    " "
    "WHERE year=" +
    to_string(BoringYear));

  PQXX_CHECK_EQUAL(std::size(R), 0, "Record is not gone as expected.");
}


PQXX_REGISTER_TEST(test_039);
} // namespace
