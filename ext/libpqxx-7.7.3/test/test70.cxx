#include <pqxx/pipeline>
#include <pqxx/transaction>

#include "test_helpers.hxx"

using namespace pqxx;


namespace
{
void TestPipeline(pipeline &P, int numqueries)
{
  std::string const Q{"SELECT * FROM generate_series(1, 10)"};
  result const Empty;
  PQXX_CHECK(std::empty(Empty), "Default-constructed result is not empty.");
  PQXX_CHECK(
    std::empty(Empty.query()), "Default-constructed result has query");

  P.retain();
  for (int i{numqueries}; i > 0; --i) P.insert(Q);
  P.resume();

  PQXX_CHECK(
    (numqueries == 0) || not std::empty(P), "pipeline::empty() is broken.");

  int res{0};
  result Prev;
  PQXX_CHECK_EQUAL(Prev, Empty, "Default-constructed results are not equal.");

  for (int i{numqueries}; i > 0; --i)
  {
    PQXX_CHECK(not std::empty(P), "Got no results from pipeline.");

    auto R{P.retrieve()};

    PQXX_CHECK_NOT_EQUAL(R.second, Empty, "Got empty result.");
    if (Prev != Empty)
      PQXX_CHECK_EQUAL(R.second, Prev, "Results to same query are different.");

    Prev = R.second;
    PQXX_CHECK_EQUAL(Prev, R.second, "Assignment breaks result equality.");
    PQXX_CHECK_EQUAL(R.second.query(), Q, "Result is for unexpected query.");

    if (res != 0)
      PQXX_CHECK_EQUAL(Prev[0][0].as<int>(), res, "Bad result from pipeline.");

    res = Prev[0][0].as<int>();
  }

  PQXX_CHECK(std::empty(P), "Pipeline was not empty after retrieval.");
}


// Test program for libpqxx.  Issue a query repeatedly through a pipeline, and
// compare results.  Use retain() and resume() for performance.
void test_070()
{
  connection conn;
  work tx{conn};
  pipeline P(tx);

  PQXX_CHECK(std::empty(P), "Pipeline is not empty initially.");

  // Try to confuse the pipeline by feeding it a query and flushing
  P.retain();
  std::string const Q{"SELECT * FROM pg_tables"};
  P.insert(Q);
  P.flush();

  PQXX_CHECK(std::empty(P), "Pipeline was not empty after flush().");

  // See if complete() breaks retain() as it should
  P.retain();
  P.insert(Q);
  PQXX_CHECK(not std::empty(P), "Pipeline was empty after insert().");
  P.complete();
  PQXX_CHECK(not std::empty(P), "complete() emptied pipeline.");

  PQXX_CHECK_EQUAL(
    P.retrieve().second.query(), Q, "Result is for wrong query.");

  PQXX_CHECK(std::empty(P), "Pipeline not empty after retrieve().");

  // See if retrieve() breaks retain() when it needs to
  P.retain();
  P.insert(Q);
  PQXX_CHECK_EQUAL(
    P.retrieve().second.query(), Q, "Got result for wrong query.");

  // See if regular retain()/resume() works
  for (int i{0}; i < 5; ++i) TestPipeline(P, i);

  // See if retrieve() fails on an empty pipeline, as it should
  quiet_errorhandler d(conn);
  PQXX_CHECK_THROWS_EXCEPTION(
    P.retrieve(), "Empty pipeline allows retrieve().");
}
} // namespace


PQXX_REGISTER_TEST(test_070);
