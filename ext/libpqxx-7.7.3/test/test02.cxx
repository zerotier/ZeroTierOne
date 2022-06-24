#include <pqxx/transaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Example/test program for libpqxx.  Perform a query and enumerate its output
// using array indexing.

namespace
{
void bad_connect()
{
  connection conn{"totally#invalid@connect$string!?"};
}

void test_002()
{
  // Before we really connect, test the expected behaviour of the default
  // connection type, where a failure to connect results in an immediate
  // exception rather than a silent retry.
  PQXX_CHECK_THROWS_EXCEPTION(
    bad_connect(), "Invalid connection string did not cause exception.");

  // Set up connection to database
  std::string ConnectString;
  connection C{ConnectString};

  // Start transaction within context of connection.
  work T{C, "test2"};

  // Perform query within transaction.
  result R(T.exec("SELECT * FROM pg_tables"));

  // Let's keep the database waiting as briefly as possible: commit now,
  // before we start processing results.  We could do this later, or since
  // we're not making any changes in the database that need to be committed,
  // we could in this case even omit it altogether.
  T.commit();

  // Ah, this version of postgres will tell you which table a column in a
  // result came from.  Let's just test that functionality...
  oid const rtable{R.column_table(0)};
  PQXX_CHECK_EQUAL(
    rtable, R.column_table(pqxx::row::size_type(0)),
    "Inconsistent answers from column_table()");

  std::string const rcol{R.column_name(0)};
  oid const crtable{R.column_table(rcol)};
  PQXX_CHECK_EQUAL(
    crtable, rtable, "Field looked up by name gives different origin.");

  // Now we've got all that settled, let's process our results.
  for (auto const &f : R)
  {
    oid const ftable{f[0].table()};
    PQXX_CHECK_EQUAL(ftable, rtable, "field::table() is broken.");

    oid const ttable{f.column_table(0)};

    PQXX_CHECK_EQUAL(
      ttable, f.column_table(pqxx::row::size_type(0)),
      "Inconsistent pqxx::row::column_table().");

    PQXX_CHECK_EQUAL(ttable, rtable, "Inconsistent result::column_table().");

    oid const cttable{f.column_table(rcol)};

    PQXX_CHECK_EQUAL(cttable, rtable, "pqxx::row::column_table() is broken.");
  }
}


PQXX_REGISTER_TEST(test_002);
} // namespace
