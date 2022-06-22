#include <iostream>

#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
using namespace std::literals;


void compare_esc(
  pqxx::connection &c, pqxx::transaction_base &t, char const text[])
{
  std::size_t const len{std::size(std::string{text})};
  PQXX_CHECK_EQUAL(
    c.esc(std::string_view{text, len}), t.esc(std::string_view{text, len}),
    "Connection & transaction escape differently.");

  PQXX_CHECK_EQUAL(
    t.esc(std::string_view{text, len}), t.esc(text),
    "Length argument to esc() changes result.");

  PQXX_CHECK_EQUAL(
    t.esc(std::string{text}), t.esc(text),
    "esc(std::string()) differs from esc(char const[]).");

  PQXX_CHECK_EQUAL(
    text,
    t.query_value<std::string>(
      "SELECT '" + t.esc(std::string_view{text, len}) + "'"),
    "esc() is not idempotent.");

  PQXX_CHECK_EQUAL(
    t.esc(std::string_view{text, len}), t.esc(text),
    "Oversized buffer affects esc().");
}


void test_esc(pqxx::connection &c, pqxx::transaction_base &t)
{
  PQXX_CHECK_EQUAL(
    t.esc(std::string_view{"", 0}), "",
    "Empty string doesn't escape properly.");
  PQXX_CHECK_EQUAL(
    t.esc(std::string_view{"'", 1}), "''",
    "Single quote escaped incorrectly.");
  PQXX_CHECK_EQUAL(
    t.esc(std::string_view{"hello"}), "hello", "Trivial escape went wrong.");
  char const *const escstrings[]{"x", " ", "", nullptr};
  for (std::size_t i{0}; escstrings[i] != nullptr; ++i)
    compare_esc(c, t, escstrings[i]);
}


void test_quote(pqxx::connection &c, pqxx::transaction_base &t)
{
  PQXX_CHECK_EQUAL(t.quote("x"), "'x'", "Basic quote() fails.");
  PQXX_CHECK_EQUAL(
    t.quote(1), "'1'", "quote() not dealing with int properly.");
  PQXX_CHECK_EQUAL(t.quote(0), "'0'", "Quoting zero is a problem.");
  char const *const null_ptr{nullptr};
  PQXX_CHECK_EQUAL(t.quote(null_ptr), "NULL", "Not quoting NULL correctly.");
  PQXX_CHECK_EQUAL(
    t.quote(std::string{"'"}), "''''", "Escaping quotes goes wrong.");

  PQXX_CHECK_EQUAL(
    t.quote("x"), c.quote("x"),
    "Connection and transaction quote differently.");

  char const *test_strings[]{"",   "x",   "\\", "\\\\", "'",
                             "''", "\\'", "\t", "\n",   nullptr};

  for (std::size_t i{0}; test_strings[i] != nullptr; ++i)
  {
    auto r{t.query_value<std::string>("SELECT " + t.quote(test_strings[i]))};
    PQXX_CHECK_EQUAL(
      r, test_strings[i], "Selecting quoted string does not come back equal.");
  }
}


void test_quote_name(pqxx::transaction_base &t)
{
  PQXX_CHECK_EQUAL(
    "\"A b\"", t.quote_name("A b"), "Escaped identifier is not as expected.");
  PQXX_CHECK_EQUAL(
    std::string{"A b"},
    t.exec("SELECT 1 AS " + t.quote_name("A b")).column_name(0),
    "Escaped identifier does not work in SQL.");
}


void test_esc_raw_unesc_raw(pqxx::transaction_base &t)
{
  constexpr char binary[]{"1\0023\\4x5"};
  std::basic_string<std::byte> const data(
    reinterpret_cast<std::byte const *>(binary), std::size(binary));
  std::string const escaped{t.esc_raw(
    std::basic_string_view<std::byte>{std::data(data), std::size(binary)})};

  for (auto const i : escaped)
  {
    PQXX_CHECK_GREATER(
      static_cast<unsigned>(static_cast<unsigned char>(i)), 7u,
      "Non-ASCII character in escaped data: " + escaped);
    PQXX_CHECK_LESS(
      static_cast<unsigned>(static_cast<unsigned char>(i)), 127u,
      "Non-ASCII character in escaped data: " + escaped);
  }

  for (auto const i : escaped)
    PQXX_CHECK(
      isprint(i), "Unprintable character in escaped data: " + escaped);

  PQXX_CHECK_EQUAL(
    escaped, "\\x3102335c34783500", "Binary data escaped wrong.");
  PQXX_CHECK_EQUAL(
    std::size(t.unesc_bin(escaped)), std::size(data),
    "Wrong size after unescaping.");
  auto unescaped{t.unesc_bin(escaped)};
  PQXX_CHECK_EQUAL(
    std::size(unescaped), std::size(data),
    "Unescaping did not restore original size.");
  for (std::size_t i{0}; i < std::size(unescaped); ++i)
    PQXX_CHECK_EQUAL(
      int(unescaped[i]), int(data[i]),
      "Unescaping binary data did not restore byte #" + pqxx::to_string(i) +
        ".");
}


