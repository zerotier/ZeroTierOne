#include <pqxx/stream_from>
#include <pqxx/transaction>

#include "../test_helpers.hxx"
#include "../test_types.hxx"

#include <cstring>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <optional>


namespace
{
void test_nonoptionals(pqxx::connection &connection)
{
  pqxx::work tx{connection};
  auto extractor{pqxx::stream_from::query(
    tx, "SELECT * FROM stream_from_test ORDER BY number0")};
  PQXX_CHECK(extractor, "stream_from failed to initialize.");

  std::tuple<int, std::string, int, ipv4, std::string, bytea> got_tuple;

  try
  {
    // We can't read the "910" row -- it contains nulls, which our tuple does
    // not accept.
    extractor >> got_tuple;
    PQXX_CHECK_NOTREACHED(
      "Failed to fail to stream null values into null-less fields.");
  }
  catch (pqxx::conversion_error const &e)
  {
    std::string const what{e.what()};
    if (what.find("null") == std::string::npos)
      throw;
    pqxx::test::expected_exception(
      "Could not stream nulls into null-less fields: " + what);
  }

  // The stream is still good though.
  // The second tuple is fine.
  extractor >> got_tuple;
  PQXX_CHECK(extractor, "Stream ended prematurely.");

  PQXX_CHECK_EQUAL(std::get<0>(got_tuple), 1234, "Bad value.");
  // Don't know much about the timestamp, but let's assume it starts with a
  // year in the second millennium.
  PQXX_CHECK(
    std::get<1>(got_tuple).at(0) == '2', "Bad value.  Expected timestamp.");
  PQXX_CHECK_LESS(
    std::size(std::get<1>(got_tuple)), 40u, "Unexpected length.");
  PQXX_CHECK_GREATER(
    std::size(std::get<1>(got_tuple)), 20u, "Unexpected length.");
  PQXX_CHECK_EQUAL(std::get<2>(got_tuple), 4321, "Bad value.");
  PQXX_CHECK_EQUAL(std::get<3>(got_tuple), (ipv4{8, 8, 8, 8}), "Bad value.");
  PQXX_CHECK_EQUAL(std::get<4>(got_tuple), "hello\n \tworld", "Bad value.");
  PQXX_CHECK_EQUAL(
    std::get<5>(got_tuple), (bytea{'\x00', '\x01', '\x02'}), "Bad value.");

  // The third tuple contains some nulls. For what it's worth, when we *know*
  // that we're getting nulls, we can stream them into nullptr_t fields.
  std::tuple<
    int, std::string, std::nullptr_t, std::nullptr_t, std::string, bytea>
    tup_w_nulls;

  extractor >> tup_w_nulls;
  PQXX_CHECK(extractor, "Stream ended prematurely.");

  PQXX_CHECK_EQUAL(std::get<0>(tup_w_nulls), 5678, "Bad value.");
  PQXX_CHECK(std::get<2>(tup_w_nulls) == nullptr, "Bad null.");
  PQXX_CHECK(std::get<3>(tup_w_nulls) == nullptr, "Bad null.");

  // We're at the end of the stream.
  extractor >> tup_w_nulls;
  PQXX_CHECK(not extractor, "Stream did not end.");

  // Of course we can't stream a non-null value into a nullptr field.
  auto ex2{pqxx::stream_from::query(tx, "SELECT 1")};
  std::tuple<std::nullptr_t> null_tup;
  try
  {
    ex2 >> null_tup;
    PQXX_CHECK_NOTREACHED(
      "stream_from should have refused to convert non-null value to "
      "nullptr_t.");
  }
  catch (pqxx::conversion_error const &e)
  {
    std::string const what{e.what()};
    if (what.find("null") == std::string::npos)
      throw;
    pqxx::test::expected_exception(
      std::string{"Could not extract row: "} + what);
  }
  ex2 >> null_tup;
  PQXX_CHECK(not ex2, "Stream did not end.");

  PQXX_CHECK_SUCCEEDS(
    tx.exec1("SELECT 1"), "Could not use transaction after stream_from.");
}


void test_bad_tuples(pqxx::connection &conn)
{
  pqxx::work tx{conn};
  auto extractor{pqxx::stream_from::table(tx, {"stream_from_test"})};
  PQXX_CHECK(extractor, "stream_from failed to initialize");

  std::tuple<int> got_tuple_too_short;
  try
  {
    extractor >> got_tuple_too_short;
    PQXX_CHECK_NOTREACHED("stream_from improperly read first row");
  }
  catch (pqxx::usage_error const &e)
  {
    std::string what{e.what()};
    if (
      what.find("1") == std::string::npos or
      what.find("6") == std::string::npos)
      throw;
    pqxx::test::expected_exception("Tuple is wrong size: " + what);
  }

  std::tuple<int, std::string, int, ipv4, std::string, bytea, std::string>
    got_tuple_too_long;
  try
  {
    extractor >> got_tuple_too_long;
    PQXX_CHECK_NOTREACHED("stream_from improperly read first row");
  }
  catch (pqxx::usage_error const &e)
  {
    std::string what{e.what()};
    if (
      what.find("6") == std::string::npos or
      what.find("7") == std::string::npos)
      throw;
    pqxx::test::expected_exception("Could not extract row: " + what);
  }

  extractor.complete();
}


#define ASSERT_FIELD_EQUAL(OPT, VAL)                                          \
  PQXX_CHECK(static_cast<bool>(OPT), "unexpected null field");                \
  PQXX_CHECK_EQUAL(*OPT, VAL, "field value mismatch")
#define ASSERT_FIELD_NULL(OPT)                                                \
  PQXX_CHECK(not static_cast<bool>(OPT), "expected null field")


template<template<typename...> class O>
void test_optional(pqxx::connection &connection)
{
  pqxx::work tx{connection};
  auto extractor{pqxx::stream_from::query(
    tx, "SELECT * FROM stream_from_test ORDER BY number0")};
  PQXX_CHECK(extractor, "stream_from failed to initialize");

  std::tuple<int, O<std::string>, O<int>, O<ipv4>, O<std::string>, O<bytea>>
    got_tuple;

  extractor >> got_tuple;
  PQXX_CHECK(extractor, "stream_from failed to read third row");
  PQXX_CHECK_EQUAL(std::get<0>(got_tuple), 910, "field value mismatch");
  ASSERT_FIELD_NULL(std::get<1>(got_tuple));
  ASSERT_FIELD_NULL(std::get<2>(got_tuple));
  ASSERT_FIELD_NULL(std::get<3>(got_tuple));
  ASSERT_FIELD_EQUAL(std::get<4>(got_tuple), "\\N");
  ASSERT_FIELD_EQUAL(std::get<5>(got_tuple), bytea{});

  extractor >> got_tuple;
  PQXX_CHECK(extractor, "stream_from failed to read first row.");
  PQXX_CHECK_EQUAL(std::get<0>(got_tuple), 1234, "Field value mismatch.");
  PQXX_CHECK(
    static_cast<bool>(std::get<1>(got_tuple)), "Unexpected null field.");
  // PQXX_CHECK_EQUAL(*std::get<1>(got_tuple), , "field value mismatch");
  ASSERT_FIELD_EQUAL(std::get<2>(got_tuple), 4321);
  ASSERT_FIELD_EQUAL(std::get<3>(got_tuple), (ipv4{8, 8, 8, 8}));
  ASSERT_FIELD_EQUAL(std::get<4>(got_tuple), "hello\n \tworld");
  ASSERT_FIELD_EQUAL(std::get<5>(got_tuple), (bytea{'\x00', '\x01', '\x02'}));

  extractor >> got_tuple;
  PQXX_CHECK(extractor, "stream_from failed to read second row");
  PQXX_CHECK_EQUAL(std::get<0>(got_tuple), 5678, "field value mismatch");
  ASSERT_FIELD_EQUAL(std::get<1>(got_tuple), "2018-11-17 21:23:00");
  ASSERT_FIELD_NULL(std::get<2>(got_tuple));
  ASSERT_FIELD_NULL(std::get<3>(got_tuple));
  ASSERT_FIELD_EQUAL(std::get<4>(got_tuple), "\u3053\u3093\u306b\u3061\u308f");
  ASSERT_FIELD_EQUAL(
    std::get<5>(got_tuple), (bytea{'f', 'o', 'o', ' ', 'b', 'a', 'r', '\0'}));

  extractor >> got_tuple;
  PQXX_CHECK(not extractor, "stream_from failed to detect end of stream");

  extractor.complete();
}


void test_stream_from()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  tx.exec0(
    "CREATE TEMP TABLE stream_from_test ("
    "number0 INT NOT NULL,"
    "ts1     TIMESTAMP NULL,"
    "number2 INT NULL,"
    "addr3   INET NULL,"
    "txt4    TEXT NULL,"
    "bin5    BYTEA NOT NULL"
    ")");
  tx.exec_params(
    "INSERT INTO stream_from_test VALUES ($1,$2,$3,$4,$5,$6)", 910, nullptr,
    nullptr, nullptr, "\\N", bytea{});
  tx.exec_params(
    "INSERT INTO stream_from_test VALUES ($1,$2,$3,$4,$5,$6)", 1234, "now",
    4321, ipv4{8, 8, 8, 8}, "hello\n \tworld", bytea{'\x00', '\x01', '\x02'});
  tx.exec_params(
    "INSERT INTO stream_from_test VALUES ($1,$2,$3,$4,$5,$6)", 5678,
    "2018-11-17 21:23:00", nullptr, nullptr, "\u3053\u3093\u306b\u3061\u308f",
    bytea{'f', 'o', 'o', ' ', 'b', 'a', 'r', '\0'});
  tx.commit();

