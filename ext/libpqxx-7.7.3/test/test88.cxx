#include <iostream>

#include <pqxx/subtransaction>
#include <pqxx/transaction>

#include "test_helpers.hxx"


// Test program for libpqxx.  Attempt to perform nested transactions.
namespace
{
void test_088()
{
  pqxx::connection conn;

  pqxx::work tx0{conn};
  pqxx::test::create_pqxxevents(tx0);

  // Trivial test: create subtransactions, and commit/abort
  std::cout << tx0.exec1("SELECT 'tx0 starts'")[0].c_str() << std::endl;

  pqxx::subtransaction T0a(static_cast<pqxx::dbtransaction &>(tx0), "T0a");
  T0a.commit();

  pqxx::subtransaction T0b(static_cast<pqxx::dbtransaction &>(tx0), "T0b");
  T0b.abort();
  std::cout << tx0.exec1("SELECT 'tx0 ends'")[0].c_str() << std::endl;
  tx0.commit();

  // Basic functionality: perform query in subtransaction; abort, continue
  pqxx::work tx1{conn, "tx1"};
  std::cout << tx1.exec1("SELECT 'tx1 starts'")[0].c_str() << std::endl;
  pqxx::subtransaction tx1a{tx1, "tx1a"};
  std::cout << tx1a.exec1("SELECT '  a'")[0].c_str() << std::endl;
  tx1a.commit();
  pqxx::subtransaction tx1b{tx1, "tx1b"};
  std::cout << tx1b.exec1("SELECT '  b'")[0].c_str() << std::endl;
  tx1b.abort();
  pqxx::subtransaction tx1c{tx1, "tx1c"};
  std::cout << tx1c.exec1("SELECT '  c'")[0].c_str() << std::endl;
  tx1c.commit();
  std::cout << tx1.exec1("SELECT 'tx1 ends'")[0].c_str() << std::endl;
  tx1.commit();

  // Commit/rollback functionality
  pqxx::work tx2{conn, "tx2"};
  std::string const Table{"test088"};
  tx2.exec0("CREATE TEMP TABLE " + Table + "(no INTEGER, text VARCHAR)");

  tx2.exec0("INSERT INTO " + Table + " VALUES(1,'tx2')");

  pqxx::subtransaction tx2a{tx2, "tx2a"};
  tx2a.exec0("INSERT INTO " + Table + " VALUES(2,'tx2a')");
  tx2a.commit();
  pqxx::subtransaction tx2b{tx2, "tx2b"};
  tx2b.exec0("INSERT INTO " + Table + " VALUES(3,'tx2b')");
  tx2b.abort();
  pqxx::subtransaction tx2c{tx2, "tx2c"};
  tx2c.exec0("INSERT INTO " + Table + " VALUES(4,'tx2c')");
  tx2c.commit();
  auto const R{tx2.exec("SELECT * FROM " + Table + " ORDER BY no")};
  for (auto const &i : R)
    std::cout << '\t' << i[0].c_str() << '\t' << i[1].c_str() << std::endl;

  PQXX_CHECK_EQUAL(std::size(R), 3, "Wrong number of results.");

  int expected[3]{1, 2, 4};
  for (pqxx::result::size_type n{0}; n < std::size(R); ++n)
    PQXX_CHECK_EQUAL(
      R[n][0].as<int>(), expected[n], "Hit unexpected row number.");

  tx2.abort();

  // Auto-abort should only roll back the subtransaction.
  pqxx::work tx3{conn, "tx3"};
  pqxx::subtransaction tx3a(tx3, "tx3a");
  PQXX_CHECK_THROWS(
    tx3a.exec("SELECT * FROM nonexistent_table WHERE nonattribute=0"),
    pqxx::sql_error, "Bogus query did not fail.");

  // Subtransaction can only be aborted now, because there was an error.
  tx3a.abort();
  // We're back in our top-level transaction.  This did not abort.
  tx3.exec1("SELECT count(*) FROM pqxxevents");
  // Make sure we can commit exactly one more level of transaction.
  tx3.commit();
}
} // namespace


PQXX_REGISTER_TEST(test_088);
