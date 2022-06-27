/** Implementation of the pqxx::connection class.
 *
 * pqxx::connection encapsulates a connection to a database.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>

// For fcntl().
#if __has_include(<fcntl.h>)
#  include <fcntl.h>
#endif
#if __has_include(<unistd.h>)
#  include <unistd.h>
#endif

// For ioctlsocket().
#if defined(_WIN32) && __has_include(<winsock2.h>)
#  include <winsock2.h>
#endif

extern "C"
{
#include <libpq-fe.h>
}

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/binarystring.hxx"
#include "pqxx/internal/wait.hxx"
#include "pqxx/nontransaction.hxx"
#include "pqxx/notification.hxx"
#include "pqxx/pipeline.hxx"
#include "pqxx/result.hxx"
#include "pqxx/strconv.hxx"
#include "pqxx/transaction.hxx"

#include "pqxx/internal/gates/errorhandler-connection.hxx"
#include "pqxx/internal/gates/result-connection.hxx"
#include "pqxx/internal/gates/result-creation.hxx"

#include "pqxx/internal/header-post.hxx"


extern "C"
{
  // The PQnoticeProcessor that receives an error or warning from libpq and
  // sends it to the appropriate connection for processing.
  void pqxx_notice_processor(void *conn, char const *msg) noexcept
  {
    reinterpret_cast<pqxx::connection *>(conn)->process_notice(msg);
  }


  // There's no way in libpq to disable a connection's notice processor.  So,
  // set an inert one to get the same effect.
  void inert_notice_processor(void *, char const *) noexcept {}
} // extern "C"

using namespace std::literals;

std::string PQXX_COLD
pqxx::encrypt_password(char const user[], char const password[])
{
  std::unique_ptr<char, std::function<void(char *)>> p{
    PQencryptPassword(password, user), PQfreemem};
  return {p.get()};
}


pqxx::connection::connection(connection &&rhs) :
        m_conn{rhs.m_conn}, m_unique_id{rhs.m_unique_id}
{
  rhs.check_movable();
  rhs.m_conn = nullptr;
}


pqxx::connection::connection(
  connection::connect_mode, zview connection_string) :
        m_conn{PQconnectStart(connection_string.c_str())}
{
  if (m_conn == nullptr)
    throw std::bad_alloc{};
  if (status() == CONNECTION_BAD)
    throw pqxx::broken_connection{PQerrorMessage(m_conn)};
}


std::pair<bool, bool> pqxx::connection::poll_connect()
{
  switch (PQconnectPoll(m_conn))
  {
  case PGRES_POLLING_FAILED:
    throw pqxx::broken_connection{PQerrorMessage(m_conn)};
  case PGRES_POLLING_READING: return std::make_pair(true, false);
  case PGRES_POLLING_WRITING: return std::make_pair(false, true);
  case PGRES_POLLING_OK:
    if (not is_open())
      throw pqxx::broken_connection{PQerrorMessage(m_conn)};
    return std::make_pair(false, false);
  case PGRES_POLLING_ACTIVE:
    throw internal_error{
      "Nonblocking connection poll returned obsolete 'active' state."};
  default:
    throw internal_error{
      "Nonblocking connection poll returned unknown value."};
  }
}

void pqxx::connection::complete_init()
{
  if (m_conn == nullptr)
    throw std::bad_alloc{};
  try
  {
    if (not is_open())
      throw broken_connection{PQerrorMessage(m_conn)};

    set_up_state();
  }
  catch (std::exception const &)
  {
    PQfinish(m_conn);
    m_conn = nullptr;
    throw;
  }
}


void pqxx::connection::init(char const options[])
{
  m_conn = PQconnectdb(options);
  complete_init();
}


void pqxx::connection::init(char const *params[], char const *values[])
{
  m_conn = PQconnectdbParams(params, values, 0);
  complete_init();
}


void pqxx::connection::check_movable() const
{
  if (m_trans)
    throw pqxx::usage_error{"Moving a connection with a transaction open."};
  if (not std::empty(m_errorhandlers))
    throw pqxx::usage_error{
      "Moving a connection with error handlers registered."};
  if (not std::empty(m_receivers))
    throw pqxx::usage_error{
      "Moving a connection with notification receivers registered."};
}


void pqxx::connection::check_overwritable() const
{
  if (m_trans)
    throw pqxx::usage_error{
      "Moving a connection onto one with a transaction open."};
  if (not std::empty(m_errorhandlers))
    throw pqxx::usage_error{
      "Moving a connection onto one with error handlers registered."};
  if (not std::empty(m_receivers))
    throw usage_error{
      "Moving a connection onto one "
      "with notification receivers registered."};
}


pqxx::connection &pqxx::connection::operator=(connection &&rhs)
{
  check_overwritable();
  rhs.check_movable();

  close();

  m_conn = std::exchange(rhs.m_conn, nullptr);
  m_unique_id = rhs.m_unique_id;

  return *this;
}


pqxx::result pqxx::connection::make_result(
  internal::pq::PGresult *pgr, std::shared_ptr<std::string> const &query,
  std::string_view desc)
{
  if (pgr == nullptr)
  {
    if (is_open())
      throw failure(err_msg());
    else
      throw broken_connection{"Lost connection to the database server."};
  }
  internal::encoding_group enc;
  try
  {
    enc = internal::enc_group(encoding_id());
  }
  catch (std::exception const &)
  {
    // Don't let the PGresult leak.
    // TODO: Can we just accept a unique_ptr instead?
    internal::clear_result(pgr);
    throw;
  }
  auto const r{pqxx::internal::gate::result_creation::create(pgr, query, enc)};
  pqxx::internal::gate::result_creation{r}.check_status(desc);
  return r;
}


int PQXX_COLD pqxx::connection::backendpid() const &noexcept
{
  return (m_conn == nullptr) ? 0 : PQbackendPID(m_conn);
}


namespace
{
PQXX_PURE int socket_of(::pqxx::internal::pq::PGconn const *c) noexcept
{
  return (c == nullptr) ? -1 : PQsocket(c);
}
} // namespace


int pqxx::connection::sock() const &noexcept
{
  return socket_of(m_conn);
}


int PQXX_COLD pqxx::connection::protocol_version() const noexcept
{
  return (m_conn == nullptr) ? 0 : PQprotocolVersion(m_conn);
}


int PQXX_COLD pqxx::connection::server_version() const noexcept
{
  return PQserverVersion(m_conn);
}


void pqxx::connection::set_variable(
  std::string_view var, std::string_view value) &
{
  exec(internal::concat("SET ", quote_name(var), "=", value));
}


std::string pqxx::connection::get_variable(std::string_view var)
{
  return exec(internal::concat("SHOW ", quote_name(var)))
    .at(0)
    .at(0)
    .as(std::string{});
}


std::string pqxx::connection::get_var(std::string_view var)
{
  // (Variables can't be null, so far as I can make out.)
  return exec(internal::concat("SHOW "sv, quote_name(var)))[0][0]
    .as<std::string>();
}


/** Set up various parts of logical connection state that may need to be
 * recovered because the physical connection to the database was lost and is
 * being reset, or that may not have been initialized yet.
 */
