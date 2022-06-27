#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>

#include <pqxx/internal/header-pre.hxx>

#include <pqxx/internal/wait.hxx>

#include <pqxx/internal/header-post.hxx>

#include <pqxx/notification>
#include <pqxx/transaction>
#include <pqxx/transactor>

#include "test_helpers.hxx"

using namespace pqxx;

// Example program for libpqxx.  Send notification to self.

namespace
{
int Backend_PID{0};


// Sample implementation of notification receiver.
class TestListener final : public notification_receiver
{
  bool m_done;

public:
  explicit TestListener(connection &conn) :
          notification_receiver(conn, "listen"), m_done(false)
  {}

  virtual void operator()(std::string const &, int be_pid) override
  {
    m_done = true;
    PQXX_CHECK_EQUAL(
      be_pid, Backend_PID, "Notification came from wrong backend process.");
  }

  bool done() const { return m_done; }
};


void test_004()
{
  connection conn;

  TestListener L{conn};
  // Trigger our notification receiver.
  perform([&conn, &L] {
    work tx(conn);
    tx.exec0("NOTIFY " + conn.quote_name(L.channel()));
    Backend_PID = conn.backendpid();
    tx.commit();
  });

  int notifs{0};
  for (int i{0}; (i < 20) and not L.done(); ++i)
  {
    PQXX_CHECK_EQUAL(notifs, 0, "Got unexpected notifications.");
    // Sleep for one second.  I'm not proud of this, but how does one inject
    // a change to the built-in clock in a static language?
    pqxx::internal::wait_for(1'000'000u);
    notifs = conn.get_notifs();
  }

  PQXX_CHECK_NOT_EQUAL(L.done(), false, "No notification received.");
  PQXX_CHECK_EQUAL(notifs, 1, "Got too many notifications.");
}


PQXX_REGISTER_TEST(test_004);
} // namespace
