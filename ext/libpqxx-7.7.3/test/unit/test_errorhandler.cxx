#include <vector>

#include <pqxx/connection>
#include <pqxx/errorhandler>

#include "../test_helpers.hxx"

namespace
{
class TestErrorHandler final : public pqxx::errorhandler
{
public:
  TestErrorHandler(
    pqxx::connection &c, std::vector<TestErrorHandler *> &activated_handlers,
    bool retval = true) :
          pqxx::errorhandler(c),
          return_value(retval),
          message(),
          handler_list(activated_handlers)
  {}

  bool operator()(char const msg[]) noexcept override
  {
    message = std::string{msg};
    handler_list.push_back(this);
    return return_value;
  }

  bool return_value;
  std::string message;
  std::vector<TestErrorHandler *> &handler_list;
};
} // namespace


namespace pqxx
{
template<> struct nullness<TestErrorHandler *>
{
  // clang warns about these being unused.  And clang 6 won't accept a
  // [[maybe_unused]] attribute on them either!

  // static inline constexpr bool has_null{true};
  // static inline constexpr bool always_null{false};

  static constexpr bool is_null(TestErrorHandler *e) noexcept
  {
    return e == nullptr;
  }
  static constexpr TestErrorHandler *null() noexcept { return nullptr; }
};


template<> struct string_traits<TestErrorHandler *>
{
  static constexpr std::size_t size_buffer(TestErrorHandler *const &) noexcept
  {
    return 100;
  }

  static char *into_buf(char *begin, char *end, TestErrorHandler *const &value)
  {
    std::string text{"TestErrorHandler at " + pqxx::to_string(value)};
    if (pqxx::internal::cmp_greater_equal(std::size(text), end - begin))
      throw conversion_overrun{"Not enough buffer for TestErrorHandler."};
    std::memcpy(begin, text.c_str(), std::size(text) + 1);
    return begin + std::size(text) + 1;
  }
};
} // namespace pqxx


