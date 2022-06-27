#include <cmath>

#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
/// Test conversions for some floating-point type.
template<typename T> void infinity_test()
{
  T inf{std::numeric_limits<T>::infinity()};
  std::string inf_string;
  T back_conversion;

  inf_string = pqxx::to_string(inf);
  pqxx::from_string(inf_string, back_conversion);
  PQXX_CHECK_LESS(
    T(999999999), back_conversion,
    "Infinity doesn't convert back to something huge.");

  inf_string = pqxx::to_string(-inf);
  pqxx::from_string(inf_string, back_conversion);
  PQXX_CHECK_LESS(
    back_conversion, -T(999999999), "Negative infinity is broken");
}

void test_infinities()
{
  infinity_test<float>();
  infinity_test<double>();
  infinity_test<long double>();
}


/// Reproduce bug #262: repeated float conversions break without charconv.
template<typename T> void bug_262()
{
  pqxx::connection conn;
  conn.prepare("stmt", "select cast($1 as float)");
  pqxx::work tr{conn};

  // We must use the same float type both for passing the value to the
  // statement and for retrieving result of the statement execution.  This is
  // due to an internal stringstream being instantiated as a a parameterized
  // thread-local singleton.  So, there are separate stream<float>,
  // stream<double>, stream<long double>, but every such instance is a
  // singleton. We should use only one of them for this test.

  pqxx::row row;

  // Nothing bad here, select a float value.
  // The stream is clear, so just fill it with the value and extract str().
  row = tr.exec1("SELECT 1.0");

  // This works properly, but as we parse the value from the stream, the
  // seeking cursor moves towards the EOF. When the inevitable EOF happens
  // 'eof' flag is set in the stream and 'good' flag is unset.
  row[0].as<T>();

  // The second try. Select a float value again. The stream is not clean, so
  // we need to put an empty string into its buffer {stream.str("");}. This
  // resets the seeking cursor to 0. Then we will put the value using
  // operator<<().
  // ...
  // ...
  // OOPS. stream.str("") does not reset 'eof' flag and 'good' flag! We are
  // trying to read from EOF! This is no good.
  // Throws on unpatched pqxx v6.4.5
  row = tr.exec1("SELECT 2.0");

  // We won't get here without patch. The following statements are just for
  // demonstration of how are intended to work. If we
  // simply just reset the stream flags properly, this would work fine.
  // The most obvious patch is just explicitly stream.seekg(0).
  row[0].as<T>();
  row = tr.exec1("SELECT 3.0");
  row[0].as<T>();
}


/// Test for bug #262.
void test_bug_262()
{
  bug_262<float>();
  bug_262<double>();
  bug_262<long double>();
}


/// Test conversion of malformed floating-point values.
void test_bad_float()
{
  float x [[maybe_unused]];
  PQXX_CHECK_THROWS(
    x = pqxx::from_string<float>(""), pqxx::conversion_error,
    "Conversion of empty string to float was not caught.");

  PQXX_CHECK_THROWS(
    x = pqxx::from_string<float>("Infancy"), pqxx::conversion_error,
    "Misleading infinity was not caught.");
  PQXX_CHECK_THROWS(
    x = pqxx::from_string<float>("-Infighting"), pqxx::conversion_error,
    "Misleading negative infinity was not caught.");

  PQXX_CHECK_THROWS(
    x = pqxx::from_string<float>("Nanny"), pqxx::conversion_error,
    "Conversion of misleading NaN was not caught.");
}


template<typename T> void test_float_length(T value)
{
  auto const text{pqxx::to_string(value)};
  PQXX_CHECK_GREATER_EQUAL(
    pqxx::size_buffer(value), std::size(text) + 1,
    "Not enough buffer space for " + text + ".");
}


/// Test conversion of long float values to strings.
void test_long_float()
{
  test_float_length(0.1f);
  test_float_length(0.1);

  test_float_length(std::numeric_limits<float>::denorm_min());
  test_float_length(-std::numeric_limits<float>::denorm_min());
  test_float_length(std::numeric_limits<float>::min());
  test_float_length(-std::numeric_limits<float>::min());
  test_float_length(std::numeric_limits<float>::max());
  test_float_length(-std::numeric_limits<float>::max());
  test_float_length(-std::nextafter(1.0f, 2.0f));

  test_float_length(std::numeric_limits<double>::denorm_min());
  test_float_length(-std::numeric_limits<double>::denorm_min());
  test_float_length(std::numeric_limits<double>::min());
  test_float_length(-std::numeric_limits<double>::min());
  test_float_length(std::numeric_limits<double>::max());
  test_float_length(-std::numeric_limits<double>::max());
  test_float_length(-std::nextafter(1.0, 2.0));

  test_float_length(std::numeric_limits<long double>::denorm_min());
  test_float_length(-std::numeric_limits<long double>::denorm_min());
  test_float_length(std::numeric_limits<long double>::min());
  test_float_length(-std::numeric_limits<long double>::min());
  test_float_length(std::numeric_limits<long double>::max());
  test_float_length(-std::numeric_limits<long double>::max());
  test_float_length(-std::nextafter(1.0L, 2.0L));

  // Ahem.  I'm not proud of this.  We really can't assume much about the
  // floating-point types, but I'd really like to try a few things to see that
  // buffer sizes are in the right ballpark.  So, if "double" is at least 64
  // bits, check for some examples of long conversions.
  if constexpr (sizeof(double) >= 8)
  {
    auto constexpr awkward{-2.2250738585072014e-308};
    auto const text{pqxx::to_string(awkward)};
    PQXX_CHECK_LESS_EQUAL(
      std::size(text), 25u, text + " converted to too long a string.");
  }
  if constexpr (sizeof(double) <= 8)
  {
    auto const text{pqxx::to_string(0.99)};
    PQXX_CHECK_LESS_EQUAL(
      pqxx::size_buffer(0.99), 25u, text + " converted to too long a string.");
  }
}


PQXX_REGISTER_TEST(test_infinities);
PQXX_REGISTER_TEST(test_bug_262);
PQXX_REGISTER_TEST(test_bad_float);
PQXX_REGISTER_TEST(test_long_float);
} // namespace
