#include <cstdio>
#include <cstring>
#include <iostream>

#include <pqxx/transaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Test program for libpqxx.  Query a table and report its metadata.
namespace
{
void test_030()
{
  std::string const Table{"pg_tables"};

  connection conn;
  work tx{conn, "test30"};

  result R(tx.exec(("SELECT * FROM " + Table).c_str()));
  PQXX_CHECK(not std::empty(R), "Table " + Table + " is empty, cannot test.");

  // Print column names
  for (pqxx::row::size_type c{0}; c < R.columns(); ++c)
  {
    std::string N{R.column_name(c)};

    PQXX_CHECK_EQUAL(
      R[0].column_number(N), R.column_number(N),
      "row::column_number() is inconsistent with result::column_number().");

    PQXX_CHECK_EQUAL(R[0].column_number(N), c, "Inconsistent column numbers.");
  }

  // If there are rows in R, compare their metadata to R's.
  if (std::empty(R))
  {
    std::cout << "(Table is empty.)\n";
    return;
  }

  PQXX_CHECK_EQUAL(R[0].rownumber(), 0, "Row 0 reports wrong number.");

  if (std::size(R) < 2)
    std::cout << "(Only one row in table.)\n";
  else
    PQXX_CHECK_EQUAL(R[1].rownumber(), 1, "Row 1 reports wrong number.");

  for (pqxx::row::size_type c{0}; c < std::size(R[0]); ++c)
  {
    std::string N{R.column_name(c)};

    PQXX_CHECK_EQUAL(
      std::string{R[0].at(c).c_str()}, R[0].at(N).c_str(),
      "Different field values by name and by number.");

    PQXX_CHECK_EQUAL(
      std::string{R[0][c].c_str()}, R[0][N].c_str(),
      "at() is inconsistent with operator[].");

    PQXX_CHECK_EQUAL(R[0][c].name(), N, "Inconsistent field names.");

    PQXX_CHECK_EQUAL(
      std::size(R[0][c]), std::strlen(R[0][c].c_str()),
      "Inconsistent field lengths.");
  }
}


PQXX_REGISTER_TEST(test_030);
} // namespace
