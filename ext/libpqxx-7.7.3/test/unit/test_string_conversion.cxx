#include <cstdint>
#include <variant>

#include <pqxx/connection>
#include <pqxx/transaction>

#include "../test_helpers.hxx"

// Some enums with string conversions.
enum EnumA
{
  ea0,
  ea1,
  ea2
};
enum EnumB
{
  eb0,
  eb1,
  eb2
};
namespace pqxx
{
PQXX_DECLARE_ENUM_CONVERSION(EnumA);
PQXX_DECLARE_ENUM_CONVERSION(EnumB);
} // namespace pqxx


namespace
{
// "A minimal difference."
constexpr double thres{0.00001};


void test_string_conversion()
{
  PQXX_CHECK_EQUAL(
    "C string array", pqxx::to_string("C string array"),
    "C-style string constant does not convert to string properly.");

  char text_array[]{"C char array"};
  PQXX_CHECK_EQUAL(
    "C char array", pqxx::to_string(text_array),
    "C-style non-const char array does not convert to string properly.");

  char const *text_ptr{"C string pointer"};
  PQXX_CHECK_EQUAL(
    "C string pointer", pqxx::to_string(text_ptr),
    "C-style string pointer does not convert to string properly.");

  std::string const cxx_string{"C++ string"};
  PQXX_CHECK_EQUAL(
    "C++ string", pqxx::to_string(cxx_string),
    "C++-style string object does not convert to string properly.");

  PQXX_CHECK_EQUAL("0", pqxx::to_string(0), "Zero does not convert right.");
  PQXX_CHECK_EQUAL(
    "1", pqxx::to_string(1), "Basic integer does not convert right.");
  PQXX_CHECK_EQUAL("-1", pqxx::to_string(-1), "Negative numbers don't work.");
  PQXX_CHECK_EQUAL(
    "9999", pqxx::to_string(9999), "Larger numbers don't work.");
  PQXX_CHECK_EQUAL(
    "-9999", pqxx::to_string(-9999), "Larger negative numbers don't work.");

  int x;
  pqxx::from_string("0", x);
  PQXX_CHECK_EQUAL(0, x, "Zero does not parse right.");
  pqxx::from_string("1", x);
  PQXX_CHECK_EQUAL(1, x, "Basic integer does not parse right.");
  pqxx::from_string("-1", x);
  PQXX_CHECK_EQUAL(-1, x, "Negative numbers don't work.");
  pqxx::from_string("9999", x);
  PQXX_CHECK_EQUAL(9999, x, "Larger numbers don't work.");
  pqxx::from_string("-9999", x);
  PQXX_CHECK_EQUAL(-9999, x, "Larger negative numbers don't work.");

  // Bug #263 describes a case where this kind of overflow went undetected.
  if (sizeof(unsigned int) == 4)
  {
    std::uint32_t u;
    PQXX_CHECK_THROWS(
      pqxx::from_string("4772185884", u), pqxx::conversion_error,
      "Overflow not detected.");
  }

  // We can convert to and from long double.  The implementation may fall
  // back on a thread-local std::stringstream.  Each call does its own
  // cleanup, so the conversion works multiple times.
  constexpr long double ld1{123456789.25}, ld2{9876543210.5};
  constexpr char lds1[]{"123456789.25"}, lds2[]{"9876543210.5"};
  PQXX_CHECK_EQUAL(
    pqxx::to_string(ld1).substr(0, 12), lds1,
    "Wrong conversion from long double.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(ld2).substr(0, 12), lds2,
    "Wrong value on repeated conversion from long double.");
  long double ldi1, ldi2;
  pqxx::from_string(lds1, ldi1);
  PQXX_CHECK_BOUNDS(
    ldi1, ld1 - thres, ld1 + thres, "Wrong conversion to long double.");
  pqxx::from_string(lds2, ldi2);
  PQXX_CHECK_BOUNDS(
    ldi2, ld2 - thres, ld2 + thres,
    "Wrong repeated conversion to long double.");

  // We can define string conversions for enums.
  PQXX_CHECK_EQUAL(
    pqxx::to_string(ea0), "0", "Enum-to-string conversion is broken.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(eb0), "0",
    "Enum-to-string conversion is inconsistent between enum types.");
  PQXX_CHECK_EQUAL(
    pqxx::to_string(ea1), "1",
    "Enum-to-string conversion breaks for nonzero value.");

  EnumA ea;
  pqxx::from_string("2", ea);
  PQXX_CHECK_EQUAL(ea, ea2, "String-to-enum conversion is broken.");
}


void test_convert_variant_to_string()
{
  PQXX_CHECK_EQUAL(
    pqxx::to_string(std::variant<int, std::string>{99}), "99",
    "First variant field did not convert right.");

  PQXX_CHECK_EQUAL(
    pqxx::to_string(std::variant<int, std::string>{"Text"}), "Text",
    "Second variant field did not convert right.");
}


void test_integer_conversion()
{
  PQXX_CHECK_EQUAL(
    pqxx::from_string<int>("12"), 12, "Basic integer conversion failed.");
  PQXX_CHECK_EQUAL(
    pqxx::from_string<int>(" 12"), 12,
    "Leading whitespace confused integer conversion.");
  PQXX_CHECK_THROWS(
    pqxx::ignore_unused(pqxx::from_string<int>("")), pqxx::conversion_error,
    "Converting empty string to integer did not throw conversion error.");
  PQXX_CHECK_THROWS(
    pqxx::ignore_unused(pqxx::from_string<int>(" ")), pqxx::conversion_error,
    "Converting whitespace to integer did not throw conversion error.");
  PQXX_CHECK_EQUAL(
    pqxx::from_string<int>("-6"), -6,
    "Leading whitespace did not work with negative number.");
  PQXX_CHECK_THROWS(
    pqxx::ignore_unused(pqxx::from_string<int>("- 3")), pqxx::conversion_error,
    "A space between negation and number was not properly flagged.");
  PQXX_CHECK_THROWS(
    pqxx::ignore_unused(pqxx::from_string<int>("-")), pqxx::conversion_error,
    "Just a minus sign should not parse as an integer.");
}


void test_convert_null()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  PQXX_CHECK_EQUAL(
    tx.quote(nullptr), "NULL", "Null pointer did not come out as SQL 'null'.");
  PQXX_CHECK_EQUAL(
    tx.quote(std::nullopt), "NULL",
    "std::nullopt did not come out as SQL 'null'.");
  PQXX_CHECK_EQUAL(
    tx.quote(std::monostate{}), "NULL",
    "std::monostate did not come out as SQL 'null'.");
}


PQXX_REGISTER_TEST(test_string_conversion);
PQXX_REGISTER_TEST(test_convert_variant_to_string);
PQXX_REGISTER_TEST(test_integer_conversion);
PQXX_REGISTER_TEST(test_convert_null);
} // namespace