  test_nonoptionals(conn);
  test_bad_tuples(conn);
  std::cout << "testing `std::unique_ptr` as optional...\n";
  test_optional<std::unique_ptr>(conn);
  std::cout << "testing `std::optional` as optional...\n";
  test_optional<std::optional>(conn);
}


void test_stream_from_does_escaping()
{
  std::string const input{"a\t\n\n\n \\b\nc"};
  pqxx::connection conn;
  pqxx::work tx{conn};
  tx.exec0("CREATE TEMP TABLE badstr (str text)");
  tx.exec0("INSERT INTO badstr (str) VALUES (" + tx.quote(input) + ")");
  auto reader{pqxx::stream_from::table(tx, {"badstr"})};
  std::tuple<std::string> out;
  reader >> out;
  PQXX_CHECK_EQUAL(
    std::get<0>(out), input, "stream_from got weird characters wrong.");
}


void test_stream_from_does_iteration()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  tx.exec0("CREATE TEMP TABLE str (s text)");
  tx.exec0("INSERT INTO str (s) VALUES ('foo')");
  auto reader{pqxx::stream_from::table(tx, {"str"})};

  int i{0};
  std::string out;
  for (std::tuple<std::string> t : reader.iter<std::string>())
  {
    i++;
    out = std::get<0>(t);
  }
  PQXX_CHECK_EQUAL(i, 1, "Wrong number of iterations.");
  PQXX_CHECK_EQUAL(out, "foo", "Got wrong string.");

  tx.exec0("INSERT INTO str (s) VALUES ('bar')");
  i = 0;
  std::set<std::string> strings;
  auto reader2{pqxx::stream_from::table(tx, {"str"})};
  for (std::tuple<std::string> t : reader2.iter<std::string>())
  {
    i++;
    strings.insert(std::get<0>(t));
  }
  PQXX_CHECK_EQUAL(i, 2, "Wrong number of iterations.");
  PQXX_CHECK_EQUAL(
    std::size(strings), 2u, "Wrong number of strings retrieved.");
  PQXX_CHECK(strings.find("foo") != std::end(strings), "Missing key.");
  PQXX_CHECK(strings.find("bar") != std::end(strings), "Missing key.");
}


