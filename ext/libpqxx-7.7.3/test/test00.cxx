#include <locale>

#include <pqxx/cursor>
#include <pqxx/strconv>

#include "test_helpers.hxx"

using namespace pqxx;


// Initial test program for libpqxx.  Test functionality that doesn't require a
// running database.

namespace
{
template<typename T>
inline void
strconv(std::string const &type, T const &Obj, std::string const &expected)
{
  std::string const Objstr{to_string(Obj)};

  PQXX_CHECK_EQUAL(Objstr, expected, "String mismatch for " + type + ".");
  T NewObj;
  from_string(Objstr, NewObj);
  PQXX_CHECK_EQUAL(
    to_string(NewObj), expected, "String mismatch for recycled " + type + ".");
}

// There's no from_string<char const *>()...
inline void
strconv(std::string const &type, char const Obj[], std::string const &expected)
{
  std::string const Objstr(to_string(Obj));
  PQXX_CHECK_EQUAL(Objstr, expected, "String mismatch for " + type + ".");
}

constexpr double not_a_number{std::numeric_limits<double>::quiet_NaN()};

struct intderef
{
  template<typename ITER> int operator()(ITER i) const noexcept
  {
    return int(*i);
  }
};


void test_000()
{
  PQXX_CHECK_EQUAL(
    oid_none, 0u,
    "InvalidIod is not zero as it used to be.  This may conceivably "
    "cause problems in libpqxx.");

  PQXX_CHECK(
    cursor_base::prior() < 0 and cursor_base::backward_all() < 0,
    "cursor_base::difference_type appears to be unsigned.");

  constexpr char weird[]{"foo\t\n\0bar"};
  std::string const weirdstr(weird, std::size(weird) - 1);

  // Test string conversions
  strconv("char const[]", "", "");
  strconv("char const[]", "foo", "foo");
  strconv("int", 0, "0");
  strconv("int", 100, "100");
  strconv("int", -1, "-1");

#if defined(_MSC_VER)
  long const long_min{LONG_MIN}, long_max{LONG_MAX};
#else
  long const long_min{std::numeric_limits<long>::min()},
    long_max{std::numeric_limits<long>::max()};
#endif

  std::stringstream lminstr, lmaxstr, llminstr, llmaxstr, ullmaxstr;
  lminstr.imbue(std::locale("C"));
  lmaxstr.imbue(std::locale("C"));
  llminstr.imbue(std::locale("C"));
  llmaxstr.imbue(std::locale("C"));
  ullmaxstr.imbue(std::locale("C"));

  lminstr << long_min;
  lmaxstr << long_max;

  auto const ullong_max{std::numeric_limits<unsigned long long>::max()};
  auto const llong_max{std::numeric_limits<long long>::max()},
    llong_min{std::numeric_limits<long long>::min()};

  llminstr << llong_min;
  llmaxstr << llong_max;
  ullmaxstr << ullong_max;

  strconv("long", 0, "0");
  strconv("long", long_min, lminstr.str());
  strconv("long", long_max, lmaxstr.str());
  strconv("double", not_a_number, "nan");
  strconv("string", std::string{}, "");
  strconv("string", weirdstr, weirdstr);
  strconv("long long", 0LL, "0");
  strconv("long long", llong_min, llminstr.str());
  strconv("long long", llong_max, llmaxstr.str());
  strconv("unsigned long long", 0ULL, "0");
  strconv("unsigned long long", ullong_max, ullmaxstr.str());

  std::stringstream ss;
  strconv("empty stringstream", ss, "");
  ss << -3.1415;
  strconv("stringstream", ss, ss.str());
}


PQXX_REGISTER_TEST(test_000);
} // namespace
