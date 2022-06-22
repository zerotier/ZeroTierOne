#include <iostream>

#include <pqxx/nontransaction>
#include <pqxx/pipeline>
#include <pqxx/transaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Example program for libpqxx.  Test session variable functionality.
namespace
{
std::string GetDatestyle(connection &conn)
{
  return conn.get_var("DATESTYLE");
}


std::string SetDatestyle(connection &conn, std::string style)
{
  conn.set_session_var("DATESTYLE", style);
  std::string const fullname{GetDatestyle(conn)};
  PQXX_CHECK(
    not std::empty(fullname),
    "Setting datestyle to " + style + " makes it an empty string.");

  return fullname;
}


void CheckDatestyle(connection &conn, std::string expected)
{
  PQXX_CHECK_EQUAL(GetDatestyle(conn), expected, "Got wrong datestyle.");
}


void RedoDatestyle(
  connection &conn, std::string const &style, std::string const &expected)
{
  PQXX_CHECK_EQUAL(
    SetDatestyle(conn, style), expected, "Set wrong datestyle.");
}


void ActivationTest(
  connection &conn, std::string const &style, std::string const &expected)
{
  RedoDatestyle(conn, style, expected);
  CheckDatestyle(conn, expected);
}


void test_060()
{
  connection conn;

  PQXX_CHECK(not std::empty(GetDatestyle(conn)), "Initial datestyle not set.");

  std::string const ISOname{SetDatestyle(conn, "ISO")};
  std::string const SQLname{SetDatestyle(conn, "SQL")};

  PQXX_CHECK_NOT_EQUAL(ISOname, SQLname, "Same datestyle in SQL and ISO.");

  RedoDatestyle(conn, "SQL", SQLname);

  ActivationTest(conn, "ISO", ISOname);
  ActivationTest(conn, "SQL", SQLname);

  PQXX_CHECK_THROWS(
    conn.set_session_var("bonjour_name", std::optional<std::string>{}),
    pqxx::variable_set_to_null,
    "Setting a variable to null did not report the error correctly.");

  // Prove that setting an unknown variable causes an error, as expected
  quiet_errorhandler d{conn};
  PQXX_CHECK_THROWS(
    conn.set_session_var("NONEXISTENT_VARIABLE_I_HOPE", 1), sql_error,
    "Setting unknown variable failed to fail.");
}


PQXX_REGISTER_TEST(test_060);
} // namespace
