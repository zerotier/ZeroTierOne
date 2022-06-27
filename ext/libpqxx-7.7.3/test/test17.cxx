#include <iostream>

#include <pqxx/connection>
#include <pqxx/nontransaction>
#include <pqxx/transactor>

#include "test_helpers.hxx"

using namespace pqxx;


// Simple test program for libpqxx.  Open connection to database, start
// a dummy transaction to gain nontransactional access, and perform a query.
namespace
{
void test_017()
{
  connection conn;
  perform([&conn] {
    nontransaction tx{conn};
    auto const r{tx.exec("SELECT * FROM generate_series(1, 4)")};
    PQXX_CHECK_EQUAL(std::size(r), 4, "Weird query result.");
    tx.commit();
  });
}


PQXX_REGISTER_TEST(test_017);
} // namespace