namespace
{
void test_process_notice_calls_errorhandler(pqxx::connection &c)
{
  std::vector<TestErrorHandler *> dummy;
  TestErrorHandler handler(c, dummy);
  c.process_notice("Error!\n");
  PQXX_CHECK_EQUAL(handler.message, "Error!\n", "Error not handled.");
}


void test_error_handlers_get_called_newest_to_oldest(pqxx::connection &c)
{
  std::vector<TestErrorHandler *> handlers;
  TestErrorHandler h1(c, handlers);
  TestErrorHandler h2(c, handlers);
  TestErrorHandler h3(c, handlers);
  c.process_notice("Warning.\n");
  PQXX_CHECK_EQUAL(h3.message, "Warning.\n", "Message not handled.");
  PQXX_CHECK_EQUAL(h2.message, "Warning.\n", "Broken handling chain.");
  PQXX_CHECK_EQUAL(h1.message, "Warning.\n", "Insane handling chain.");
  PQXX_CHECK_EQUAL(std::size(handlers), 3u, "Wrong number of handler calls.");
  PQXX_CHECK_EQUAL(&h3, handlers[0], "Unexpected handling order.");
  PQXX_CHECK_EQUAL(&h2, handlers[1], "Insane handling order.");
  PQXX_CHECK_EQUAL(&h1, handlers[2], "Impossible handling order.");
}

void test_returning_false_stops_error_handling(pqxx::connection &c)
{
  std::vector<TestErrorHandler *> handlers;
  TestErrorHandler starved(c, handlers);
  TestErrorHandler blocker(c, handlers, false);
  c.process_notice("Error output.\n");
  PQXX_CHECK_EQUAL(std::size(handlers), 1u, "Handling chain was not stopped.");
  PQXX_CHECK_EQUAL(handlers[0], &blocker, "Wrong handler got message.");
  PQXX_CHECK_EQUAL(blocker.message, "Error output.\n", "Didn't get message.");
  PQXX_CHECK_EQUAL(starved.message, "", "Message received; it shouldn't be.");
}

void test_destroyed_error_handlers_are_not_called(pqxx::connection &c)
{
  std::vector<TestErrorHandler *> handlers;
  {
    TestErrorHandler doomed(c, handlers);
  }
  c.process_notice("Unheard output.");
  PQXX_CHECK(
    std::empty(handlers), "Message was received on dead errorhandler.");
}

void test_destroying_connection_unregisters_handlers()
{
  TestErrorHandler *survivor;
  std::vector<TestErrorHandler *> handlers;
  {
    pqxx::connection c;
    survivor = new TestErrorHandler(c, handlers);
  }
  // Make some pointless use of survivor just to prove that this doesn't crash.
  (*survivor)("Hi");
  PQXX_CHECK_EQUAL(
    std::size(handlers), 1u, "Ghost of dead ex-connection haunts handler.");
  delete survivor;
}


class MinimalErrorHandler final : public pqxx::errorhandler
{
public:
  explicit MinimalErrorHandler(pqxx::connection &c) : pqxx::errorhandler(c) {}
  bool operator()(char const[]) noexcept override { return true; }
};


void test_get_errorhandlers(pqxx::connection &c)
{
  std::unique_ptr<MinimalErrorHandler> eh3;
  auto const handlers_before{c.get_errorhandlers()};
  std::size_t const base_handlers{std::size(handlers_before)};

  {
    MinimalErrorHandler eh1(c);
    auto const handlers_with_eh1{c.get_errorhandlers()};
    PQXX_CHECK_EQUAL(
      std::size(handlers_with_eh1), base_handlers + 1,
      "Registering a handler didn't create exactly one handler.");
    PQXX_CHECK_EQUAL(
      std::size_t(*std::rbegin(handlers_with_eh1)), std::size_t(&eh1),
      "Wrong handler or wrong order.");

    {
      MinimalErrorHandler eh2(c);
      auto const handlers_with_eh2{c.get_errorhandlers()};
      PQXX_CHECK_EQUAL(
        std::size(handlers_with_eh2), base_handlers + 2,
        "Adding second handler didn't work.");
      PQXX_CHECK_EQUAL(
        std::size_t(*(std::rbegin(handlers_with_eh2) + 1)), std::size_t(&eh1),
        "Second handler upset order.");
      PQXX_CHECK_EQUAL(
        std::size_t(*std::rbegin(handlers_with_eh2)), std::size_t(&eh2),
        "Second handler isn't right.");
    }
    auto const handlers_without_eh2{c.get_errorhandlers()};
    PQXX_CHECK_EQUAL(
      std::size(handlers_without_eh2), base_handlers + 1,
      "Handler destruction produced wrong-sized handlers list.");
    PQXX_CHECK_EQUAL(
      std::size_t(*std::rbegin(handlers_without_eh2)), std::size_t(&eh1),
      "Destroyed wrong handler.");

    eh3 = std::make_unique<MinimalErrorHandler>(c);
    auto const handlers_with_eh3{c.get_errorhandlers()};
    PQXX_CHECK_EQUAL(
      std::size(handlers_with_eh3), base_handlers + 2,
      "Remove-and-add breaks.");
    PQXX_CHECK_EQUAL(
      std::size_t(*std::rbegin(handlers_with_eh3)), std::size_t(eh3.get()),
      "Added wrong third handler.");
  }
  auto const handlers_without_eh1{c.get_errorhandlers()};
  PQXX_CHECK_EQUAL(
    std::size(handlers_without_eh1), base_handlers + 1,
    "Destroying oldest handler didn't work as expected.");
  PQXX_CHECK_EQUAL(
    std::size_t(*std::rbegin(handlers_without_eh1)), std::size_t(eh3.get()),
    "Destroyed wrong handler.");

  eh3.reset();

  auto const handlers_without_all{c.get_errorhandlers()};
  PQXX_CHECK_EQUAL(
    std::size(handlers_without_all), base_handlers,
    "Destroying all custom handlers didn't work as expected.");
}


void test_errorhandler()
{
  pqxx::connection conn;
  test_process_notice_calls_errorhandler(conn);
  test_error_handlers_get_called_newest_to_oldest(conn);
  test_returning_false_stops_error_handling(conn);
  test_destroyed_error_handlers_are_not_called(conn);
  test_destroying_connection_unregisters_handlers();
  test_get_errorhandlers(conn);
}


PQXX_REGISTER_TEST(test_errorhandler);
} // namespace
