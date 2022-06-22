#include <iostream>
#include <optional>

#include <pqxx/stream_to>
#include <pqxx/transaction>

#include "../test_helpers.hxx"
#include "../test_types.hxx"

namespace
{
std::string truncate_sql_error(std::string const &what)
{
  auto trunc{what.substr(0, what.find('\n'))};
  if (std::size(trunc) > 64)
    trunc = trunc.substr(0, 61) + "...";
  return trunc;
}


void test_nonoptionals(pqxx::connection &connection)
{
  pqxx::work tx{connection};
  auto inserter{pqxx::stream_to::table(tx, {"stream_to_test"})};
  PQXX_CHECK(inserter, "stream_to failed to initialize");

  auto const nonascii{"\u3053\u3093\u306b\u3061\u308f"};
  bytea const binary{'\x00', '\x01', '\x02'},
    text{'f', 'o', 'o', ' ', 'b', 'a', 'r', '\0'};

  inserter << std::make_tuple(
    1234, "now", 4321, ipv4{8, 8, 4, 4}, "hello nonoptional world", binary);
  inserter << std::make_tuple(
    5678, "2018-11-17 21:23:00", nullptr, nullptr, nonascii, text);
  inserter << std::make_tuple(910, nullptr, nullptr, nullptr, "\\N", bytea{});

  inserter.complete();

  auto r1{tx.exec1("SELECT * FROM stream_to_test WHERE number0 = 1234")};
  PQXX_CHECK_EQUAL(r1[0].as<int>(), 1234, "Read back wrong first int.");
  PQXX_CHECK_EQUAL(
    r1[4].as<std::string>(), "hello nonoptional world",
    "Read back wrong string.");
  PQXX_CHECK_EQUAL(r1[3].as<ipv4>(), ipv4(8, 8, 4, 4), "Read back wrong ip.");
  PQXX_CHECK_EQUAL(r1[5].as<bytea>(), binary, "Read back wrong bytea.");

  auto r2{tx.exec1("SELECT * FROM stream_to_test WHERE number0 = 5678")};
  PQXX_CHECK_EQUAL(r2[0].as<int>(), 5678, "Wrong int on second row.");
  PQXX_CHECK(r2[2].is_null(), "Field 2 was meant to be null.");
  PQXX_CHECK(r2[3].is_null(), "Field 3 was meant to be null.");
  PQXX_CHECK_EQUAL(r2[4].as<std::string>(), nonascii, "Wrong non-ascii text.");
  tx.commit();
}

void test_nonoptionals_fold(pqxx::connection &connection)
{
  pqxx::work tx{connection};
  auto inserter{pqxx::stream_to::table(tx, {"stream_to_test"})};
  PQXX_CHECK(inserter, "stream_to failed to initialize");

  auto const nonascii{"\u3053\u3093\u306b\u3061\u308f"};
  bytea const binary{'\x00', '\x01', '\x02'},
    text{'f', 'o', 'o', ' ', 'b', 'a', 'r', '\0'};

  inserter.write_values(
    1234, "now", 4321, ipv4{8, 8, 4, 4}, "hello nonoptional world", binary);
  inserter.write_values(
    5678, "2018-11-17 21:23:00", nullptr, nullptr, nonascii, text);
  inserter.write_values(910, nullptr, nullptr, nullptr, "\\N", bytea{});

  inserter.complete();

  auto r1{tx.exec1("SELECT * FROM stream_to_test WHERE number0 = 1234")};
  PQXX_CHECK_EQUAL(r1[0].as<int>(), 1234, "Read back wrong first int.");
  PQXX_CHECK_EQUAL(
    r1[4].as<std::string>(), "hello nonoptional world",
    "Read back wrong string.");
  PQXX_CHECK_EQUAL(r1[3].as<ipv4>(), ipv4(8, 8, 4, 4), "Read back wrong ip.");
  PQXX_CHECK_EQUAL(r1[5].as<bytea>(), binary, "Read back wrong bytera.");

  auto r2{tx.exec1("SELECT * FROM stream_to_test WHERE number0 = 5678")};
  PQXX_CHECK_EQUAL(r2[0].as<int>(), 5678, "Wrong int on second row.");
  PQXX_CHECK(r2[2].is_null(), "Field 2 was meant to be null.");
  PQXX_CHECK(r2[3].is_null(), "Field 3 was meant to be null.");
  PQXX_CHECK_EQUAL(r2[4].as<std::string>(), nonascii, "Wrong non-ascii text.");
  tx.commit();
}


/// Try to violate stream_to_test's not-null constraint using a stream_to.
void insert_bad_null_tuple(pqxx::stream_to &inserter)
{
  inserter << std::make_tuple(
    nullptr, "now", 4321, ipv4{8, 8, 8, 8}, "hello world",
    bytea{'\x00', '\x01', '\x02'});
  inserter.complete();
}


void test_bad_null(pqxx::connection &connection)
{
  pqxx::work tx{connection};
  auto inserter{pqxx::stream_to::table(tx, {"stream_to_test"})};
  PQXX_CHECK(inserter, "stream_to failed to initialize");
  PQXX_CHECK_THROWS(
    insert_bad_null_tuple(inserter), pqxx::not_null_violation,
    "Did not expected not_null_violation when stream_to inserts a bad null.");
}


/// Try to violate stream_to_test's not-null construct using a stream_to.
void insert_bad_null_write(pqxx::stream_to &inserter)
{
  inserter.write_values(
    nullptr, "now", 4321, ipv4{8, 8, 8, 8}, "hello world",
    bytea{'\x00', '\x01', '\x02'});
  inserter.complete();
}


void test_bad_null_fold(pqxx::connection &connection)
{
  pqxx::work tx{connection};
  auto inserter{pqxx::stream_to::table(tx, {"stream_to_test"})};
  PQXX_CHECK(inserter, "stream_to failed to initialize");
  PQXX_CHECK_THROWS(
    insert_bad_null_write(inserter), pqxx::not_null_violation,
    "Did not expected not_null_violation when stream_to inserts a bad null.");
}


void test_too_few_fields(pqxx::connection &connection)
{
  pqxx::work tx{connection};
  auto inserter{pqxx::stream_to::table(tx, {"stream_to_test"})};
  PQXX_CHECK(inserter, "stream_to failed to initialize");

  try
  {
    inserter << std::make_tuple(1234, "now", 4321, ipv4{8, 8, 8, 8});
    inserter.complete();
    tx.commit();
    PQXX_CHECK_NOTREACHED("stream_from improperly inserted row");
  }
  catch (pqxx::sql_error const &e)
  {
    std::string what{e.what()};
    if (what.find("missing data for column") == std::string::npos)
      throw;
    pqxx::test::expected_exception(
      "Could not insert row: " + truncate_sql_error(what));
  }
}

void test_too_few_fields_fold(pqxx::connection &connection)
{
  pqxx::work tx{connection};
  auto inserter{pqxx::stream_to::table(tx, {"stream_to_test"})};
  PQXX_CHECK(inserter, "stream_to failed to initialize");

  try
  {
    inserter.write_values(1234, "now", 4321, ipv4{8, 8, 8, 8});
    inserter.complete();
    tx.commit();
    PQXX_CHECK_NOTREACHED("stream_from_fold improperly inserted row");
  }
  catch (pqxx::sql_error const &e)
  {
    std::string what{e.what()};
    if (what.find("missing data for column") == std::string::npos)
      throw;
    pqxx::test::expected_exception(
      "Fold - Could not insert row: " + truncate_sql_error(what));
  }
}


void test_too_many_fields(pqxx::connection &connection)
{
  pqxx::work tx{connection};
  auto inserter{pqxx::stream_to::table(tx, {"stream_to_test"})};
  PQXX_CHECK(inserter, "stream_to failed to initialize");

  try
  {
    inserter << std::make_tuple(
      1234, "now", 4321, ipv4{8, 8, 8, 8}, "hello world",
      bytea{'\x00', '\x01', '\x02'}, 5678);
    inserter.complete();
    tx.commit();
    PQXX_CHECK_NOTREACHED("stream_from improperly inserted row");
  }
  catch (pqxx::sql_error const &e)
  {
    std::string what{e.what()};
    if (what.find("extra data") == std::string::npos)
      throw;
    pqxx::test::expected_exception(
      "Could not insert row: " + truncate_sql_error(what));
  }
}

void test_too_many_fields_fold(pqxx::connection &connection)
{
  pqxx::work tx{connection};
  auto inserter{pqxx::stream_to::table(tx, {"stream_to_test"})};
  PQXX_CHECK(inserter, "stream_to failed to initialize");

  try
  {
    inserter.write_values(
      1234, "now", 4321, ipv4{8, 8, 8, 8}, "hello world",
      bytea{'\x00', '\x01', '\x02'}, 5678);
    inserter.complete();
    tx.commit();
    PQXX_CHECK_NOTREACHED("stream_from_fold improperly inserted row");
  }
  catch (pqxx::sql_error const &e)
  {
    std::string what{e.what()};
    if (what.find("extra data") == std::string::npos)
      throw;
    pqxx::test::expected_exception(
      "Fold - Could not insert row: " + truncate_sql_error(what));
  }
}


void test_stream_to_does_nonnull_optional()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  tx.exec0("CREATE TEMP TABLE foo(x integer, y text)");
  auto inserter{pqxx::stream_to::table(tx, {"foo"})};
  inserter.write_values(
    std::optional<int>{368}, std::optional<std::string>{"Text"});
  inserter.complete();
  auto const row{tx.exec1("SELECT x, y FROM foo")};
  PQXX_CHECK_EQUAL(
    row[0].as<std::string>(), "368", "Non-null int optional came out wrong.");
  PQXX_CHECK_EQUAL(
    row[1].as<std::string>(), "Text",
    "Non-null string optional came out wrong.");
}


