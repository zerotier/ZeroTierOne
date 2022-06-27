#include <iostream>

#include <pqxx/pipeline>
#include <pqxx/transaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Test program for libpqxx.  Test error handling for pipeline.
namespace
{
void test_072()
{
  connection conn;
  work tx{conn};
  pipeline P{tx};

  // Ensure all queries are issued at once to make the test more interesting
  P.retain();

  // The middle query should fail; the surrounding two should succeed
  auto const id_1{P.insert("SELECT 1")};
  auto const id_f{P.insert("SELECT * FROM pg_nonexist")};
  auto const id_2{P.insert("SELECT 2")};

  // See that we can process the queries without stumbling over the error
  P.complete();

  // We should be able to get the first result, which preceeds the error
  auto const res_1{P.retrieve(id_1).at(0).at(0).as<int>()};
  PQXX_CHECK_EQUAL(res_1, 1, "Got wrong result from pipeline.");

  // We should *not* get a result for the query behind the error
  {
    quiet_errorhandler d{conn};
    PQXX_CHECK_THROWS(
      P.retrieve(id_2).at(0).at(0).as<int>(), std::runtime_error,
      "Pipeline wrongly resumed after SQL error.");
  }

  // Now see that we get an exception when we touch the failed result
  {
    quiet_errorhandler d{conn};
    PQXX_CHECK_THROWS(
      P.retrieve(id_f), sql_error, "Pipeline failed to register SQL error.");
  }
}
} // namespace


PQXX_REGISTER_TEST(test_072);
