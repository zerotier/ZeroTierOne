#include <pqxx/time>
#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
#if defined(PQXX_HAVE_YEAR_MONTH_DAY)
using namespace std::literals;


void test_date_string_conversion()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  std::tuple<int, unsigned, unsigned, std::string_view> const conversions[]{
    {-542, 1, 1, "0543-01-01 BC"sv},
    {-1, 2, 3, "0002-02-03 BC"sv},
    {0, 9, 14, "0001-09-14 BC"sv},
    {1, 12, 8, "0001-12-08"sv},
    {2021, 10, 24, "2021-10-24"sv},
    {10191, 8, 30, "10191-08-30"sv},
    {-4712, 1, 1, "4713-01-01 BC"sv},
    {32767, 12, 31, "32767-12-31"sv},
    {2000, 2, 29, "2000-02-29"sv},
    {2004, 2, 29, "2004-02-29"sv},
    // This one won't work in postgres, but we can test the conversions.
    {-32767, 11, 3, "32768-11-03 BC"sv},
  };
  for (auto const &[y, m, d, text] : conversions)
  {
    std::chrono::year_month_day const date{
      std::chrono::year{y}, std::chrono::month{m}, std::chrono::day{d}};
    PQXX_CHECK_EQUAL(
      pqxx::to_string(date), text, "Date did not convert right.");
    PQXX_CHECK_EQUAL(
      pqxx::from_string<std::chrono::year_month_day>(text), date,
      "Date did not parse right.");
    if (int{date.year()} > -4712)
    {
      // We can't test this for years before 4713 BC (4712 BCE), because
      // postgres doesn't handle earlier years.
      PQXX_CHECK_EQUAL(
        tx.query_value<std::string>(
          "SELECT '" + pqxx::to_string(date) + "'::date"),
        text, "Backend interpreted date differently.");
    }
  }

  std::string_view const invalid[]{
    ""sv,
    "yesterday"sv,
    "1981-01"sv,
    "2010"sv,
    "2010-8-9"sv,
    "1900-02-29"sv,
    "2021-02-29"sv,
    "2000-11-29-3"sv,
    "1900-02-29"sv,
    "2003-02-29"sv,
    "12-12-12"sv,
    "0000-09-16"sv,
    "-01-01"sv,
    "-1000-01-01"sv,
    "1000-00-01"sv,
    "1000-01-00"sv,
    "2001y-01-01"sv,
    "10-09-08"sv,
    "0-01-01"sv,
    "0000-01-01"sv,
    "2021-13-01"sv,
    "2021-+02-01"sv,
    "2021-12-32"sv,
  };
  for (auto const text : invalid)
    PQXX_CHECK_THROWS(
      pqxx::ignore_unused(
        pqxx::from_string<std::chrono::year_month_day>(text)),
      pqxx::conversion_error,
      pqxx::internal::concat("Invalid date '", text, "' parsed as if valid."));
}


PQXX_REGISTER_TEST(test_date_string_conversion);
#endif // PQXX_HAVE_YEAR_MONTH_DAY
} // namespace
