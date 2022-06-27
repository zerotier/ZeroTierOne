#include <pqxx/nontransaction>
#include <pqxx/subtransaction>

#include "../test_helpers.hxx"

namespace
{
void test_simultaneous_transactions()
{
  pqxx::connection conn;

  pqxx::nontransaction n1{conn};
  PQXX_CHECK_THROWS(
    pqxx::nontransaction n2{conn}, std::logic_error,
    "Allowed to open simultaneous nontransactions.");
}


PQXX_REGISTER_TEST(test_simultaneous_transactions);
} // namespace