void pqxx::connection::set_up_state()
{
  if (auto const proto_ver{protocol_version()}; proto_ver < 3)
  {
    if (proto_ver == 0)
      throw broken_connection{"No connection."};
    else
      throw feature_not_supported{
        "Unsupported frontend/backend protocol version; 3.0 is the minimum."};
  }

  if (server_version() <= 90000)
    throw feature_not_supported{
      "Unsupported server version; 9.0 is the minimum."};

  // The default notice processor in libpq writes to stderr.  Ours does
  // nothing.
  // If the caller registers an error handler, this gets replaced with an
  // error handler that walks down the connection's chain of handlers.  We
  // don't do that by default because there's a danger: libpq may call the
  // notice processor via a result object, even after the connection has been
  // destroyed and the handlers list no longer exists.
  PQXX_LIKELY
  PQsetNoticeProcessor(m_conn, inert_notice_processor, nullptr);
}


bool pqxx::connection::is_open() const noexcept
{
  return status() == CONNECTION_OK;
}


void pqxx::connection::process_notice_raw(char const msg[]) noexcept
{
  if ((msg == nullptr) or (*msg == '\0'))
    return;
  auto const rbegin = std::crbegin(m_errorhandlers),
             rend = std::crend(m_errorhandlers);
  for (auto i{rbegin}; (i != rend) and (**i)(msg); ++i)
    ;
}


