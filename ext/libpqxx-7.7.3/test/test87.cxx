#include "pqxx/config-public-compiler.h"
#include <cctype>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <iostream>

#include <pqxx/internal/header-pre.hxx>

#include <pqxx/internal/wait.hxx>

#include <pqxx/internal/header-post.hxx>

#include <pqxx/notification>
#include <pqxx/transaction>
#include <pqxx/transactor>

#include "test_helpers.hxx"


// Test program for libpqxx.  Send notification to self, and wait on the
// socket's connection for it to come in.  In a simple situation you'd use
// connection::await_notification() for this, but that won't let you wait for
// multiple sockets.
namespace
{
// Sample implementation of notification receiver.
class TestListener final : public pqxx::notification_receiver
{
  bool m_done;

public:
  explicit TestListener(pqxx::connection &conn, std::string Name) :
          pqxx::notification_receiver(conn, Name), m_done(false)
  {}

  void operator()(std::string const &, int be_pid) override
  {
    m_done = true;
    PQXX_CHECK_EQUAL(
      be_pid, conn().backendpid(),
      "Notification came from wrong backend process.");

    std::cout << "Received notification: " << channel() << " pid=" << be_pid
              << std::endl;
  }

  bool done() const { return m_done; }
};


void test_087()
{
  pqxx::connection conn;

  std::string const NotifName{"my notification"};
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

    pqxx::internal::wait_fd(conn.sock(), true, false);
    notifs = conn.get_notifs();
  }
  std::cout << std::endl;

  PQXX_CHECK(L.done(), "No notification received.");
  PQXX_CHECK_EQUAL(notifs, 1, "Got unexpected number of notifications.");
}
} // namespace


PQXX_REGISTER_TEST(test_087);
