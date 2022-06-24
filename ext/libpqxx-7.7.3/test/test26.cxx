#include <iostream>
#include <map>

#include <pqxx/nontransaction>
#include <pqxx/transaction>
#include <pqxx/transactor>

#include "test_helpers.hxx"

using namespace pqxx;


// Example program for libpqxx.  Modify the database, retaining transactional
// integrity using the transactor framework.
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
  else
    PQXX_CHECK(Y >= 1970, "Unexpected year: " + to_string(Y));
  return Result;
}


// Transaction definition for year-field update.  Returns conversions done.
std::map<int, int> update_years(connection &C)
{
  std::map<int, int> conversions;
  work tx{C};

  // Note all different years currently occurring in the table, writing them
  // and their correct mappings to m_conversions
  for (auto const &[y] :
       tx.stream<std::optional<int>>("SELECT year FROM pqxxevents"))
  {
    // Read year, and if it is non-null, note its converted value
    if (bool(y))
      conversions[y.value()] = To4Digits(y.value());
  }

  // For each occurring year, write converted date back to whereever it may
  // occur in the table.  Since we're in a transaction, any changes made by
  // others at the same time will not affect us.
  for (auto const &c : conversions)
    tx.exec0(
      "UPDATE pqxxevents "
      "SET year=" +
      to_string(c.second) +
      " "
      "WHERE year=" +
      to_string(c.first));

  tx.commit();

  return conversions;
}


void test_026()
{
  connection conn;
  {
    nontransaction tx{conn};
    test::create_pqxxevents(tx);
    tx.commit();
  }

  // Perform (an instantiation of) the UpdateYears transactor we've defined
  // in the code above.  This is where the work gets done.
  auto const conversions{perform([&conn] { return update_years(conn); })};

  PQXX_CHECK(not std::empty(conversions), "No conversions done!");
}


PQXX_REGISTER_TEST(test_026);
} // namespace