void pqxx::connection::process_notice(char const msg[]) noexcept
{
  if (msg == nullptr)
    return;
  zview const view{msg};
  if (std::empty(view))
    return;
  else if (msg[std::size(view) - 1] == '\n')
    process_notice_raw(msg);
  else
    // Newline is missing.  Let the zview version of the code add it.
    PQXX_UNLIKELY
  process_notice(view);
}


void pqxx::connection::process_notice(zview msg) noexcept
{
  if (std::empty(msg))
    return;
  else if (msg[std::size(msg) - 1] == '\n')
    process_notice_raw(msg.c_str());
  else
    try
    {
      // Add newline.
      std::string buf;
      buf.reserve(std::size(msg) + 1);
      buf.assign(msg);
      buf.push_back('\n');
      process_notice_raw(buf.c_str());
    }
    catch (std::exception const &)
    {
      // If nothing else works, try writing the message without the newline.
      PQXX_UNLIKELY
      process_notice_raw(msg.c_str());
    }
}


void PQXX_COLD pqxx::connection::trace(FILE *out) noexcept
{
  if (m_conn)
  {
    if (out)
      PQtrace(m_conn, out);
    else
      PQuntrace(m_conn);
  }
}


void PQXX_COLD pqxx::connection::add_receiver(pqxx::notification_receiver *n)
{
  if (n == nullptr)
    throw argument_error{"Null receiver registered"};

  // Add to receiver list and attempt to start listening.
  auto const p{m_receivers.find(n->channel())};
  auto const new_value{receiver_list::value_type{n->channel(), n}};

  if (p == std::end(m_receivers))
  {
    // Not listening on this event yet, start doing so.
    auto const lq{std::make_shared<std::string>(
      internal::concat("LISTEN ", quote_name(n->channel())))};
    make_result(PQexec(m_conn, lq->c_str()), lq, *lq);
    m_receivers.insert(new_value);
  }
  else
  {
    m_receivers.insert(p, new_value);
  }
}


void PQXX_COLD
pqxx::connection::remove_receiver(pqxx::notification_receiver *T) noexcept
{
  if (T == nullptr)
    return;

  try
  {
    auto needle{
      std::pair<std::string const, notification_receiver *>{T->channel(), T}};
    auto R{m_receivers.equal_range(needle.first)};
    auto i{find(R.first, R.second, needle)};

    if (i == R.second)
    {
      PQXX_UNLIKELY
      process_notice(internal::concat(
        "Attempt to remove unknown receiver '", needle.first, "'"));
    }
    else
    {
      // Erase first; otherwise a notification for the same receiver may yet
      // come in and wreak havoc.  Thanks Dragan Milenkovic.
      bool const gone{R.second == ++R.first};
      m_receivers.erase(i);
      if (gone)
        exec(internal::concat("UNLISTEN ", quote_name(needle.first)).c_str());
    }
  }
  catch (std::exception const &e)
  {
    PQXX_UNLIKELY
    process_notice(e.what());
  }
}


bool pqxx::connection::consume_input() noexcept
{
  return PQconsumeInput(m_conn) != 0;
}


bool pqxx::connection::is_busy() const noexcept
{
  return PQisBusy(m_conn) != 0;
}


void PQXX_COLD pqxx::connection::cancel_query()
{
  using pointer = std::unique_ptr<PGcancel, std::function<void(PGcancel *)>>;
  pointer cancel{PQgetCancel(m_conn), PQfreeCancel};
  if (cancel == nullptr)
    PQXX_UNLIKELY
  throw std::bad_alloc{};

  std::array<char, 500u> errbuf;
  auto const err{errbuf.data()};
  auto const c{
    PQcancel(cancel.get(), err, static_cast<int>(std::size(errbuf)))};
  if (c == 0)
    PQXX_UNLIKELY
  throw pqxx::sql_error{std::string{err, std::size(errbuf)}, "[cancel]"};
}


