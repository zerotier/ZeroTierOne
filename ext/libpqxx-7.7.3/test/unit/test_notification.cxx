#include <chrono>

#include <pqxx/internal/header-pre.hxx>

#include <pqxx/internal/wait.hxx>

#include <pqxx/internal/header-post.hxx>

#include <pqxx/nontransaction>
#include <pqxx/notification>

#include "../test_helpers.hxx"

namespace
{
class TestReceiver final : public pqxx::notification_receiver
{
public:
  std::string payload;
  int backend_pid;

  TestReceiver(pqxx::connection &c, std::string const &channel_name) :
          pqxx::notification_receiver(c, channel_name),
          payload(),
          backend_pid(0)
  {}

  virtual void
  operator()(std::string const &payload_string, int backend) override
  {
    this->payload = payload_string;
    this->backend_pid = backend;
  }
};


void test_receive(
  pqxx::transaction_base &t, std::string const &channel,
  char const payload[] = nullptr)
{
  pqxx::connection &conn(t.conn());

  std::string SQL{"NOTIFY \"" + channel + "\""};
  if (payload != nullptr)
    SQL += ", " + t.quote(payload);

  TestReceiver receiver{t.conn(), channel};

  // Clear out any previously pending notifications that might otherwise
  // confuse the test.
  conn.get_notifs();

  // Notify, and receive.
  t.exec(SQL);
  t.commit();

  int notifs{0};
  for (int i{0}; (i < 10) and (notifs == 0);
       ++i, pqxx::internal::wait_for(1'000'000u))
    notifs = conn.get_notifs();

  PQXX_CHECK_EQUAL(notifs, 1, "Got wrong number of notifications.");
  PQXX_CHECK_EQUAL(receiver.backend_pid, conn.backendpid(), "Bad pid.");
  if (payload == nullptr)
    PQXX_CHECK(std::empty(receiver.payload), "Unexpected payload.");
  else
    PQXX_CHECK_EQUAL(receiver.payload, payload, "Bad payload.");
}


void test_notification()
{
  pqxx::connection conn;
  TestReceiver receiver(conn, "mychannel");
  PQXX_CHECK_EQUAL(receiver.channel(), "mychannel", "Bad channel.");

  pqxx::work tx{conn};
  test_receive(tx, "channel1");

  pqxx::nontransaction u(conn);
  test_receive(u, "channel2", "payload");
}


PQXX_REGISTER_TEST(test_notification);
} // namespace
