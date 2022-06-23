#include <iostream>

#include <pqxx/connection>
#include <pqxx/nontransaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Test: nontransaction changes are not rolled back on abort.
namespace
{
constexpr unsigned long BoringYear{1977};


void test_020()
{
  connection conn;
  nontransaction t1{conn};
  test::create_pqxxevents(t1);

  std::string const Table{"pqxxevents"};

  // Verify our start condition before beginning: there must not be a 1977
  // record already.
  result R(t1.exec(("SELECT * FROM " + Table +
                    " "
                    "WHERE year=" +
                    to_string(BoringYear))
                     .c_str()));
  PQXX_CHECK_EQUAL(
    std::size(R), 0,
    "Already have a row for " + to_string(BoringYear) + ", cannot test.");

  // (Not needed, but verify that clear() works on empty containers)
  R.clear();
  PQXX_CHECK(std::empty(R), "result::clear() is broken.");

  // OK.  Having laid that worry to rest, add a record for 1977.
  t1.exec0(
    "INSERT INTO " + Table +
    " VALUES"
    "(" +
    to_string(BoringYear) +
    ","
    "'Yawn'"
    ")");

  // Abort T1.  Since T1 is a nontransaction, which provides only the
  // transaction class interface without providing any form of transactional
  // integrity, this is not going to undo our work.
  t1.abort();

  // Verify that our record was added, despite the Abort()
  nontransaction t2{conn, "t2"};
  R = t2.exec(("SELECT * FROM " + Table +
               " "
               "WHERE year=" +
               to_string(BoringYear))
                .c_str());

  PQXX_CHECK_EQUAL(
    std::size(R), 1,
    "Found wrong number of rows for " + to_string(BoringYear) + ".");

  PQXX_CHECK(R.capacity() >= std::size(R), "Result's capacity is too small.");

  R.clear();
  PQXX_CHECK(std::empty(R), "result::clear() doesn't work.");

  // Now remove our record again
  t2.exec0(
    "DELETE FROM " + Table +
    " "
    "WHERE year=" +
    to_string(BoringYear));

  t2.commit();

  // And again, verify results
  nontransaction t3{conn, "t3"};

  R = t3.exec(("SELECT * FROM " + Table +
               " "
               "WHERE year=" +
               to_string(BoringYear))
                .c_str());

  PQXX_CHECK_EQUAL(std::size(R), 0, "Record still found after removal.");
}


PQXX_REGISTER_TEST(test_020);
} // namespace