namespace
{
// C++20: std::span?
/// Get error string for a given @c errno value.
template<std::size_t BYTES>
char const *PQXX_COLD
error_string(int err_num, std::array<char, BYTES> &buffer)
{
  // Not entirely clear whether strerror_s will be in std or global namespace.
  using namespace std;

#if defined(PQXX_HAVE_STERROR_S) || defined(PQXX_HAVE_STRERROR_R)
#  if defined(PQXX_HAVE_STRERROR_S)
  auto const err_result{strerror_s(std::data(buffer), BYTES, err_num)};
#  else
  auto const err_result{strerror_r(err_num, std::data(buffer), BYTES)};
#  endif
  if constexpr (std::is_same_v<pqxx::strip_t<decltype(err_result)>, char *>)
  {
    // GNU version of strerror_r; returns the error string, which may or may
    // not reside within buffer.
    return err_result;
  }
  else
  {
    // Either strerror_s or POSIX strerror_r; returns an error code.
    // Sorry for being lazy here: Not reporting error string for the case
    // where we can't retrieve an error string.
    if (err_result == 0)
      return std::data(buffer);
    else
      return "Compound errors.";
  }

#else
  // Fallback case, hopefully for no actual platforms out there.
  pqxx::ignore_unused(err_num, buffer);
  return "(No error information available.)";
#endif
}
} // namespace


#if defined(_WIN32) || __has_include(<fcntl.h>)
void pqxx::connection::set_blocking(bool block) &
{
  auto const fd{sock()};
#  if defined _WIN32
  unsigned long mode{not block};
  if (::ioctlsocket(fd, FIONBIO, &mode) != 0)
  {
    std::array<char, 200> errbuf;
    char const *err{error_string(WSAGetLastError(), errbuf)};
    throw broken_connection{
      internal::concat("Could not set socket's blocking mode: ", err)};
  }
#  else  // _WIN32
  std::array<char, 200> errbuf;
  auto flags{::fcntl(fd, F_GETFL, 0)};
  if (flags == -1)
  {
    char const *const err{error_string(errno, errbuf)};
    throw broken_connection{
      internal::concat("Could not get socket state: ", err)};
  }
  if (block)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;
  if (::fcntl(fd, F_SETFL, flags) == -1)
  {
    char const *const err{error_string(errno, errbuf)};
    throw broken_connection{
      internal::concat("Could not set socket's blocking mode: ", err)};
  }
#  endif // _WIN32
}
#endif // defined(_WIN32) || __has_include(<fcntl.h>)


void PQXX_COLD
pqxx::connection::set_verbosity(error_verbosity verbosity) &noexcept
{
  PQsetErrorVerbosity(m_conn, static_cast<PGVerbosity>(verbosity));
}


namespace
{
/// Unique pointer to PGnotify.
using notify_ptr = std::unique_ptr<PGnotify, std::function<void(PGnotify *)>>;


/// Get one notification from a connection, or null.
notify_ptr get_notif(pqxx::internal::pq::PGconn *conn)
{
  return notify_ptr(PQnotifies(conn), PQfreemem);
}
} // namespace


int pqxx::connection::get_notifs()
{
  if (not consume_input())
    throw broken_connection{"Connection lost."};

  // Even if somehow we receive notifications during our transaction, don't
  // deliver them.
  if (m_trans)
    PQXX_UNLIKELY
  return 0;

  int notifs = 0;
  for (auto N{get_notif(m_conn)}; N.get(); N = get_notif(m_conn))
  {
    notifs++;

    auto const Hit{m_receivers.equal_range(std::string{N->relname})};
    if (Hit.second != Hit.first)
    {
      std::string payload{N->extra};
      for (auto i{Hit.first}; i != Hit.second; ++i) try
        {
          (*i->second)(payload, N->be_pid);
        }
        catch (std::exception const &e)
        {
          try
          {
            process_notice(internal::concat(
              "Exception in notification receiver '", i->first,
              "': ", e.what(), "\n"));
          }
          catch (std::bad_alloc const &)
          {
            // Out of memory.  Try to get the message out in a more robust way.
            process_notice(
              "Exception in notification receiver, "
              "and also ran out of memory\n");
          }
          catch (std::exception const &)
          {
            process_notice(
              "Exception in notification receiver "
              "(compounded by other error)\n");
          }
        }
    }

    N.reset();
  }
  return notifs;
}


char const *PQXX_COLD pqxx::connection::dbname() const
{
  return PQdb(m_conn);
}


char const *PQXX_COLD pqxx::connection::username() const
{
  return PQuser(m_conn);
}


char const *PQXX_COLD pqxx::connection::hostname() const
{
  return PQhost(m_conn);
}


char const *PQXX_COLD pqxx::connection::port() const
{
  return PQport(m_conn);
}


char const *pqxx::connection::err_msg() const noexcept
{
  return (m_conn == nullptr) ? "No connection to database" :
                               PQerrorMessage(m_conn);
}