void test_transaction_stream_from()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  tx.exec0("CREATE TEMP TABLE sample (id integer, name varchar)");
  tx.exec0("INSERT INTO sample (id, name) VALUES (321, 'something')");

  int items{0};
  int id{0};
  std::string name;

  for (auto [iid, iname] :
       tx.stream<int, std::string_view>("SELECT id, name FROM sample"))
  {
    items++;
    id = iid;
    name = iname;
  }
  PQXX_CHECK_EQUAL(items, 1, "Wrong number of iterations.");
  PQXX_CHECK_EQUAL(id, 321, "Got wrong int.");
  PQXX_CHECK_EQUAL(name, std::string{"something"}, "Got wrong string.");

  PQXX_CHECK_EQUAL(
    tx.query_value<int>("SELECT 4"), 4,
    "Loop did not relinquish transaction.");
}


void test_stream_from_read_row()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  tx.exec0("CREATE TEMP TABLE sample (id integer, name varchar, opt integer)");
  tx.exec0("INSERT INTO sample (id, name) VALUES (321, 'something')");

  auto stream{pqxx::stream_from::table(tx, {"sample"})};
  auto fields{stream.read_row()};
  PQXX_CHECK_EQUAL(fields->size(), 3ul, "Wrong number of fields.");
  PQXX_CHECK_EQUAL(
    std::string((*fields)[0]), "321", "Integer field came out wrong.");
  PQXX_CHECK_EQUAL(
    std::string((*fields)[1]), "something", "Text field came out wrong.");
  PQXX_CHECK(std::data((*fields)[2]) == nullptr, "Null field came out wrong.");

  auto last{stream.read_row()};
  PQXX_CHECK(last == nullptr, "No null pointer at end of stream.");
}


PQXX_REGISTER_TEST(test_stream_from);
PQXX_REGISTER_TEST(test_stream_from_does_escaping);
PQXX_REGISTER_TEST(test_stream_from_does_iteration);
PQXX_REGISTER_TEST(test_transaction_stream_from);
PQXX_REGISTER_TEST(test_stream_from_read_row);
} // namespace