template<template<typename...> class O>
void test_optional(pqxx::connection &connection)
{
  pqxx::work tx{connection};
  auto inserter{pqxx::stream_to::table(tx, {"stream_to_test"})};
  PQXX_CHECK(inserter, "stream_to failed to initialize");

  inserter << std::make_tuple(
    910, O<std::string>{pqxx::nullness<O<std::string>>::null()},
    O<int>{pqxx::nullness<O<int>>::null()},
    O<ipv4>{pqxx::nullness<O<ipv4>>::null()}, "\\N", bytea{});

  inserter.complete();
  tx.commit();
}

template<template<typename...> class O>
void test_optional_fold(pqxx::connection &connection)
{
  pqxx::work tx{connection};
  auto inserter{pqxx::stream_to::table(tx, {"stream_to_test"})};
  PQXX_CHECK(inserter, "stream_to failed to initialize");

  inserter.write_values(
    910, O<std::string>{pqxx::nullness<O<std::string>>::null()},
    O<int>{pqxx::nullness<O<int>>::null()},
    O<ipv4>{pqxx::nullness<O<ipv4>>::null()}, "\\N", bytea{});

  inserter.complete();
  tx.commit();
}


// As an alternative to a tuple, you can also insert a container.
void test_container_stream_to()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  tx.exec0("CREATE TEMP TABLE test_container(a integer, b integer)");

  auto inserter{pqxx::stream_to::table(tx, {"test_container"})};

  inserter << std::vector{112, 244};
  inserter.complete();

  auto read{tx.exec1("SELECT * FROM test_container")};
  PQXX_CHECK_EQUAL(
    read[0].as<int>(), 112, "stream_to on container went wrong.");
  PQXX_CHECK_EQUAL(
    read[1].as<int>(), 244, "Second container field went wrong.");
  tx.commit();
}

