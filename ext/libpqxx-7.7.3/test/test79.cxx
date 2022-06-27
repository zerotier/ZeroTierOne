#include <cerrno>
#include <cstring>
#include <iostream>

#include <pqxx/notification>
#include <pqxx/transaction>
#include <pqxx/transactor>

#include "test_helpers.hxx"


// Example program for libpqxx.  Test waiting for notification with timeout.
namespace
{
// Sample implementation of notification receiver.
class TestListener final : public pqxx::notification_receiver
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
      be_pid, conn().backendpid(), "Notification came from wrong backend.");

    std::cout << "Received notification: " << channel() << " pid=" << be_pid
              << std::endl;
  }

  bool done() const { return m_done; }
};


void test_079()
{
  pqxx::connection conn;

  std::string const NotifName{"mylistener"};
  TestListener L(conn, NotifName);

  // First see if the timeout really works: we're not expecting any notifs
  int notifs{conn.await_notification(0, 1)};
  PQXX_CHECK_EQUAL(notifs, 0, "Got unexpected notification.");

  pqxx::perform([&conn, &L] {
    pqxx::work tx{conn};
    tx.exec0("NOTIFY " + L.channel());
    tx.commit();
  });

  for (int i{0}; (i < 20) and not L.done(); ++i)
  {
    PQXX_CHECK_EQUAL(notifs, 0, "Got notifications, but no handler called.");
    std::cout << ".";
    notifs = conn.await_notification(1, 0);
  }
  std::cout << std::endl;

  PQXX_CHECK(L.done(), "No notifications received.");
  PQXX_CHECK_EQUAL(notifs, 1, "Got unexpected notifications.");
}
} // namespace


PQXX_REGISTER_TEST(test_079);
