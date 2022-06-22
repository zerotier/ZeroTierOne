#include <iostream>
#include <map>

#include <pqxx/pipeline>
#include <pqxx/transaction>

#include "test_helpers.hxx"

using namespace pqxx;


// Test program for libpqxx.  Issue queries through a pipeline, and retrieve
// results both in-order and out-of-order.
namespace
{
using Exp = std::map<pipeline::query_id, int>;

template<typename PAIR> void checkresult(pipeline &P, PAIR c)
{
  result const r{P.retrieve(c.first)};
  int const val{r.at(0).at(0).as(int(0))};
  PQXX_CHECK_EQUAL(val, c.second, "Wrong result from pipeline.");
}


void test_071()
{
  connection conn;
  work tx{conn};
  pipeline P(tx);

  // Keep expected result for every query we issue
  Exp values;

  // Insert queries returning various numbers.
  for (int i{1}; i < 10; ++i) values[P.insert("SELECT " + to_string(i))] = i;

  // Retrieve results in query_id order, and compare to expected values
  for (auto &c : values) checkresult(P, c);

  PQXX_CHECK(std::empty(P), "Pipeline was not empty retrieving all results.");

  values.clear();

  // Insert more queries returning various numbers
  P.retain(20);
  for (int i{100}; i > 90; --i) values[P.insert("SELECT " + to_string(i))] = i;

  P.resume();

  // Retrieve results in reverse order
  for (auto c{std::rbegin(values)}; c != std::rend(values); ++c)
    checkresult(P, *c);

  values.clear();
  P.retain(10);
  for (int i{1010}; i > 1000; --i)
    values[P.insert("SELECT " + to_string(i))] = i;
  for (auto &c : values)
  {
    if (P.is_finished(c.first))
      std::cout << "Query #" << c.first << " completed despite retain()"
                << std::endl;
  }

  // See that all results are retrieved by complete()
  P.complete();
  for (auto &c : values)
    PQXX_CHECK(P.is_finished(c.first), "Query not finished after complete().");
}
} // namespace


PQXX_REGISTER_TEST(test_071);
