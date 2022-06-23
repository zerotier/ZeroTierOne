#include <iostream>

#include <pqxx/transaction>

#include "test_helpers.hxx"

using namespace pqxx;


namespace
{
// Simple test program for libpqxx.  Open connection to database, start
// a transaction, and perform a query inside it.
void test_001()
{
  connection conn;

  // Begin a transaction acting on our current connection.  Give it a human-
  // readable name so the library can include it in error messages.
  work tx{conn, "test1"};

  // Perform a query on the database, storing result rows in R.
  result r(tx.exec("SELECT * FROM pg_tables"));

  // We're expecting to find some tables...
  PQXX_CHECK(not std::empty(r), "No tables found.");

  tx.commit();
}


PQXX_REGISTER_TEST(test_001);
} // namespace