void PQXX_COLD pqxx::connection::register_errorhandler(errorhandler *handler)
{
  // Set notice processor on demand, i.e. only when the caller actually
  // registers an error handler.
  // We do this just to make it less likely that users fall into the trap
  // where a result object may hold a notice processor derived from its parent
  // connection which has already been destroyed.  Our notice processor goes
  // through the connection's list of error handlers.  If the connection object
  // has already been destroyed though, that list no longer exists.
  // By setting the notice processor on demand, we absolve users who never
  // register an error handler from ahving to care about this nasty subtlety.
  if (std::empty(m_errorhandlers))
    PQsetNoticeProcessor(m_conn, pqxx_notice_processor, this);
  m_errorhandlers.push_back(handler);
}


void PQXX_COLD
pqxx::connection::unregister_errorhandler(errorhandler *handler) noexcept
{
  // The errorhandler itself will take care of nulling its pointer to this
  // connection.
  m_errorhandlers.remove(handler);
  if (std::empty(m_errorhandlers))
    PQsetNoticeProcessor(m_conn, inert_notice_processor, nullptr);
}


std::vector<pqxx::errorhandler *>
  PQXX_COLD pqxx::connection::get_errorhandlers() const
{
  return {std::begin(m_errorhandlers), std::end(m_errorhandlers)};
}


pqxx::result
pqxx::connection::exec(std::string_view query, std::string_view desc)
{
  return exec(std::make_shared<std::string>(query), desc);
}


pqxx::result pqxx::connection::exec(
  std::shared_ptr<std::string> query, std::string_view desc)
{
  auto const res{make_result(PQexec(m_conn, query->c_str()), query, desc)};
  get_notifs();
  return res;
}


std::string pqxx::connection::encrypt_password(
  char const user[], char const password[], char const *algorithm)
{
#if defined(PQXX_HAVE_PQENCRYPTPASSWORDCONN)
  {
    auto const buf{PQencryptPasswordConn(m_conn, password, user, algorithm)};
    std::unique_ptr<char const, std::function<void(char const *)>> ptr{
      buf, [](char const *x) { PQfreemem(const_cast<char *>(x)); }};
    return std::string(ptr.get());
  }
#else
  {
    // No PQencryptPasswordConn.  Fall back on the old PQencryptPassword...
    // unless the caller selects a different algorithm.
    if (algorithm != nullptr and std::strcmp(algorithm, "md5") != 0)
      throw feature_not_supported{
        "Could not encrypt password: available libpq version does not support "
        "algorithms other than md5."};
#  include "pqxx/internal/ignore-deprecated-pre.hxx"
    return pqxx::encrypt_password(user, password);
#  include "pqxx/internal/ignore-deprecated-post.hxx"
  }
#endif // PQXX_HAVE_PQENCRYPTPASSWORDCONN
}


void pqxx::connection::prepare(char const name[], char const definition[]) &
{
  auto const q{std::make_shared<std::string>(
    pqxx::internal::concat("[PREPARE ", name, "]"))};

  auto const r{
    make_result(PQprepare(m_conn, name, definition, 0, nullptr), q, *q)};
}


void pqxx::connection::prepare(char const definition[]) &
{
  this->prepare("", definition);
}


void pqxx::connection::unprepare(std::string_view name)
{
  exec(internal::concat("DEALLOCATE ", quote_name(name)));
}


pqxx::result pqxx::connection::exec_prepared(
  std::string_view statement, internal::c_params const &args)
{
  auto const q{std::make_shared<std::string>(statement)};
  auto const pq_result{PQexecPrepared(
    m_conn, q->c_str(),
    check_cast<int>(std::size(args.values), "exec_prepared"sv),
    args.values.data(), args.lengths.data(),
    reinterpret_cast<int const *>(args.formats.data()),
    static_cast<int>(format::text))};
  auto const r{make_result(pq_result, q, statement)};
  get_notifs();
  return r;
}


