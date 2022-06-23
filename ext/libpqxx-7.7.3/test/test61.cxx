#include <iostream>

#include <pqxx/transaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Example program for libpqxx.  Test local variable functionality.
namespace
{
std::string GetDatestyle(transaction_base &T)
{
  return T.conn().get_var("DATESTYLE");
}


std::string SetDatestyle(transaction_base &T, std::string style)
{
  T.conn().set_session_var("DATESTYLE", style);
  std::string const fullname{GetDatestyle(T)};
  PQXX_CHECK(
    not std::empty(fullname),
    "Setting datestyle to " + style + " makes it an empty string.");

  return fullname;
}


void RedoDatestyle(
  transaction_base &T, std::string const &style, std::string const &expected)
{
  PQXX_CHECK_EQUAL(SetDatestyle(T, style), expected, "Set wrong datestyle.");
}


void test_061()
{
  connection conn;
  work tx{conn};

  PQXX_CHECK(not std::empty(GetDatestyle(tx)), "Initial datestyle not set.");

  std::string const ISOname{SetDatestyle(tx, "ISO")};
  std::string const SQLname{SetDatestyle(tx, "SQL")};

  PQXX_CHECK_NOT_EQUAL(ISOname, SQLname, "Same datestyle in SQL and ISO.");

  RedoDatestyle(tx, "SQL", SQLname);

  // Prove that setting an unknown variable causes an error, as expected
  quiet_errorhandler d(tx.conn());
  PQXX_CHECK_THROWS(
    conn.set_session_var("NONEXISTENT_VARIABLE_I_HOPE", 1), sql_error,
    "Setting unknown variable failed to fail.");
}


PQXX_REGISTER_TEST(test_061);
} // namespace
