#include <pqxx/nontransaction>

#include "test_helpers.hxx"


// Test program for libpqxx.  Test result::swap()
namespace
{
void test_077()
{
  pqxx::connection conn;
  pqxx::nontransaction tx{conn};

  auto RFalse{tx.exec("SELECT 1=0")}, RTrue{tx.exec("SELECT 1=1")};
  auto f{pqxx::from_string<bool>(RFalse[0][0])};
  auto t{pqxx::from_string<bool>(RTrue[0][0])};
  PQXX_CHECK(
    not f and t, "Booleans converted incorrectly; can't trust this test.");

  RFalse.swap(RTrue);
  f = pqxx::from_string<bool>(RFalse[0][0]);
  t = pqxx::from_string<bool>(RTrue[0][0]);
  PQXX_CHECK(f and not t, "result::swap() is broken.");
}
} // namespace


PQXX_REGISTER_TEST(test_077);
