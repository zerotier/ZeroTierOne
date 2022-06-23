#include <pqxx/transaction>

#include "../test_helpers.hxx"

#include "pqxx/internal/ignore-deprecated-pre.hxx"

namespace pqxx
{
template<> struct nullness<row::const_iterator> : no_null<row::const_iterator>
{};

template<>
struct nullness<row::const_reverse_iterator>
        : no_null<const_reverse_row_iterator>
{};


template<> struct string_traits<row::const_iterator>
{
  static constexpr zview text{"[row::const_iterator]"};
  static zview to_buf(char *, char *, row::const_iterator const &)
  {
    return text;
  }
  static char *into_buf(char *begin, char *end, row::const_iterator const &)
  {
    if ((end - begin) <= 30)
      throw conversion_overrun{"Not enough buffer for const row iterator."};
    std::memcpy(begin, text.c_str(), std::size(text) + 1);
    return begin + std::size(text);
  }
  static constexpr std::size_t
  size_buffer(row::const_iterator const &) noexcept
  {
    return std::size(text) + 1;
  }
};


template<> struct string_traits<row::const_reverse_iterator>
{
  static constexpr zview text{"[row::const_reverse_iterator]"};
  static pqxx::zview
  to_buf(char *, char *, row::const_reverse_iterator const &)
  {
    return text;
  }
  static char *
  into_buf(char *begin, char *end, row::const_reverse_iterator const &)
  {
    if ((end - begin) <= 30)
      throw conversion_overrun{"Not enough buffer for const row iterator."};
    std::memcpy(begin, text.c_str(), std::size(text) + 1);
    return begin + std::size(text);
  }
  static constexpr std::size_t
  size_buffer(row::const_reverse_iterator const &) noexcept
  {
    return 100;
  }
};
} // namespace pqxx

namespace
{
void test_result_slicing()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  auto r{tx.exec("SELECT 1")};

  PQXX_CHECK(not std::empty(r[0]), "A plain row shows up as empty.");

  // Empty slice at beginning of row.
  pqxx::row s{r[0].slice(0, 0)};
  PQXX_CHECK(std::empty(s), "Empty slice does not show up as empty.");
  PQXX_CHECK_EQUAL(std::size(s), 0, "Slicing produces wrong row size.");
  PQXX_CHECK_EQUAL(
    std::begin(s), std::end(s), "Slice begin()/end() are broken.");
  PQXX_CHECK_EQUAL(
    std::rbegin(s), std::rend(s), "Slice rbegin()/rend() are broken.");

  PQXX_CHECK_THROWS(s.at(0), pqxx::range_error, "at() does not throw.");
  pqxx::row slice;
  PQXX_CHECK_THROWS(
    slice = r[0].slice(0, 2), pqxx::range_error, "No range check.");
  pqxx::ignore_unused(slice);
  PQXX_CHECK_THROWS(
    slice = r[0].slice(1, 0), pqxx::range_error, "Can reverse-slice.");
  pqxx::ignore_unused(slice);

  // Empty slice at end of row.
  s = r[0].slice(1, 1);
  PQXX_CHECK(std::empty(s), "empty() is broken.");
  PQXX_CHECK_EQUAL(std::size(s), 0, "size() is broken.");
  PQXX_CHECK_EQUAL(std::begin(s), std::end(s), "begin()/end() are broken.");
  PQXX_CHECK_EQUAL(
    std::rbegin(s), std::rend(s), "rbegin()/rend() are broken.");

  PQXX_CHECK_THROWS(s.at(0), pqxx::range_error, "at() is inconsistent.");

  // Slice that matches the entire row.
  s = r[0].slice(0, 1);
  PQXX_CHECK(not std::empty(s), "Nonempty slice shows up as empty.");
  PQXX_CHECK_EQUAL(std::size(s), 1, "size() breaks for non-empty slice.");
  PQXX_CHECK_EQUAL(std::begin(s) + 1, std::end(s), "Iteration is broken.");
  PQXX_CHECK_EQUAL(
    std::rbegin(s) + 1, std::rend(s), "Reverse iteration is broken.");
  PQXX_CHECK_EQUAL(s.at(0).as<int>(), 1, "Accessing a slice is broken.");
  PQXX_CHECK_EQUAL(s[0].as<int>(), 1, "operator[] is broken.");
  PQXX_CHECK_THROWS(s.at(1).as<int>(), pqxx::range_error, "at() is off.");

  // Meaningful slice at beginning of row.
  r = tx.exec("SELECT 1, 2, 3");
  s = r[0].slice(0, 1);
  PQXX_CHECK(not std::empty(s), "Slicing confuses empty().");
  PQXX_CHECK_THROWS(
    s.at(1).as<int>(), pqxx::range_error, "at() does not enforce slice.");

  // Meaningful slice that skips an initial column.
  s = r[0].slice(1, 2);
  PQXX_CHECK(
    not std::empty(s), "Slicing away leading columns confuses empty().");
  PQXX_CHECK_EQUAL(s[0].as<int>(), 2, "Slicing offset is broken.");
  PQXX_CHECK_EQUAL(
    std::begin(s)->as<int>(), 2, "Iteration uses wrong offset.");
  PQXX_CHECK_EQUAL(
    std::begin(s) + 1, std::end(s), "Iteration has wrong range.");
  PQXX_CHECK_EQUAL(
    std::rbegin(s) + 1, std::rend(s), "Reverse iteration has wrong range.");
  PQXX_CHECK_THROWS(
    s.at(1).as<int>(), pqxx::range_error, "Offset slicing is broken.");

  // Column names in a slice.
  r = tx.exec("SELECT 1 AS one, 2 AS two, 3 AS three");
  s = r[0].slice(1, 2);
  PQXX_CHECK_EQUAL(s["two"].as<int>(), 2, "Column addressing breaks.");
  PQXX_CHECK_THROWS(
    pqxx::ignore_unused(s.column_number("one")), pqxx::argument_error,
    "Can access column name before slice.");
  PQXX_CHECK_THROWS(
    pqxx::ignore_unused(s.column_number("three")), pqxx::argument_error,
    "Can access column name after slice.");
  PQXX_CHECK_EQUAL(
    s.column_number("Two"), 0, "Column name is case sensitive.");

  // Identical column names.
  r = tx.exec("SELECT 1 AS x, 2 AS x");
  s = r[0].slice(1, 2);
  PQXX_CHECK_EQUAL(s["x"].as<int>(), 2, "Identical column names break slice.");
}


PQXX_REGISTER_TEST(test_result_slicing);
} // namespace

#include "pqxx/internal/ignore-deprecated-post.hxx"
