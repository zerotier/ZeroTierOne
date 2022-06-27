#include <cmath>

#include <pqxx/transaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Test program for libpqxx.  Test fieldstream.
namespace
{
void test_074()
{
  connection conn;
  work tx{conn};

  result R{tx.exec("SELECT * FROM pg_tables")};
  std::string const sval{R.at(0).at(1).c_str()};
  std::string sval2;
  fieldstream fs1(R.front()[1]);
  fs1 >> sval2;
  PQXX_CHECK_EQUAL(sval2, sval, "fieldstream returned wrong value.");

  R = tx.exec("SELECT count(*) FROM pg_tables");
  int ival;
  fieldstream fs2(R.at(0).at(0));
  fs2 >> ival;
  PQXX_CHECK_EQUAL(
    ival, R.front().front().as<int>(), "fieldstream::front() is broken.");

  double dval;
  (fieldstream(R.at(0).at(0))) >> dval;
  PQXX_CHECK_BOUNDS(
    dval, R[0][0].as<double>() - 0.1, R[0][0].as<double>() + 0.1,
    "Got wrong double from fieldstream.");

  auto const roughpi{static_cast<float>(3.1415926435)};
  R = tx.exec("SELECT " + to_string(roughpi));
  float pival;
  (fieldstream(R.at(0).at(0))) >> pival;
  PQXX_CHECK_BOUNDS(
    pival, roughpi - 0.001, roughpi + 0.001,
    "Pi approximation came back wrong from fieldstream.");

  PQXX_CHECK_EQUAL(
    to_string(R[0][0]), R[0][0].c_str(),
    "to_string(result::field) is inconsistent with c_str().");

  float float_pi;
  from_string(to_string(roughpi), float_pi);
  PQXX_CHECK_BOUNDS(
    float_pi, roughpi - 0.00001, roughpi + 0.00001,
    "Float changed in conversion.");

  double double_pi;
  pqxx::from_string(pqxx::to_string(static_cast<double>(roughpi)), double_pi);
  PQXX_CHECK_BOUNDS(
    double_pi, roughpi - 0.00001, roughpi + 0.00001,
    "Double changed in conversion.");

  long double const ld{roughpi};
  long double long_double_pi;
  from_string(to_string(ld), long_double_pi);
  PQXX_CHECK_BOUNDS(
    long_double_pi, roughpi - 0.00001, roughpi + 0.00001,
    "long double changed in conversion.");
}
} // namespace


PQXX_REGISTER_TEST(test_074);