void test_esc_like(pqxx::transaction_base &tx)
{
  PQXX_CHECK_EQUAL(tx.esc_like(""), "", "esc_like breaks on empty string.");
  PQXX_CHECK_EQUAL(tx.esc_like("abc"), "abc", "esc_like is broken.");
  PQXX_CHECK_EQUAL(tx.esc_like("_"), "\\_", "esc_like fails on underscore.");
  PQXX_CHECK_EQUAL(tx.esc_like("%"), "\\%", "esc_like fails on %.");
  PQXX_CHECK_EQUAL(
    tx.esc_like("a%b_c"), "a\\%b\\_c", "esc_like breaks on mix.");
  PQXX_CHECK_EQUAL(
    tx.esc_like("_", '+'), "+_", "esc_like ignores escape character.");
}


void test_escaping()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  test_esc(conn, tx);
  test_quote(conn, tx);
  test_quote_name(tx);
  test_esc_raw_unesc_raw(tx);
  test_esc_like(tx);
}


void test_esc_escapes_into_buffer()
{
#if defined(PQXX_HAVE_CONCEPTS)
  pqxx::connection conn;
  pqxx::work tx{conn};

  std::string buffer;
  buffer.resize(20);

  auto const text{"Ain't"sv};
  auto escaped_text{tx.esc(text, buffer)};
  PQXX_CHECK_EQUAL(escaped_text, "Ain''t", "Escaping into buffer went wrong.");

  std::basic_string<std::byte> const data{std::byte{0x22}, std::byte{0x43}};
  auto escaped_data(tx.esc(data, buffer));
  PQXX_CHECK_EQUAL(escaped_data, "\\x2243", "Binary data escaped wrong.");
#endif
}


void test_esc_accepts_various_types()
{
#if defined(PQXX_HAVE_CONCEPTS) && defined(PQXX_HAVE_SPAN)
  pqxx::connection conn;
  pqxx::work tx{conn};

  std::string buffer;
  buffer.resize(20);

  std::string const text{"it's"};
  auto escaped_text{tx.esc(text, buffer)};
  PQXX_CHECK_EQUAL(escaped_text, "it''s", "Escaping into buffer went wrong.");

  std::vector<std::byte> const data{std::byte{0x23}, std::byte{0x44}};
  auto escaped_data(tx.esc(data, buffer));
  PQXX_CHECK_EQUAL(escaped_data, "\\x2344", "Binary data escaped wrong.");
#endif
}


void test_binary_esc_checks_buffer_length()
{
#if defined(PQXX_HAVE_CONCEPTS) && defined(PQXX_HAVE_SPAN)
  pqxx::connection conn;
  pqxx::work tx{conn};

  std::string buf;
  std::basic_string<std::byte> bin{
    std::byte{'b'}, std::byte{'o'}, std::byte{'o'}};

  buf.resize(2 * std::size(bin) + 3);
  pqxx::ignore_unused(tx.esc(bin, buf));
  PQXX_CHECK_EQUAL(int{buf[0]}, int{'\\'}, "Unexpected binary escape format.");
  PQXX_CHECK_NOT_EQUAL(
    int(buf[std::size(buf) - 2]), int('\0'), "Escaped binary ends too soon.");
  PQXX_CHECK_EQUAL(
    int(buf[std::size(buf) - 1]), int('\0'), "Terminating zero is missing.");

  buf.resize(2 * std::size(bin) + 2);
  PQXX_CHECK_THROWS(
    pqxx::ignore_unused(tx.esc(bin, buf)), pqxx::range_error,
    "Didn't get expected exception from escape overrun.");
#endif
}


PQXX_REGISTER_TEST(test_escaping);
PQXX_REGISTER_TEST(test_esc_escapes_into_buffer);
PQXX_REGISTER_TEST(test_esc_accepts_various_types);
PQXX_REGISTER_TEST(test_binary_esc_checks_buffer_length);
} // namespace
