/* libpqxx test runner.
 */
#include <cassert>
#include <iostream>
#include <list>
#include <map>
#include <new>
#include <stdexcept>
#include <string>

#include <pqxx/transaction>

#include "test_helpers.hxx"

namespace
{
inline std::string deref_field(pqxx::field const &f)
{
  return f.c_str();
}
} // namespace


namespace pqxx::test
{
test_failure::test_failure(
  std::string const &ffile, int fline, std::string const &desc) :
        std::logic_error(desc), m_file(ffile), m_line(fline)
{}

test_failure::~test_failure() noexcept = default;


/// Drop table, if it exists.
inline void drop_table(transaction_base &t, std::string const &table)
{
  t.exec("DROP TABLE IF EXISTS " + table);
}


[[noreturn]] void
check_notreached(char const file[], int line, std::string desc)
{
  throw test_failure(file, line, desc);
}


void check(
  char const file[], int line, bool condition, char const text[],
  std::string const &desc)
{
  if (not condition)
    throw test_failure(
      file, line, desc + " (failed expression: " + text + ")");
}


void expected_exception(std::string const &message)
{
  std::cout << "(Expected) " << message << std::endl;
}


std::string list_row(row Obj)
{
  return separated_list(", ", std::begin(Obj), std::end(Obj), deref_field);
}


std::string list_result(result Obj)
{
  if (std::empty(Obj))
    return "<empty>";
  return "{" +
         separated_list(
           "}\n{", std::begin(Obj), std::end(Obj),
           [](row r) { return list_row(r); }) +
         "}";
}


std::string list_result_iterator(result::const_iterator Obj)
{
  return "<iterator at " + to_string(Obj.rownumber()) + ">";
}


void create_pqxxevents(transaction_base &t)
{
  t.exec(
    "CREATE TEMP TABLE pqxxevents(year integer, event varchar) "
    "ON COMMIT PRESERVE ROWS");
  t.exec("INSERT INTO pqxxevents(year, event) VALUES (71, 'jtv')");
  t.exec("INSERT INTO pqxxevents(year, event) VALUES (38, 'time_t overflow')");
  t.exec(
    "INSERT INTO pqxxevents(year, event) VALUES (1, '''911'' WTC attack')");
  t.exec("INSERT INTO pqxxevents(year, event) VALUES (81, 'C:\\>')");
  t.exec(
    "INSERT INTO pqxxevents(year, event) VALUES (1978, 'bloody\t\tcold')");
  t.exec("INSERT INTO pqxxevents(year, event) VALUES (99, '')");
  t.exec("INSERT INTO pqxxevents(year, event) VALUES (2002, 'libpqxx')");
  t.exec(
    "INSERT INTO pqxxevents(year, event) "
    "VALUES (1989, 'Ode an die Freiheit')");
  t.exec(
    "INSERT INTO pqxxevents(year, event) VALUES (2001, 'New millennium')");
  t.exec("INSERT INTO pqxxevents(year, event) VALUES (1974, '')");
  t.exec("INSERT INTO pqxxevents(year, event) VALUES (97, 'Asian crisis')");
  t.exec(
    "INSERT INTO pqxxevents(year, event) VALUES (2001, 'A Space Odyssey')");
}
} // namespace pqxx::test


namespace
{
std::map<std::string const, pqxx::test::testfunc> *all_tests{nullptr};
} // namespace


namespace pqxx::test
{
void register_test(char const name[], pqxx::test::testfunc func)
{
  if (all_tests == nullptr)
  {
    all_tests = new std::map<std::string const, pqxx::test::testfunc>();
  }
  else
  {
    assert(all_tests->find(name) == all_tests->end());
  }
  (*all_tests)[name] = func;
}
} // namespace pqxx::test


int main(int argc, char const *argv[])
{
  char const *const test_name{(argc > 1) ? argv[1] : nullptr};

  int test_count = 0;
  std::list<std::string> failed;
  for (auto const &i : *all_tests)
    if (test_name == nullptr or std::string{test_name} == std::string{i.first})
    {
      std::cout << std::endl << "Running: " << i.first << std::endl;

      bool success = false;
      try
      {
        i.second();
        success = true;
      }
      catch (pqxx::test::test_failure const &e)
      {
        std::cerr << "Test failure in " + e.file() + " line " +
                       pqxx::to_string(e.line())
                  << ": " << e.what() << std::endl;
      }
      catch (std::bad_alloc const &)
      {
        std::cerr << "Out of memory!" << std::endl;
      }
      catch (pqxx::feature_not_supported const &e)
      {
        std::cerr << "Not testing unsupported feature: " << e.what()
                  << std::endl;
        success = true;
        --test_count;
      }
      catch (pqxx::sql_error const &e)
      {
        std::cerr << "SQL error: " << e.what() << std::endl
                  << "Query was: " << e.query() << std::endl;
      }
      catch (std::exception const &e)
      {
        std::cerr << "Exception: " << e.what() << std::endl;
      }
      catch (...)
      {
        std::cerr << "Unknown exception" << std::endl;
      }

      if (not success)
      {
        std::cerr << "FAILED: " << i.first << std::endl;
        failed.emplace_back(i.first);
      }
      ++test_count;
    }

  std::cout << "Ran " << test_count << " test(s).\n";

  if (not std::empty(failed))
  {
    std::cerr << "*** " << std::size(failed) << " test(s) failed: ***\n";
    for (auto const &i : failed) std::cerr << "\t" << i << '\n';
  }

  return int(std::size(failed));
}