void pqxx::connection::close()
{
  try
  {
    if (m_trans)
      PQXX_UNLIKELY
    process_notice(internal::concat(
      "Closing connection while ",
      internal::describe_object("transaction"sv, m_trans->name()),
      " is still open."));

    if (not std::empty(m_receivers))
    {
      PQXX_UNLIKELY
      process_notice("Closing connection with outstanding receivers.");
      m_receivers.clear();
    }

    std::list<errorhandler *> old_handlers;
    m_errorhandlers.swap(old_handlers);
    auto const rbegin{std::crbegin(old_handlers)},
      rend{std::crend(old_handlers)};
    for (auto i{rbegin}; i != rend; ++i)
      pqxx::internal::gate::errorhandler_connection{**i}.unregister();

    PQfinish(m_conn);
    m_conn = nullptr;
  }
  catch (std::exception const &)
  {
    m_conn = nullptr;
    throw;
  }
}


int pqxx::connection::status() const noexcept
{
  return PQstatus(m_conn);
}


namespace
{
/// Return a name for t, if t is non-null and has a name; or empty string.
std::string_view get_name(pqxx::transaction_base const *t)
{
  return (t == nullptr) ? ""sv : t->name();
}
} // namespace


void pqxx::connection::register_transaction(transaction_base *t)
{
  internal::check_unique_register(
    m_trans, "transaction", get_name(m_trans), t, "transaction", get_name(t));
  m_trans = t;
}


void pqxx::connection::unregister_transaction(transaction_base *t) noexcept
{
  try
  {
    internal::check_unique_unregister(
      m_trans, "transaction", get_name(m_trans), t, "transaction",
      get_name(t));
  }
  catch (std::exception const &e)
  {
    process_notice(e.what());
  }
  m_trans = nullptr;
}


std::pair<std::unique_ptr<char, std::function<void(char *)>>, std::size_t>
pqxx::connection::read_copy_line()
{
  char *buf{nullptr};

  // Allocate once, re-use across invocations.
  static auto const q{std::make_shared<std::string>("[END COPY]")};

  auto const line_len{PQgetCopyData(m_conn, &buf, false)};
  switch (line_len)
  {
  case -2: // Error.
    throw failure{
      internal::concat("Reading of table data failed: ", err_msg())};

  case -1: // End of COPY.
    make_result(PQgetResult(m_conn), q, *q);
    return {};

  case 0: // "Come back later."
    throw internal_error{"table read inexplicably went asynchronous"};

  default: // Success, got buffer size.
    // Line size includes a trailing zero, which we ignore.
    auto const text_len{static_cast<std::size_t>(line_len) - 1};
    return std::make_pair(
      std::unique_ptr<char, std::function<void(char *)>>{buf, PQfreemem},
      text_len);
  }
}


void pqxx::connection::write_copy_line(std::string_view line)
{
  static std::string const err_prefix{"Error writing to table: "};
  auto const size{check_cast<int>(
    internal::ssize(line), "Line in stream_to is too long to process."sv)};
  if (PQputCopyData(m_conn, line.data(), size) <= 0)
    PQXX_UNLIKELY
  throw failure{err_prefix + err_msg()};
  if (PQputCopyData(m_conn, "\n", 1) <= 0)
    PQXX_UNLIKELY
  throw failure{err_prefix + err_msg()};
}


void pqxx::connection::end_copy_write()
{
  int res{PQputCopyEnd(m_conn, nullptr)};
  switch (res)
  {
  case -1:
    throw failure{internal::concat("Write to table failed: ", err_msg())};
  case 0: throw internal_error{"table write is inexplicably asynchronous"};
  case 1:
    // Normal termination.  Retrieve result object.
    break;

  default:
    throw internal_error{
      internal::concat("unexpected result ", res, " from PQputCopyEnd()")};
  }

  static auto const q{std::make_shared<std::string>("[END COPY]")};
  make_result(PQgetResult(m_conn), q, *q);
}


void pqxx::connection::start_exec(char const query[])
{
  if (PQsendQuery(m_conn, query) == 0)
    PQXX_UNLIKELY
  throw failure{err_msg()};
}


pqxx::internal::pq::PGresult *pqxx::connection::get_result()
{
  return PQgetResult(m_conn);
}


size_t pqxx::connection::esc_to_buf(std::string_view text, char *buf) const
{
  int err{0};
  auto const copied{
    PQescapeStringConn(m_conn, buf, text.data(), std::size(text), &err)};
  if (err)
    PQXX_UNLIKELY
  throw argument_error{err_msg()};
  return copied;
}


std::string pqxx::connection::esc(std::string_view text) const
{
  std::string buf;
  buf.resize(2 * std::size(text) + 1);
  auto const copied{esc_to_buf(text, buf.data())};
  buf.resize(copied);
  return buf;
}


