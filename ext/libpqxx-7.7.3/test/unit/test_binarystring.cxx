#include <pqxx/binarystring>
#include <pqxx/stream_to>
#include <pqxx/transaction>

#include "../test_helpers.hxx"
#include "../test_types.hxx"


namespace
{
pqxx::binarystring
make_binarystring(pqxx::transaction_base &T, std::string content)
{
#include "pqxx/internal/ignore-deprecated-pre.hxx"
  return pqxx::binarystring(T.exec1("SELECT " + T.quote_raw(content))[0]);
#include "pqxx/internal/ignore-deprecated-post.hxx"
}


void test_binarystring()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  auto b{make_binarystring(tx, "")};
  PQXX_CHECK(std::empty(b), "Empty binarystring is not empty.");
  PQXX_CHECK_EQUAL(b.str(), "", "Empty binarystring doesn't work.");
  PQXX_CHECK_EQUAL(std::size(b), 0u, "Empty binarystring has nonzero size.");
  PQXX_CHECK_EQUAL(b.length(), 0u, "Length/size mismatch.");
  PQXX_CHECK(std::begin(b) == std::end(b), "Empty binarystring iterates.");
  PQXX_CHECK(
    std::cbegin(b) == std::begin(b), "Wrong cbegin for empty binarystring.");
  PQXX_CHECK(
    std::rbegin(b) == std::rend(b), "Empty binarystring reverse-iterates.");
  PQXX_CHECK(
    std::crbegin(b) == std::rbegin(b),
    "Wrong crbegin for empty binarystring.");
  PQXX_CHECK_THROWS(
    b.at(0), std::out_of_range, "Empty binarystring accepts at().");

  b = make_binarystring(tx, "z");
  PQXX_CHECK_EQUAL(b.str(), "z", "Basic nonempty binarystring is broken.");
  PQXX_CHECK(not std::empty(b), "Nonempty binarystring is empty.");
  PQXX_CHECK_EQUAL(std::size(b), 1u, "Bad binarystring size.");
  PQXX_CHECK_EQUAL(b.length(), 1u, "Length/size mismatch.");
  PQXX_CHECK(
    std::begin(b) != std::end(b), "Nonempty binarystring does not iterate.");
  PQXX_CHECK(
    std::rbegin(b) != std::rend(b),
    "Nonempty binarystring does not reverse-iterate.");
  PQXX_CHECK(std::begin(b) + 1 == std::end(b), "Bad iteration.");
  PQXX_CHECK(std::rbegin(b) + 1 == std::rend(b), "Bad reverse iteration.");
  PQXX_CHECK(std::cbegin(b) == std::begin(b), "Wrong cbegin.");
  PQXX_CHECK(std::cend(b) == std::end(b), "Wrong cend.");
  PQXX_CHECK(std::crbegin(b) == std::rbegin(b), "Wrong crbegin.");
  PQXX_CHECK(std::crend(b) == std::rend(b), "Wrong crend.");
  PQXX_CHECK(b.front() == 'z', "Unexpected front().");
  PQXX_CHECK(b.back() == 'z', "Unexpected back().");
  PQXX_CHECK(b.at(0) == 'z', "Unexpected data at index 0.");
  PQXX_CHECK_THROWS(
    b.at(1), std::out_of_range, "Failed to catch range error.");

  std::string const simple{"ab"};
  b = make_binarystring(tx, simple);
  PQXX_CHECK_EQUAL(
    b.str(), simple, "Binary (un)escaping went wrong somewhere.");
  PQXX_CHECK_EQUAL(
    std::size(b), std::size(simple), "Escaping confuses length.");

  std::string const simple_escaped{
    tx.esc_raw(std::basic_string_view<std::byte>{
      reinterpret_cast<std::byte const *>(std::data(simple)),
      std::size(simple)})};
  for (auto c : simple_escaped)
  {
    auto const uc{static_cast<unsigned char>(c)};
    PQXX_CHECK(uc <= 127, "Non-ASCII byte in escaped string.");
  }

#include "pqxx/internal/ignore-deprecated-pre.hxx"
  PQXX_CHECK_EQUAL(
    tx.quote_raw(
      reinterpret_cast<unsigned char const *>(simple.c_str()),
      std::size(simple)),
    tx.quote(b), "quote_raw is broken");
  PQXX_CHECK_EQUAL(
    tx.quote(b), tx.quote_raw(simple), "Binary quoting is broken.");
  PQXX_CHECK_EQUAL(
    pqxx::binarystring(tx.exec1("SELECT " + tx.quote(b))[0]).str(), simple,
    "Binary string is not idempotent.");
#include "pqxx/internal/ignore-deprecated-post.hxx"

  std::string const bytes("\x01\x23\x23\xa1\x2b\x0c\xff");
  b = make_binarystring(tx, bytes);
  PQXX_CHECK_EQUAL(b.str(), bytes, "Binary data breaks (un)escaping.");

  std::string const nully("a\0b", 3);
  b = make_binarystring(tx, nully);
  PQXX_CHECK_EQUAL(b.str(), nully, "Nul byte broke binary (un)escaping.");
  PQXX_CHECK_EQUAL(std::size(b), 3u, "Nul byte broke binarystring size.");

  b = make_binarystring(tx, "foo");
  PQXX_CHECK_EQUAL(std::string(b.get(), 3), "foo", "get() appears broken.");