void test_variant_fold(pqxx::connection_base &connection)
{
  pqxx::work tx{connection};
  auto inserter{pqxx::stream_to::table(tx, {"stream_to_test"})};
  PQXX_CHECK(inserter, "stream_to failed to initialize");

  inserter.write_values(
    std::variant<std::string, int>{1234},
    std::variant<float, std::string>{"now"}, 4321, ipv4{8, 8, 8, 8},
    "hello world", bytea{'\x00', '\x01', '\x02'});
  inserter.write_values(
    5678, "2018-11-17 21:23:00", nullptr, nullptr,
    "\u3053\u3093\u306b\u3061\u308f",
    bytea{'f', 'o', 'o', ' ', 'b', 'a', 'r', '\0'});
  inserter.write_values(910, nullptr, nullptr, nullptr, "\\N", bytea{});

  inserter.complete();
  tx.commit();
}

void clear_table(pqxx::connection &conn)
{
  pqxx::work tx{conn};
  tx.exec0("DELETE FROM stream_to_test");
  tx.commit();
}


void test_stream_to()
{
  pqxx::connection conn;
  pqxx::work tx{conn};

  tx.exec0(
    "CREATE TEMP TABLE stream_to_test ("
    "number0 INT NOT NULL,"
    "ts1     TIMESTAMP NULL,"
    "number2 INT NULL,"
    "addr3   INET NULL,"
    "txt4    TEXT NULL,"
    "bin5    BYTEA NOT NULL"
    ")");
  tx.commit();

  test_nonoptionals(conn);
  clear_table(conn);
  test_nonoptionals_fold(conn);
  clear_table(conn);
  test_bad_null(conn);
  clear_table(conn);
  test_bad_null_fold(conn);
  clear_table(conn);
  test_too_few_fields(conn);
  clear_table(conn);
  test_too_few_fields_fold(conn);
  clear_table(conn);
  test_too_many_fields(conn);
  clear_table(conn);
  test_too_many_fields_fold(conn);
  clear_table(conn);
  test_optional<std::unique_ptr>(conn);
  clear_table(conn);
  test_optional_fold<std::unique_ptr>(conn);
  clear_table(conn);
  test_optional<std::optional>(conn);
  clear_table(conn);
  test_optional_fold<std::optional>(conn);
  clear_table(conn);
  test_variant_fold(conn);
}


