/** Implementation of date/time support.
 */
#include "pqxx-source.hxx"

#include <cstdlib>

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/time.hxx"

#include "pqxx/internal/header-post.hxx"

// std::chrono::year_month_day is C++20, so let's worry a bit less about C++17
// compatibility in this file.
#if defined(PQXX_HAVE_YEAR_MONTH_DAY)
namespace
{
using namespace std::literals;


/// Render the numeric part of a year value into a buffer.
/** Converts the year from "common era" (with a Year Zero) to "anno domini"
 * (without a Year Zero).
 *
 * Doesn't render the sign.  When you're rendering a date, you indicate a
 * negative year by suffixing "BC" at the very end.
 *
 * Where @c string_traits::into_buf() returns a pointer to the position right
 * after the terminating zero, this function returns a pointer to the character
 * right after the last digit.  (It may or may not write a terminating zero at
 * that position itself.)
 */
inline char *
year_into_buf(char *begin, char *end, std::chrono::year const &value)
{
  int const y{value};
  if (y == int{(std::chrono::year::min)()})
  {
    // This is an evil special case: C++ year -32767 translates to 32768 BC,
    // which is a number we can't fit into a short.  At the moment postgres
    // doesn't handle years before 4713 BC, but who knows, right?
    static_assert(int{(std::chrono::year::min)()} == -32767);
    constexpr auto hardcoded{"32768"sv};
    PQXX_UNLIKELY
    begin += hardcoded.copy(begin, std::size(hardcoded));
  }
  else
  {
    // C++ std::chrono::year has a year zero.  PostgreSQL does not.  So, C++
    // year zero is 1 BC in the postgres calendar; C++ 1 BC is postgres 2 BC,
    // and so on.
    auto const absy{static_cast<short>(std::abs(y) + int{y <= 0})};

    // PostgreSQL requires year input to be at least 3 digits long, or it
    // won't be able to deduce the date format correctly.  However on output
    // it always writes years as at least 4 digits, and we'll do the same.
    // Dates and times are a dirty, dirty business.
    if (absy < 1000)
    {
      PQXX_UNLIKELY
      *begin++ = '0';
      if (absy < 100)
        *begin++ = '0';
      if (absy < 10)
        *begin++ = '0';
    }
    begin = pqxx::string_traits<short>::into_buf(begin, end, absy) - 1;
  }
  return begin;
}


/// Parse the numeric part of a year value.
inline int year_from_buf(std::string_view text)
{
  if (std::size(text) < 4)
    throw pqxx::conversion_error{
      pqxx::internal::concat("Year field is too small: '", text, "'.")};
  // Parse as int, so we can accommodate 32768 BC which won't fit in a short
  // as-is, but equates to 32767 BCE which will.
  int const year{pqxx::string_traits<int>::from_string(text)};
  if (year <= 0)
    throw pqxx::conversion_error{
      pqxx::internal::concat("Bad year: '", text, "'.")};
  return year;
}


/// Render a valid 1-based month number into a buffer.
/* Where @c string_traits::into_buf() returns a pointer to the position right
 * after the terminating zero, this function returns a pointer to the character
 * right after the last digit.  (It may or may not write a terminating zero at
 * that position itself.)
 */
inline static char *
month_into_buf(char *begin, std::chrono::month const &value)
{
  unsigned const m{value};
  if (m >= 10)
    *begin = '1';
  else
    *begin = '0';
  ++begin;
  *begin++ = pqxx::internal::number_to_digit(static_cast<int>(m % 10));
  return begin;
}


/// Parse a 1-based month value.
inline std::chrono::month month_from_string(std::string_view text)
{
  if (
    not pqxx::internal::is_digit(text[0]) or
    not pqxx::internal::is_digit(text[1]))
    throw pqxx::conversion_error{
      pqxx::internal::concat("Invalid month: '", text, "'.")};
  return std::chrono::month{unsigned(
    (10 * pqxx::internal::digit_to_number(text[0])) +
    pqxx::internal::digit_to_number(text[1]))};
}


/// Render a valid 1-based day-of-month value into a buffer.
inline char *day_into_buf(char *begin, std::chrono::day const &value)
{
  unsigned d{value};
  *begin++ = pqxx::internal::number_to_digit(static_cast<int>(d / 10));
  *begin++ = pqxx::internal::number_to_digit(static_cast<int>(d % 10));
  return begin;
}


/// Parse a 1-based day-of-month value.
inline std::chrono::day day_from_string(std::string_view text)
{
  if (
    not pqxx::internal::is_digit(text[0]) or
    not pqxx::internal::is_digit(text[1]))
    throw pqxx::conversion_error{
      pqxx::internal::concat("Bad day in date: '", text, "'.")};
  std::chrono::day const d{unsigned(
    (10 * pqxx::internal::digit_to_number(text[0])) +
    pqxx::internal::digit_to_number(text[1]))};
  if (not d.ok())
    throw pqxx::conversion_error{
      pqxx::internal::concat("Bad day in date: '", text, "'.")};
  return d;
}


/// Look for the dash separating year and month.
/** Assumes that @c text is nonempty.
 */
inline std::size_t find_year_month_separator(std::string_view text) noexcept
{
  // We're looking for a dash.  PostgreSQL won't output a negative year, so
  // no worries about a leading dash.  We could start searching at offset 4,
  // but starting at the beginning produces more helpful error messages for
  // malformed years.
  std::size_t here;
  for (here = 0; here < std::size(text) and text[here] != '-'; ++here)
    ;
  return here;
}


/// Componse generic "invalid date" message for given (invalid) date text.
std::string make_parse_error(std::string_view text)
{
  return pqxx::internal::concat("Invalid date: '", text, "'.");
}
} // namespace