  auto b1{make_binarystring(tx, "1")}, b2{make_binarystring(tx, "2")};
  PQXX_CHECK_NOT_EQUAL(b1.get(), b2.get(), "Madness rules.");
  PQXX_CHECK_NOT_EQUAL(b1.str(), b2.str(), "Logic has no more meaning.");
  b1.swap(b2);
  PQXX_CHECK_NOT_EQUAL(b1.str(), b2.str(), "swap() equalized binarystrings.");
  PQXX_CHECK_NOT_EQUAL(b1.str(), "1", "swap() did not happen.");
  PQXX_CHECK_EQUAL(b1.str(), "2", "swap() is broken.");
  PQXX_CHECK_EQUAL(b2.str(), "1", "swap() went insane.");

  b = make_binarystring(tx, "bar");
  b.swap(b);
  PQXX_CHECK_EQUAL(b.str(), "bar", "Self-swap confuses binarystring.");

  b = make_binarystring(tx, "\\x");
  PQXX_CHECK_EQUAL(b.str(), "\\x", "Hex-escape header confused (un)escaping.");
}


void test_binarystring_conversion()
{
  constexpr char bytes[]{"f\to\0o\n\0"};
  std::string_view const data{bytes, std::size(bytes) - 1};
#include "pqxx/internal/ignore-deprecated-pre.hxx"
  pqxx::binarystring bin{data};
#include "pqxx/internal/ignore-deprecated-post.hxx"
  auto const escaped{pqxx::to_string(bin)};
  PQXX_CHECK_EQUAL(
    escaped, std::string_view{"\\x66096f006f0a00"}, "Unexpected hex escape.");
  auto const restored{pqxx::from_string<pqxx::binarystring>(escaped)};
  PQXX_CHECK_EQUAL(
    std::size(restored), std::size(data), "Unescaping produced wrong length.");
}


void test_binarystring_stream()
{
  constexpr char bytes[]{"a\tb\0c"};
  std::string_view const data{bytes, std::size(bytes) - 1};
#include "pqxx/internal/ignore-deprecated-pre.hxx"
  pqxx::binarystring bin{data};
#include "pqxx/internal/ignore-deprecated-post.hxx"

  pqxx::connection conn;
  pqxx::transaction tx{conn};
  tx.exec0("CREATE TEMP TABLE pqxxbinstream(id integer, bin bytea)");

  auto to{pqxx::stream_to::table(tx, {"pqxxbinstream"})};
  to.write_values(0, bin);
  to.complete();

  auto ptr{reinterpret_cast<std::byte const *>(std::data(data))};
  auto expect{
    tx.quote(std::basic_string_view<std::byte>{ptr, std::size(data)})};
  PQXX_CHECK(
    tx.query_value<bool>("SELECT bin = " + expect + " FROM pqxxbinstream"),
    "binarystring did not stream_to properly.");
  PQXX_CHECK_EQUAL(
    tx.query_value<std::size_t>("SELECT octet_length(bin) FROM pqxxbinstream"),
    std::size(data), "Did the terminating zero break the bytea?");
}


void test_binarystring_array_stream()
{
  pqxx::connection conn;
  pqxx::transaction tx{conn};
  tx.exec0("CREATE TEMP TABLE pqxxbinstream(id integer, vec bytea[])");

  constexpr char bytes1[]{"a\tb\0c"}, bytes2[]{"1\0.2"};
  std::string_view const data1{bytes1}, data2{bytes2};
#include "pqxx/internal/ignore-deprecated-pre.hxx"
  pqxx::binarystring bin1{data1}, bin2{data2};
  std::vector<pqxx::binarystring> const vec{bin1, bin2};
#include "pqxx/internal/ignore-deprecated-post.hxx"

  auto to{pqxx::stream_to::table(tx, {"pqxxbinstream"})};
  to.write_values(0, vec);
  to.complete();

  PQXX_CHECK_EQUAL(
    tx.query_value<std::size_t>(
      "SELECT array_length(vec, 1) FROM pqxxbinstream"),
    std::size(vec), "Array came out with wrong length.");

  auto ptr1{reinterpret_cast<std::byte const *>(std::data(data1))},
    ptr2{reinterpret_cast<std::byte const *>(std::data(data2))};
  auto expect1{
    tx.quote(std::basic_string_view<std::byte>{ptr1, std::size(data1)})},
    expect2{
      tx.quote(std::basic_string_view<std::byte>{ptr2, std::size(data2)})};
  PQXX_CHECK(
    tx.query_value<bool>("SELECT vec[1] = " + expect1 + " FROM pqxxbinstream"),
    "Bytea in array came out wrong.");
  PQXX_CHECK(
    tx.query_value<bool>("SELECT vec[2] = " + expect2 + " FROM pqxxbinstream"),
    "First bytea in array worked, but second did not.");
  PQXX_CHECK_EQUAL(
    tx.query_value<std::size_t>(
      "SELECT octet_length(vec[1]) FROM pqxxbinstream"),
    std::size(data1), "Bytea length broke inside array.");
}


PQXX_REGISTER_TEST(test_binarystring);
PQXX_REGISTER_TEST(test_binarystring_conversion);
PQXX_REGISTER_TEST(test_binarystring_stream);
PQXX_REGISTER_TEST(test_binarystring_array_stream);
} // namespace