void test_stream_to_factory_with_static_columns()
{
  pqxx::connection conn;
  pqxx::work tx{conn};

  tx.exec0("CREATE TEMP TABLE pqxx_stream_to(a integer, b varchar)");

  auto stream{pqxx::stream_to::table(tx, {"pqxx_stream_to"}, {"a", "b"})};
  stream.write_values(3, "three");
  stream.complete();

  auto r{tx.exec1("SELECT a, b FROM pqxx_stream_to")};
  PQXX_CHECK_EQUAL(r[0].as<int>(), 3, "Failed to stream_to a table.");
  PQXX_CHECK_EQUAL(
    r[1].as<std::string>(), "three",
    "Failed to stream_to a string to a table.");
}


void test_stream_to_factory_with_dynamic_columns()
{
  pqxx::connection conn;
  pqxx::work tx{conn};

  tx.exec0("CREATE TEMP TABLE pqxx_stream_to(a integer, b varchar)");

  std::vector<std::string_view> columns{"a", "b"};
#if defined(PQXX_HAVE_CONCEPTS)
  auto stream{pqxx::stream_to::table(tx, {"pqxx_stream_to"}, columns)};
#else
  auto stream{pqxx::stream_to::raw_table(
    tx, conn.quote_table({"pqxx_stream_to"}), conn.quote_columns(columns))};
#endif
  stream.write_values(4, "four");
  stream.complete();

  auto r{tx.exec1("SELECT a, b FROM pqxx_stream_to")};
  PQXX_CHECK_EQUAL(
    r[0].as<int>(), 4, "Failed to stream_to a table with dynamic columns.");
  PQXX_CHECK_EQUAL(
    r[1].as<std::string>(), "four",
    "Failed to stream_to a string to a table with dynamic columns.");
}


void test_stream_to_quotes_arguments()
{
  pqxx::connection conn;
  pqxx::work tx{conn};

  std::string const table{R"--(pqxx_Stream"'x)--"}, column{R"--(a'"b)--"};

  tx.exec0(
    "CREATE TEMP TABLE " + tx.quote_name(table) + "(" + tx.quote_name(column) +
    " integer)");
  auto write{pqxx::stream_to::table(tx, {table}, {column})};
  write.write_values<int>(12);
  write.complete();

  PQXX_CHECK_EQUAL(
    tx.query_value<int>(
      "SELECT " + tx.quote_name(column) + " FROM " + tx.quote_name(table)),
    12, "Stream wrote wrong value.");
}


PQXX_REGISTER_TEST(test_stream_to);
PQXX_REGISTER_TEST(test_container_stream_to);
PQXX_REGISTER_TEST(test_stream_to_does_nonnull_optional);
PQXX_REGISTER_TEST(test_stream_to_factory_with_static_columns);
PQXX_REGISTER_TEST(test_stream_to_factory_with_dynamic_columns);
PQXX_REGISTER_TEST(test_stream_to_quotes_arguments);
} // namespace