std::string PQXX_COLD
pqxx::connection::esc_raw(unsigned char const bin[], std::size_t len) const
{
  return pqxx::internal::esc_bin(binary_cast(bin, len));
}


std::string
pqxx::connection::esc_raw(std::basic_string_view<std::byte> bin) const
{
  return pqxx::internal::esc_bin(bin);
}


std::string PQXX_COLD pqxx::connection::unesc_raw(char const text[]) const
{
  if (text[0] == '\\' and text[1] == 'x')
  {
    // Hex-escaped format.
    std::string buf;
    buf.resize(pqxx::internal::size_unesc_bin(std::strlen(text)));
    pqxx::internal::unesc_bin(
      std::string_view{text}, reinterpret_cast<std::byte *>(buf.data()));
    return buf;
  }
  else
  {
    // Legacy escape format.
    // TODO: Remove legacy support.
    std::size_t len;
    auto bytes{const_cast<unsigned char *>(
      reinterpret_cast<unsigned char const *>(text))};
    std::unique_ptr<unsigned char, std::function<void(unsigned char *)>> const
      ptr{PQunescapeBytea(bytes, &len), PQfreemem};
    return std::string{ptr.get(), ptr.get() + len};
  }
}


std::string PQXX_COLD
pqxx::connection::quote_raw(unsigned char const bin[], std::size_t len) const
{
  return internal::concat("'", esc_raw(binary_cast(bin, len)), "'::bytea");
}


std::string
pqxx::connection::quote_raw(std::basic_string_view<std::byte> bytes) const
{
  return internal::concat("'", esc_raw(bytes), "'::bytea");
}


std::string PQXX_COLD pqxx::connection::quote(binarystring const &b) const
{
  return quote(b.bytes_view());
}


std::string pqxx::connection::quote(std::basic_string_view<std::byte> b) const
{
  return internal::concat("'", esc_raw(b), "'::bytea");
}


std::string pqxx::connection::quote_name(std::string_view identifier) const
{
  std::unique_ptr<char, std::function<void(char *)>> buf{
    PQescapeIdentifier(m_conn, identifier.data(), std::size(identifier)),
    PQfreemem};
  if (buf.get() == nullptr)
    PQXX_UNLIKELY
  throw failure{err_msg()};
  return std::string{buf.get()};
}


std::string pqxx::connection::quote_table(std::string_view table_name) const
{
  return this->quote_name(table_name);
}


std::string pqxx::connection::quote_table(table_path path) const
{
  return separated_list(
    ".", std::begin(path), std::end(path),
    [this](auto name) { return this->quote_name(*name); });
}


std::string
pqxx::connection::esc_like(std::string_view text, char escape_char) const
{
  std::string out;
  out.reserve(std::size(text));
  internal::for_glyphs(
    internal::enc_group(encoding_id()),
    [&out, escape_char](char const *gbegin, char const *gend) {
      if ((gend - gbegin == 1) and (*gbegin == '_' or *gbegin == '%'))
        // We're not expecting a lot of wildcards in a string.  Usually.
        PQXX_UNLIKELY
      out.push_back(escape_char);

      for (; gbegin != gend; ++gbegin) out.push_back(*gbegin);
    },
    text.data(), std::size(text));
  return out;
}


int pqxx::connection::await_notification()
{
  int notifs = get_notifs();
  if (notifs == 0)
  {
    PQXX_LIKELY
    internal::wait_fd(socket_of(m_conn), true, false, 10, 0);
    notifs = get_notifs();
  }
  return notifs;
}


int pqxx::connection::await_notification(
  std::time_t seconds, long microseconds)
{
  int notifs = get_notifs();
  if (notifs == 0)
  {
    PQXX_LIKELY
    internal::wait_fd(
      socket_of(m_conn), true, false,
      check_cast<unsigned>(seconds, "Seconds out of range."),
      check_cast<unsigned>(microseconds, "Microseconds out of range."));
    return get_notifs();
  }
  return notifs;
}


std::string pqxx::connection::adorn_name(std::string_view n)
{
  auto const id{to_string(++m_unique_id)};
  if (std::empty(n))
    return pqxx::internal::concat("x", id);
  else
    return pqxx::internal::concat(n, "_", id);
}


std::string pqxx::connection::get_client_encoding() const
{
  return internal::name_encoding(encoding_id());
}


