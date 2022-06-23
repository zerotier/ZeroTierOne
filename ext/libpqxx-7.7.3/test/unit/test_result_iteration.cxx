#include <pqxx/stream_to>
#include <pqxx/transaction>

#include "../test_helpers.hxx"

namespace
{
void test_result_iteration()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::result r{tx.exec("SELECT generate_series(1, 3)")};

  PQXX_CHECK(std::end(r) != std::begin(r), "Broken begin/end.");
  PQXX_CHECK(std::rend(r) != std::rbegin(r), "Broken rbegin/rend.");

  PQXX_CHECK(std::cbegin(r) == std::begin(r), "Wrong cbegin.");
  PQXX_CHECK(std::cend(r) == std::end(r), "Wrong cend.");
  PQXX_CHECK(std::crbegin(r) == std::rbegin(r), "Wrong crbegin.");
  PQXX_CHECK(std::crend(r) == std::rend(r), "Wrong crend.");

  PQXX_CHECK_EQUAL(r.front().front().as<int>(), 1, "Unexpected front().");
  PQXX_CHECK_EQUAL(r.back().front().as<int>(), 3, "Unexpected back().");
}


void test_result_iter()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::result r{tx.exec("SELECT generate_series(1, 3)")};

  int total{0};
  for (auto const &[i] : r.iter<int>()) total += i;
  PQXX_CHECK_EQUAL(total, 6, "iter() loop did not get the right values.");
}


void test_result_iterator_swap()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::result r{tx.exec("SELECT generate_series(1, 3)")};

  auto head{std::begin(r)}, next{head + 1};
  head.swap(next);
  PQXX_CHECK_EQUAL(head[0].as<int>(), 2, "Result iterator swap is wrong.");
  PQXX_CHECK_EQUAL(next[0].as<int>(), 1, "Result iterator swap is crazy.");

  auto tail{std::rbegin(r)}, prev{tail + 1};
  tail.swap(prev);
  PQXX_CHECK_EQUAL(tail[0].as<int>(), 2, "Reverse iterator swap is wrong.");
  PQXX_CHECK_EQUAL(prev[0].as<int>(), 3, "Reverse iterator swap is crazy.");
}


void test_result_iterator_assignment()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::result r{tx.exec("SELECT generate_series(1, 3)")};

  pqxx::result::const_iterator fwd;
  pqxx::result::const_reverse_iterator rev;

  fwd = std::begin(r);
  PQXX_CHECK_EQUAL(
    fwd[0].as<int>(), std::begin(r)[0].as<int>(),
    "Result iterator assignment is wrong.");

  rev = std::rbegin(r);
  PQXX_CHECK_EQUAL(
    rev[0].as<int>(), std::rbegin(r)[0].as<int>(),
    "Reverse iterator assignment is wrong.");
}


void check_employee(std::string name, int salary)
{
  PQXX_CHECK(name == "x" or name == "y" or name == "z", "Unknown name.");
  PQXX_CHECK(
    salary == 1000 or salary == 1200 or salary == 1500, "Unknown salary.");
}


void test_result_for_each()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  tx.exec0("CREATE TEMP TABLE employee(name varchar, salary int)");
  auto fill{pqxx::stream_to::table(tx, {"employee"}, {"name", "salary"})};
  fill.write_values("x", 1000);
  fill.write_values("y", 1200);
  fill.write_values("z", 1500);
  fill.complete();

  auto const res{tx.exec("SELECT name, salary FROM employee ORDER BY name")};

  // Use for_each with a function.
  res.for_each(check_employee);

  // Use for_each with a simple lambda.
  res.for_each(
    [](std::string name, int salary) { check_employee(name, salary); });

  // Use for_each with a lambda closure.
  std::string names{};
  int total{0};

  res.for_each([&names, &total](std::string name, int salary) {
    names.append(name);
    total += salary;
  });
  PQXX_CHECK_EQUAL(
    names, "xyz", "result::for_each did not accumulate names correctly.");
  PQXX_CHECK_EQUAL(total, 1000 + 1200 + 1500, "Salaries added up wrong.");

  // In addition to regular conversions, you can receive arguments as
  // string_view, or as references.
  names.clear();
  total = 0;
  res.for_each([&names, &total](std::string_view &&name, int const &salary) {
    names.append(name);
    total += salary;
  });
  PQXX_CHECK_EQUAL(
    names, "xyz", "result::for_each did not accumulate names correctly.");
  PQXX_CHECK_EQUAL(total, 1000 + 1200 + 1500, "Salaries added up wrong.");
}


PQXX_REGISTER_TEST(test_result_iteration);
PQXX_REGISTER_TEST(test_result_iter);
PQXX_REGISTER_TEST(test_result_iterator_swap);
PQXX_REGISTER_TEST(test_result_iterator_assignment);
PQXX_REGISTER_TEST(test_result_for_each);
} // namespace
