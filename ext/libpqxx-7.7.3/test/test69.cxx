#include <iostream>

#include <pqxx/pipeline>
#include <pqxx/transaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Test program for libpqxx.  Issue a query repeatedly through a pipeline, and
// compare results.
namespace
{
void TestPipeline(pipeline &P, int numqueries)
{
  std::string const Q{"SELECT 99"};

  for (int i{numqueries}; i > 0; --i) P.insert(Q);

  PQXX_CHECK(
    (numqueries == 0) or not std::empty(P), "pipeline::empty() is broken.");

  int res{0};
  for (int i{numqueries}; i > 0; --i)
  {
    PQXX_CHECK(
      not std::empty(P), "Got wrong number of queries from pipeline.");

    auto R{P.retrieve()};

    if (res != 0)
      PQXX_CHECK_EQUAL(
        R.second[0][0].as<int>(), res,
        "Got unexpected result out of pipeline.");

    res = R.second[0][0].as<int>();
  }

  PQXX_CHECK(std::empty(P), "Pipeline not empty after retrieval.");
}


void test_069()
{
  connection conn;
  work tx{conn};
  pipeline P(tx);
  PQXX_CHECK(std::empty(P), "Pipeline is not empty initially.");
  for (int i{0}; i < 5; ++i) TestPipeline(P, i);
}


PQXX_REGISTER_TEST(test_069);
} // namespace
