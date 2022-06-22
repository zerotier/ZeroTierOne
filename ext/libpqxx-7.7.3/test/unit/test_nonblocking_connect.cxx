#include <pqxx/transaction>

#include <pqxx/internal/wait.hxx>

#include "../test_helpers.hxx"


namespace
{
void test_nonblocking_connect()
{
  pqxx::connecting nbc;
  while (not nbc.done())
  {
    pqxx::internal::wait_fd(
      nbc.sock(), nbc.wait_to_read(), nbc.wait_to_write());
    nbc.process();
  }

  pqxx::connection conn{std::move(nbc).produce()};
  pqxx::work tx{conn};
  PQXX_CHECK_EQUAL(tx.query_value<int>("SELECT 10"), 10, "Bad value!?");
}


PQXX_REGISTER_TEST(test_nonblocking_connect);
} // namespace
