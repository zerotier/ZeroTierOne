/** Support for date/time values.
 *
 * At the moment this supports dates, but not times.
 */
#ifndef PQXX_H_TIME
#define PQXX_H_TIME

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <chrono>
#include <cstdlib>

#include "pqxx/internal/concat.hxx"
#include "pqxx/strconv.hxx"


#if defined(PQXX_HAVE_YEAR_MONTH_DAY)

namespace pqxx
{
using namespace std::literals;

template<>
struct nullness<std::chrono::year_month_day>
        : no_null<std::chrono::year_month_day>
{};


/// String representation for a Gregorian date in ISO-8601 format.
/** @warning Experimental.  There may still be design problems, particularly
 * when it comes to BC years.
 *
 * PostgreSQL supports a choice of date formats, but libpqxx does not.  The
 * other formats in turn support a choice of "month before day" versus "day
 * before month," meaning that it's not necessarily known which format a given
 * date is supposed to be.  So I repeat: ISO-8601-style format only!
 *
 * Invalid dates will not convert.  This includes February 29 on non-leap
 * years, which is why it matters that `year_month_day` represents a
 * _Gregorian_ date.
 *
 * The range of years is limited.  At the time of writing, PostgreSQL 14
 * supports years from 4713 BC to 294276 AD inclusive, and C++20 supports
 * a range of 32767 BC to 32767 AD inclusive.  So in practice, years must fall
 * between 4713 BC and 32767 AD, inclusive.
 *
 * @warning Support for BC (or BCE) years is still experimental.  I still need
 * confirmation on this issue: it looks as if C++ years are astronomical years,
 * which means they have a Year Zero.  Regular BC/AD years do not have a year
 * zero, so the year 1 AD follows directly after 1 BC.
 *
 * So, what to our calendars (and to PostgreSQL) is the year "0001 BC" seems to
 * count as year "0" in a `std::chrono::year_month_day`.  The year 0001 AD is
 * still equal to 1 as you'd expect, and all AD years work normally, but all
 * years before then are shifted by one.  For instance, the year 543 BC would
 * be -542 in C++.
 */
template<> struct PQXX_LIBEXPORT string_traits<std::chrono::year_month_day>
{
  [[nodiscard]] static zview
  to_buf(char *begin, char *end, std::chrono::year_month_day const &value)
  {
    return generic_to_buf(begin, end, value);
  }

  static char *
  into_buf(char *begin, char *end, std::chrono::year_month_day const &value);

  [[nodiscard]] static std::chrono::year_month_day
  from_string(std::string_view text);

  [[nodiscard]] static std::size_t
  size_buffer(std::chrono::year_month_day const &) noexcept
  {
    static_assert(int{(std::chrono::year::min)()} >= -99999);
    static_assert(int{(std::chrono::year::max)()} <= 99999);
    return 5 + 1 + 2 + 1 + 2 + std::size(s_bc) + 1;
  }

private:
  /// The "BC" suffix for years before 1 AD.
  static constexpr std::string_view s_bc{" BC"sv};
};
} // namespace pqxx
#endif // PQXX_HAVE_YEAR_MONTH_DAY
#endif
