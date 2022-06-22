#include <pqxx/cursor>
#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
void test_stateless_cursor_provides_random_access(pqxx::connection &conn)
{
  pqxx::work tx{conn};
  pqxx::stateless_cursor<
    pqxx::cursor_base::read_only, pqxx::cursor_base::owned>
    c{tx, "SELECT * FROM generate_series(0, 3)", "count", false};

  auto r{c.retrieve(1, 2)};
  PQXX_CHECK_EQUAL(std::size(r), 1, "Wrong number of rows from retrieve().");
  PQXX_CHECK_EQUAL(r[0][0].as<int>(), 1, "Cursor retrieved wrong data.");

  r = c.retrieve(3, 10);
  PQXX_CHECK_EQUAL(std::size(r), 1, "Expected 1 row retrieving past end.");
  PQXX_CHECK_EQUAL(r[0][0].as<int>(), 3, "Wrong data retrieved at end.");

  r = c.retrieve(0, 1);
  PQXX_CHECK_EQUAL(std::size(r), 1, "Wrong number of rows back at beginning.");
  PQXX_CHECK_EQUAL(r[0][0].as<int>(), 0, "Wrong data back at beginning.");
}


void test_stateless_cursor_ignores_trailing_semicolon(pqxx::connection &conn)
{
  pqxx::work tx{conn};
  pqxx::stateless_cursor<
    pqxx::cursor_base::read_only, pqxx::cursor_base::owned>
    c{tx, "SELECT * FROM generate_series(0, 3)  ;; ; \n \t  ", "count", false};

  auto r{c.retrieve(1, 2)};
  PQXX_CHECK_EQUAL(std::size(r), 1, "Trailing semicolon confused retrieve().");
}


void test_cursor()
{
  pqxx::connection conn;
  test_stateless_cursor_provides_random_access(conn);
  test_stateless_cursor_ignores_trailing_semicolon(conn);
}


PQXX_REGISTER_TEST(test_cursor);
} // namespace