void PQXX_COLD pqxx::connection::set_client_encoding(char const encoding[]) &
{
  switch (auto const retval{PQsetClientEncoding(m_conn, encoding)}; retval)
  {
  case 0:
    // OK.
    PQXX_LIKELY
    break;
  case -1:
    PQXX_UNLIKELY
    if (is_open())
      throw failure{"Setting client encoding failed."};
    else
      throw broken_connection{"Lost connection to the database server."};
  default:
    PQXX_UNLIKELY
    throw internal_error{internal::concat(
      "Unexpected result from PQsetClientEncoding: ", retval)};
  }
}


int pqxx::connection::encoding_id() const
{
  int const enc{PQclientEncoding(m_conn)};
  if (enc == -1)
  {
    // PQclientEncoding does not query the database, but it does check for
    // broken connections.  And unfortunately, we check the encoding right
    // *before* checking a query result for failure.  So, we need to handle
    // connection failure here and it will apply in lots of places.
    // TODO: Make pqxx::result::result(...) do all the checking.
    PQXX_UNLIKELY
    if (is_open())
      throw failure{"Could not obtain client encoding."};
    else
      throw broken_connection{"Lost connection to the database server."};
  }
  PQXX_LIKELY
  return enc;
}


pqxx::result pqxx::connection::exec_params(
  std::string_view query, internal::c_params const &args)
{
  auto const q{std::make_shared<std::string>(query)};
  auto const pq_result{PQexecParams(
    m_conn, q->c_str(),
    check_cast<int>(std::size(args.values), "exec_params"sv), nullptr,
    args.values.data(), args.lengths.data(),
    reinterpret_cast<int const *>(args.formats.data()),
    static_cast<int>(format::text))};
  auto const r{make_result(pq_result, q)};
  get_notifs();
  return r;
}


namespace
{
/// Get the prevailing default value for a connection parameter.
char const *get_default(PQconninfoOption const &opt) noexcept
{
  if (opt.envvar == nullptr)
  {
    // There's no environment variable for this setting.  The only default is
    // the one that was compiled in.
    return opt.compiled;
  }
  // As of C++11, std::getenv() uses thread-local storage, so it should be
  // thread-safe.  MSVC still warns about it though.
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4996)
#endif
  char const *var{std::getenv(opt.envvar)};
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
  if (var == nullptr)
  {
    // There's an environment variable for this setting, but it's not set.
    return opt.compiled;
  }

  // The environment variable is the prevailing default.
  return var;
}
} // namespace


std::string pqxx::connection::connection_string() const
{
  if (m_conn == nullptr)
    PQXX_UNLIKELY
  throw usage_error{"Can't get connection string: connection is not open."};

  std::unique_ptr<
    PQconninfoOption, std::function<void(PQconninfoOption *)>> const params{
    PQconninfo(m_conn), PQconninfoFree};
  if (params.get() == nullptr)
    PQXX_UNLIKELY
  throw std::bad_alloc{};

  std::string buf;
  for (std::size_t i{0}; params.get()[i].keyword != nullptr; ++i)
  {
    auto const param{params.get()[i]};
    if (param.val != nullptr)
    {
      auto const default_val{get_default(param)};
      if (
        (default_val == nullptr) or (std::strcmp(param.val, default_val) != 0))
      {
        if (not std::empty(buf))
          buf.push_back(' ');
        buf += param.keyword;
        buf.push_back('=');
        buf += param.val;
      }
    }
  }
  return buf;
}


#if defined(_WIN32) || __has_include(<fcntl.h>)
pqxx::connecting::connecting(zview connection_string) :
        m_conn{connection::connect_nonblocking, connection_string}
{}
#endif // defined(_WIN32) || __has_include(<fcntl.h>


#if defined(_WIN32) || __has_include(<fcntl.h>)
void pqxx::connecting::process() &
{
  auto const [reading, writing]{m_conn.poll_connect()};
  m_reading = reading;
  m_writing = writing;
}
#endif // defined(_WIN32) || __has_include(<fcntl.h>


#if defined(_WIN32) || __has_include(<fcntl.h>)
pqxx::connection pqxx::connecting::produce() &&
{
  if (!done())
    throw usage_error{
      "Tried to produce a nonblocking connection before it was done."};
  m_conn.complete_init();
  return std::move(m_conn);
}
#endif // defined(_WIN32) || __has_include(<fcntl.h>
