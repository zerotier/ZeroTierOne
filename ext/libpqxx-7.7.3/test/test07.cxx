#include <iostream>
#include <map>

#include <pqxx/transaction>
#include <pqxx/transactor>

#include "test_helpers.hxx"

using namespace pqxx;


// Example program for libpqxx.  Modify the database, retaining transactional
// integrity using the transactor framework.
//
// This assumes the existence of a database table "pqxxevents" containing a
// 2-digit "year" field, which is extended to a 4-digit format by assuming all
// year numbers of 70 or higher are in the 20th century, and all others in the
// 21st, and that no years before 1970 are possible.

namespace
{
// Convert year to 4-digit format.
int To4Digits(int Y)
{
  int Result{Y};

  PQXX_CHECK(Y >= 0, "Negative year: " + to_string(Y));
  if (Y < 70)
    Result += 2000;
  else if (Y < 100)
    Result += 1900;
  else if (Y < 1970)
    PQXX_CHECK_NOTREACHED("Unexpected year: " + to_string(Y));

  return Result;
}


void test_007()
{
  connection conn;
  conn.set_client_encoding("SQL_ASCII");

  {
    work tx{conn};
    test::create_pqxxevents(tx);
    tx.commit();
  }

  // Perform (an instantiation of) the UpdateYears transactor we've defined
  // in the code above.  This is where the work gets done.
  std::map<int, int> conversions;
  perform([&conversions, &conn] {
    work tx{conn};
    // First select all different years occurring in the table.
    result R(tx.exec("SELECT year FROM pqxxevents"));

    // See if we get reasonable type identifier for this column.
    oid const rctype{R.column_type(0)};
    PQXX_CHECK_EQUAL(
      R.column_type(pqxx::row::size_type(0)), rctype,
      "Inconsistent result::column_type().");

    std::string const rct{to_string(rctype)};
    PQXX_CHECK(rctype > 0, "Got strange type ID for column: " + rct);

    std::string const rcol{R.column_name(0)};
    PQXX_CHECK(not std::empty(rcol), "Didn't get a name for column.");

    oid const rcctype{R.column_type(rcol)};
    PQXX_CHECK_EQUAL(
      rcctype, rctype, "Column type is not what it is by name.");

    oid const rawrcctype{R.column_type(rcol)};
    PQXX_CHECK_EQUAL(
      rawrcctype, rctype, "Column type by C-style name is different.");

    // Note all different years currently occurring in the table, writing
    // them and their correct mappings to conversions.
    for (auto const &r : R)
    {
      int Y{0};

      // Read year, and if it is non-null, note its converted value
      if (r[0] >> Y)
        conversions[Y] = To4Digits(Y);

      // See if type identifiers are consistent
      oid const tctype{r.column_type(0)};

      PQXX_CHECK_EQUAL(
        tctype, r.column_type(pqxx::row::size_type(0)),
        "Inconsistent pqxx::row::column_type()");

      PQXX_CHECK_EQUAL(
        tctype, rctype,
        "pqxx::row::column_type() is inconsistent with "
        "result::column_type().");

      oid const ctctype{r.column_type(rcol)};

      PQXX_CHECK_EQUAL(
        ctctype, rctype, "Column type lookup by column name is broken.");

      oid const rawctctype{r.column_type(rcol)};

      PQXX_CHECK_EQUAL(
        rawctctype, rctype, "Column type lookup by C-style name is broken.");

      oid const fctype{r[0].type()};
      PQXX_CHECK_EQUAL(fctype, rctype, "Field type lookup is broken.");
    }

    // For each occurring year, write converted date back to whereever it may
    // occur in the table.  Since we're in a transaction, any changes made by
    // others at the same time will not affect us.
    for (auto const &c : conversions)
    {
      auto const query{
        "UPDATE pqxxevents "
        "SET year=" +
        to_string(c.second) +
        " "
        "WHERE year=" +
        to_string(c.first)};
      R = tx.exec0(query);
    }
  });
}


PQXX_REGISTER_TEST(test_007);
} // namespace
