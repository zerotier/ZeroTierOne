#include <cerrno>
#include <cstring>
#include <iostream>

#include <pqxx/notification>
#include <pqxx/transaction>
#include <pqxx/transactor>

#include "test_helpers.hxx"


// Example program for libpqxx.  Send notification to self, using a
// notification name with unusal characters, and without polling.
namespace
{
// Sample implementation of notification receiver.
class TestListener : public pqxx::notification_receiver
{
  bool m_done;

public:
  explicit TestListener(pqxx::connection &conn, std::string const &Name) :
          pqxx::notification_receiver(conn, Name), m_done(false)
  {}

  void operator()(std::string const &, int be_pid) override
  {
    m_done = true;
    PQXX_CHECK_EQUAL(
      be_pid, conn().backendpid(),
      "Got notification from wrong backend process.");

    std::cout << "Received notification: " << channel() << " pid=" << be_pid
              << std::endl;
  }

  bool done() const { return m_done; }
};


void test_078()
{
  pqxx::connection conn;

  std::string const NotifName{"my listener"};
  TestListener L{conn, NotifName};

  pqxx::perform([&conn, &L] {
    pqxx::work tx{conn};
    tx.exec0("NOTIFY " + tx.quote_name(L.channel()));
    tx.commit();
  });

  int notifs{0};
  for (int i{0}; (i < 20) and not L.done(); ++i)
  {
    PQXX_CHECK_EQUAL(notifs, 0, "Got unexpected notifications.");
    std::cout << ".";
    notifs = conn.await_notification();
  }
  std::cout << std::endl;

  PQXX_CHECK(L.done(), "No notification received.");
  PQXX_CHECK_EQUAL(notifs, 1, "Got unexpected number of notifications.");
}
} // namespace


PQXX_REGISTER_TEST(test_078);