namespace pqxx
{
char *string_traits<std::chrono::year_month_day>::into_buf(
  char *begin, char *end, std::chrono::year_month_day const &value)
{
  if (std::size_t(end - begin) < size_buffer(value))
    throw conversion_overrun{"Not enough room in buffer for date."};
  begin = year_into_buf(begin, end, value.year());
  *begin++ = '-';
  begin = month_into_buf(begin, value.month());
  *begin++ = '-';
  begin = day_into_buf(begin, value.day());
  if (int{value.year()} <= 0)
  {
    PQXX_UNLIKELY
    begin += s_bc.copy(begin, std::size(s_bc));
  }
  *begin++ = '\0';
  return begin;
}


std::chrono::year_month_day
string_traits<std::chrono::year_month_day>::from_string(std::string_view text)
{
  // We can't just re-use the std::chrono::year conversions, because the "BC"
  // suffix comes at the very end.
  if (std::size(text) < 9)
    throw conversion_error{make_parse_error(text)};
  bool const is_bc{text.ends_with(s_bc)};
  if (is_bc)
    PQXX_UNLIKELY
  text = text.substr(0, std::size(text) - std::size(s_bc));
  auto const ymsep{find_year_month_separator(text)};
  if ((std::size(text) - ymsep) != 6)
    throw conversion_error{make_parse_error(text)};
  auto const base_year{
    year_from_buf(std::string_view{std::data(text), ymsep})};
  if (base_year == 0)
    throw conversion_error{"Year zero conversion."};
  std::chrono::year const y{is_bc ? (-base_year + 1) : base_year};
  auto const m{month_from_string(text.substr(ymsep + 1, 2))};
  if (text[ymsep + 3] != '-')
    throw conversion_error{make_parse_error(text)};
  auto const d{day_from_string(text.substr(ymsep + 4, 2))};
  std::chrono::year_month_day const date{y, m, d};
  if (not date.ok())
    throw conversion_error{make_parse_error(text)};
  return date;
}
} // namespace pqxx
#endif
