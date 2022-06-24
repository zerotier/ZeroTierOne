#include <cstdio>
#include <cstring>
#include <iostream>

#include <pqxx/transaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Test program for libpqxx.  Query a table and report its metadata.
namespace
{
void test_011()
{
  connection conn;
  work tx{conn};
  std::string const Table{"pg_tables"};

  result R(tx.exec("SELECT * FROM " + Table));

  // Print column names
  for (pqxx::row::size_type c{0}; c < R.columns(); ++c)
  {
    std::string N{R.column_name(c)};
    PQXX_CHECK_EQUAL(R.column_number(N), c, "Inconsistent column numbers.");
  }

  // If there are rows in R, compare their metadata to R's.
  if (not std::empty(R))
  {
    PQXX_CHECK_EQUAL(R[0].rownumber(), 0, "Row 0 has wrong number.");

    if (std::size(R) >= 2)
      PQXX_CHECK_EQUAL(R[1].rownumber(), 1, "Row 1 has wrong number.");

    // Test result::iterator::swap()
    pqxx::result::const_iterator const T1(R[0]), T2(R[1]);
    PQXX_CHECK_NOT_EQUAL(T1, T2, "Values are identical--can't test swap().");
    pqxx::result::const_iterator T1s(T1), T2s(T2);
    PQXX_CHECK_EQUAL(T1s, T1, "Result iterator copy-construction is wrong.");
    PQXX_CHECK_EQUAL(
      T2s, T2, "Result iterator copy-construction is inconsistently wrong.");
    T1s.swap(T2s);
    PQXX_CHECK_NOT_EQUAL(T1s, T1, "Result iterator swap doesn't work.");
    PQXX_CHECK_NOT_EQUAL(
      T2s, T2, "Result iterator swap inconsistently wrong.");
    PQXX_CHECK_EQUAL(T2s, T1, "Result iterator swap is asymmetric.");
    PQXX_CHECK_EQUAL(
      T1s, T2, "Result iterator swap is inconsistently asymmetric.");

    for (pqxx::row::size_type c{0}; c < std::size(R[0]); ++c)
    {
      std::string N{R.column_name(c)};

      PQXX_CHECK_EQUAL(
        std::string{R[0].at(c).c_str()}, R[0].at(N).c_str(),
        "Field by name != field by number.");

      PQXX_CHECK_EQUAL(
        std::string{R[0][c].c_str()}, R[0][N].c_str(),
        "at() is inconsistent with operator[].");

      PQXX_CHECK_EQUAL(R[0][c].name(), N, "Field names are inconsistent.");

      PQXX_CHECK_EQUAL(
        std::size(R[0][c]), strlen(R[0][c].c_str()),
        "Field size is not what we expected.");
    }
  }
}


PQXX_REGISTER_TEST(test_011);
} // namespace
