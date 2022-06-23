#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
void test_field()
{
  pqxx::connection c;
  pqxx::work tx{c};
  auto const r1{tx.exec1("SELECT 9")};
  auto const &f1{r1[0]};

  PQXX_CHECK_EQUAL(f1.as<std::string>(), "9", "as<string>() is broken.");
  PQXX_CHECK_EQUAL(
    f1.as<std::string>("z"), "9", "as<string>(string) is broken.");

  PQXX_CHECK_EQUAL(f1.as<int>(), 9, "as<int>() is broken.");
  PQXX_CHECK_EQUAL(f1.as<int>(10), 9, "as<int>(int) is broken.");

  std::string s;
  PQXX_CHECK(f1.to(s), "to(string) failed.");
  PQXX_CHECK_EQUAL(s, "9", "to(string) is broken.");
  s = "x";
  PQXX_CHECK(f1.to(s, std::string{"7"}), "to(string, string) failed.");
  PQXX_CHECK_EQUAL(s, "9", "to(string, string) is broken.");

  int i{};
  PQXX_CHECK(f1.to(i), "to(int) failed.");
  PQXX_CHECK_EQUAL(i, 9, "to(int) is broken.");
  i = 8;
  PQXX_CHECK(f1.to(i, 12), "to(int, int) failed.");
  PQXX_CHECK_EQUAL(i, 9, "to(int, int) is broken.");

  auto const r2{tx.exec1("SELECT NULL")};
  auto const f2{r2[0]};
  i = 100;
  PQXX_CHECK_THROWS(
    f2.as<int>(), pqxx::conversion_error, "Null conversion failed to throw.");
  PQXX_CHECK_EQUAL(i, 100, "Null conversion touched its output.");

  PQXX_CHECK_EQUAL(f2.as<int>(66), 66, "as<int> default is broken.");

  PQXX_CHECK(!(f2.to(i)), "to(int) failed to report a null.");
  PQXX_CHECK(!(f2.to(i, 54)), "to(int, int) failed to report a null.");
  PQXX_CHECK_EQUAL(i, 54, "to(int, int) failed to default.");

  auto const r3{tx.exec("SELECT generate_series(1, 5)")};
  PQXX_CHECK_EQUAL(r3.at(3, 0).as<int>(), 4, "Two-argument at() went wrong.");
#if defined(PQXX_HAVE_MULTIDIMENSIONAL_SUBSCRIPT)
  PQXX_CHECK_EQUAL((r3[3, 0].as<int>()), 4, "Two-argument [] went wrong.");
#endif
}


PQXX_REGISTER_TEST(test_field);
} // namespace
