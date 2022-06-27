#include <iostream>

#include <pqxx/connection>
#include <pqxx/robusttransaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Test robusttransaction.
namespace
{
void test_016()
{
  connection conn;
  robusttransaction<> tx{conn};
  result R{tx.exec("SELECT * FROM pg_tables")};

  result::const_iterator c;
  for (c = std::begin(R); c != std::end(R); ++c)
    ;

  // See if back() and row comparison work properly
  PQXX_CHECK(
    std::size(R) >= 2, "Not enough rows in pg_tables to test, sorry!");

  --c;

  PQXX_CHECK_EQUAL(
    c->size(), std::size(R.back()),
    "Size mismatch between row iterator and back().");

  std::string const nullstr;
  for (pqxx::row::size_type i{0}; i < c->size(); ++i)
    PQXX_CHECK_EQUAL(
      c[i].as(nullstr), R.back()[i].as(nullstr), "Value mismatch in back().");
  PQXX_CHECK(*c == R.back(), "Row equality is broken.");
  PQXX_CHECK(not(*c != R.back()), "Row inequality is broken.");

  tx.commit();
}


PQXX_REGISTER_TEST(test_016);
} // namespace
