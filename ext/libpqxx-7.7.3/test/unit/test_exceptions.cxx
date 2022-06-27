#include <pqxx/except>
#include <pqxx/transaction>

#include "../test_helpers.hxx"


namespace
{
void test_exceptions()
{
  std::string const broken_query{"SELECT HORRIBLE ERROR"},
    err{"Error message"};

  try
  {
    throw pqxx::sql_error{err, broken_query};
  }
  catch (std::exception const &e)
  {
    PQXX_CHECK_EQUAL(e.what(), err, "Exception contains wrong message.");
    auto downcast{dynamic_cast<pqxx::sql_error const *>(&e)};
    PQXX_CHECK(
      downcast != nullptr, "exception-to-sql_error downcast is broken.");
    PQXX_CHECK_EQUAL(
      downcast->query(), broken_query,
      "Getting query from pqxx exception is broken.");
  }

  pqxx::connection conn;
  pqxx::work tx{conn};
  try
  {
    tx.exec("INVALID QUERY HERE");
  }
  catch (pqxx::syntax_error const &e)
  {
    // SQL syntax error has sqlstate error 42601.
    PQXX_CHECK_EQUAL(
      e.sqlstate(), "42601", "Unexpected sqlstate on syntax error.");
  }
}


PQXX_REGISTER_TEST(test_exceptions);
} // namespace
