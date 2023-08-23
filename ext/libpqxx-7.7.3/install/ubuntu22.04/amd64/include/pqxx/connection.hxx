/* Definition of the connection class.
 *
 * pqxx::connection encapsulates a connection to a database.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/connection instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_CONNECTION
#define PQXX_H_CONNECTION

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <cstddef>
#include <ctime>
#include <functional>
#include <initializer_list>
#include <list>
#include <map>
#include <memory>
#include <string_view>
#include <tuple>
#include <utility>

// Double-check in order to suppress an overzealous Visual C++ warning (#418).
#if defined(PQXX_HAVE_CONCEPTS) && __has_include(<ranges>)
#  include <ranges>
#endif

#include "pqxx/errorhandler.hxx"
#include "pqxx/except.hxx"
#include "pqxx/internal/concat.hxx"
#include "pqxx/params.hxx"
#include "pqxx/separated_list.hxx"
#include "pqxx/strconv.hxx"
#include "pqxx/types.hxx"
#include "pqxx/util.hxx"
#include "pqxx/zview.hxx"


/**
 * @addtogroup connections
 *
 * Use of the libpqxx library starts here.
 *
 * Everything that can be done with a database through libpqxx must go through
 * a @ref pqxx::connection object.  It connects to a database when you create
 * it, and it terminates that communication during destruction.
 *
 * Many things come together in this class.  Handling of error and warning
 * messages, for example, is defined by @ref pqxx::errorhandler objects in the
 * context of a connection.  Prepared statements are also defined here.
 *
 * When you connect to a database, you pass a connection string containing any
 * parameters and options, such as the server address and the database name.
 *
 * These are identical to the ones in libpq, the C language binding upon which
 * libpqxx itself is built:
 *
 * https://www.postgresql.org/docs/current/libpq-connect.html#LIBPQ-CONNSTRING
 *
 * There are also environment variables you can set to provide defaults, again
 * as defined by libpq:
 *
 * https://www.postgresql.org/docs/current/libpq-envars.html
 *
 * You can also create a database connection _asynchronously_ using an
 * intermediate @ref pqxx::connecting object.
 */

namespace pqxx::internal
{
class sql_cursor;

#if defined(PQXX_HAVE_CONCEPTS)
/// Concept: T is a range of pairs of zero-terminated strings.
template<typename T>
concept ZKey_ZValues = std::ranges::input_range<T> and requires(T t)
{
  {std::cbegin(t)};
  {
    std::get<0>(*std::cbegin(t))
    } -> ZString;
  {
    std::get<1>(*std::cbegin(t))
    } -> ZString;
} and std::tuple_size_v<typename std::ranges::iterator_t<T>::value_type>
== 2;
#endif // PQXX_HAVE_CONCEPTS
} // namespace pqxx::internal


namespace pqxx::internal::gate
{
class connection_dbtransaction;
class connection_errorhandler;
class connection_largeobject;
class connection_notification_receiver;
class connection_pipeline;
class connection_sql_cursor;
class connection_stream_from;
class connection_stream_to;
class connection_transaction;
class const_connection_largeobject;
} // namespace pqxx::internal::gate


namespace pqxx
{
/// Representation of a PostgreSQL table path.
/** A "table path" consists of a table name, optionally prefixed by a schema
 * name, which in turn is optionally prefixed by a database name.
 *
 * A minimal example of a table path would be `{mytable}`.  But a table path
 * may also take the forms `{myschema,mytable}` or
 * `{mydb,myschema,mytable}`.
 */
using table_path = std::initializer_list<std::string_view>;


/// Encrypt a password.  @deprecated Use connection::encrypt_password instead.
[[nodiscard,
  deprecated("Use connection::encrypt_password instead.")]] std::string
  PQXX_LIBEXPORT
  encrypt_password(char const user[], char const password[]);

/// Encrypt password.  @deprecated Use connection::encrypt_password instead.
[[nodiscard,
  deprecated("Use connection::encrypt_password instead.")]] inline std::string
encrypt_password(zview user, zview password)
{
#include "pqxx/internal/ignore-deprecated-pre.hxx"
  return encrypt_password(user.c_str(), password.c_str());
#include "pqxx/internal/ignore-deprecated-post.hxx"
}


/// Error verbosity levels.
enum class error_verbosity : int
{
  // These values must match those in libpq's PGVerbosity enum.
  terse = 0,
  normal = 1,
  verbose = 2
};


/// Connection to a database.
/** This is the first class to look at when you wish to work with a database
 * through libpqxx.  The connection opens during construction, and closes upon
 * destruction.
 *
 * When creating a connection, you can pass a connection URI or a postgres
 * connection string, to specify the database server's address, a login
 * username, and so on.  If you don't, the connection will try to obtain them
 * from certain environment variables.  If those are not set either, the
 * default is to try and connect to the local system's port 5432.
 *
 * Find more about connection strings here:
 *
 * https://www.postgresql.org/docs/current/libpq-connect.html#LIBPQ-CONNSTRING
 *
 * The variables are documented here:
 *
 * https://www.postgresql.org/docs/current/libpq-envars.html
 *
 * To query or manipulate the database once connected, use one of the
 * transaction classes (see pqxx/transaction_base.hxx) and perhaps also the
 * transactor framework (see pqxx/transactor.hxx).
 *
 * When a connection breaks, you will typically get a @ref broken_connection
 * exception.  This can happen at almost any point.
 *
 * @warning On Unix-like systems, including GNU and BSD systems, your program
 * may receive the SIGPIPE signal when the connection to the backend breaks. By
 * default this signal will abort your program.  Use "signal(SIGPIPE, SIG_IGN)"
 * if you want your program to continue running after a connection fails.
 */
class PQXX_LIBEXPORT connection
{
public:
  connection() : connection{""} {}

  /// Connect to a database, using `options` string.
  explicit connection(char const options[])
  {
    check_version();
    init(options);
  }

  /// Connect to a database, using `options` string.
  explicit connection(zview options) : connection{options.c_str()}
  {
    // (Delegates to other constructor which calls check_version for us.)
  }

  /// Move constructor.
  /** Moving a connection is not allowed if it has an open transaction, or has
   * error handlers or notification receivers registered on it.  In those
   * situations, other objects may hold references to the old object which
   * would become invalid and might produce hard-to-diagnose bugs.
   */
  connection(connection &&rhs);

#if defined(PQXX_HAVE_CONCEPTS)
  /// Connect to a database, passing options as a range of key/value pairs.
  /** @warning Experimental.  Requires C++20 "concepts" support.  Define
   * `PQXX_HAVE_CONCEPTS` to enable it.
   *
   * There's no need to escape the parameter values.
   *
   * See the PostgreSQL libpq documentation for the full list of possible
   * options:
   *
   * https://postgresql.org/docs/current/libpq-connect.html#LIBPQ-PARAMKEYWORDS
   *
   * The options can be anything that can be iterated as a series of pairs of
   * zero-terminated strings: `std::pair<std::string, std::string>`, or
   * `std::tuple<pqxx::zview, char const *>`, or
   * `std::map<std::string, pqxx::zview>`, and so on.
   */
  template<internal::ZKey_ZValues MAPPING>
  inline connection(MAPPING const &params);
#endif // PQXX_HAVE_CONCEPTS

  ~connection()
  {
    try
    {
      close();
    }
    catch (std::exception const &)
    {}
  }

  /// Move assignment.
  /** Neither connection can have an open transaction, registered error
   * handlers, or registered notification receivers.
   */
  connection &operator=(connection &&rhs);

  connection(connection const &) = delete;
  connection &operator=(connection const &) = delete;

  /// Is this connection open at the moment?
  /** @warning This function is **not** needed in most code.  Resist the
   * temptation to check it after opening a connection.  The `connection`
   * constructor will throw a @ref broken_connection exception if can't connect
   * to the database.
   */
  [[nodiscard]] bool PQXX_PURE is_open() const noexcept;

  /// Invoke notice processor function.  The message should end in newline.
  void process_notice(char const[]) noexcept;
  /// Invoke notice processor function.  Newline at end is recommended.
  /** The zview variant, with a message ending in newline, is the most
   * efficient way to call process_notice.
   */
  void process_notice(zview) noexcept;

  /// Enable tracing to a given output stream, or nullptr to disable.
  void trace(std::FILE *) noexcept;

  /**
   * @name Connection properties
   *
   * These are probably not of great interest, since most are derived from
   * information supplied by the client program itself, but they are included
   * for completeness.
   *
   * The connection needs to be currently active for these to work.
   */
  //@{
  /// Name of database we're connected to, if any.
  [[nodiscard]] char const *dbname() const;

  /// Database user ID we're connected under, if any.
  [[nodiscard]] char const *username() const;

  /// Address of server, or nullptr if none specified (i.e. default or local)
  [[nodiscard]] char const *hostname() const;

  /// Server port number we're connected to.
  [[nodiscard]] char const *port() const;

  /// Process ID for backend process, or 0 if inactive.
  [[nodiscard]] int PQXX_PURE backendpid() const &noexcept;

  /// Socket currently used for connection, or -1 for none.  Use with care!
  /** Query the current socket number.  This is intended for event loops based
   * on functions such as select() or poll(), where you're waiting for any of
   * multiple file descriptors to become ready for communication.
   *
   * Please try to stay away from this function.  It is really only meant for
   * event loops that need to wait on more than one file descriptor.  If all
   * you need is to block until a notification arrives, for instance, use
   * await_notification().  If you want to issue queries and retrieve results
   * in nonblocking fashion, check out the pipeline class.
   */
  [[nodiscard]] int PQXX_PURE sock() const &noexcept;

  /// What version of the PostgreSQL protocol is this connection using?
  /** The answer can be 0 (when there is no connection); 3 for protocol 3.0; or
   * possibly higher values as newer protocol versions come into use.
   */
  [[nodiscard]] int PQXX_PURE protocol_version() const noexcept;

  /// What version of the PostgreSQL server are we connected to?
  /** The result is a bit complicated: each of the major, medium, and minor
   * release numbers is written as a two-digit decimal number, and the three
   * are then concatenated.  Thus server version 9.4.2 will be returned as the
   * decimal number 90402.  If there is no connection to the server, this
   * returns zero.
   *
   * @warning When writing version numbers in your code, don't add zero at the
   * beginning!  Numbers beginning with zero are interpreted as octal (base-8)
   * in C++.  Thus, 070402 is not the same as 70402, and 080000 is not a number
   * at all because there is no digit "8" in octal notation.  Use strictly
   * decimal notation when it comes to these version numbers.
   */
  [[nodiscard]] int PQXX_PURE server_version() const noexcept;
  //@}

  /// @name Text encoding
  /**
   * Each connection is governed by a "client encoding," which dictates how
   * strings and other text is represented in bytes.  The database server will
   * send text data to you in this encoding, and you should use it for the
   * queries and data which you send to the server.
   *
   * Search the PostgreSQL documentation for "character set encodings" to find
   * out more about the available encodings, how to extend them, and how to use
   * them.  Not all server-side encodings are compatible with all client-side
   * encodings or vice versa.
   *
   * Encoding names are case-insensitive, so e.g. "UTF8" is equivalent to
   * "utf8".
   *
   * You can change the client encoding, but this may not work when the
   * connection is in a special state, such as when streaming a table.  It's
   * not clear what happens if you change the encoding during a transaction,
   * and then abort the transaction.
   */
  //@{
  /// Get client-side character encoding, by name.
  [[nodiscard]] std::string get_client_encoding() const;

  /// Set client-side character encoding, by name.
  /**
   * @param encoding Name of the character set encoding to use.
   */
  void set_client_encoding(zview encoding) &
  {
    set_client_encoding(encoding.c_str());
  }

  /// Set client-side character encoding, by name.
  /**
   * @param encoding Name of the character set encoding to use.
   */
  void set_client_encoding(char const encoding[]) &;

  /// Get the connection's encoding, as a PostgreSQL-defined code.
  [[nodiscard]] int PQXX_PRIVATE encoding_id() const;

  //@}

  /// Set session variable, using SQL's `SET` command.
  /** @deprecated To set a session variable, use @ref set_session_var.  To set
   * a transaction-local variable, execute an SQL `SET` command.
   *
   * @warning When setting a string value, you must escape and quote it first.
   * Use the @ref quote() function to do that.
   *
   * @warning This executes an SQL query, so do not get or set variables while
   * a table stream or pipeline is active on the same connection.
   *
   * @param var Variable to set.
   * @param value New value for Var.  This can be any SQL expression.  If it's
   * a string, be sure that it's properly escaped and quoted.
   */
  [[deprecated("To set session variables, use set_session_var.")]] void
  set_variable(std::string_view var, std::string_view value) &;

  /// Set one of the session variables to a new value.
  /** This executes SQL, so do not do it while a pipeline or stream is active
   * on the connection.
   *
   * The value you set here will last for the rest of the connection's
   * duration, or until you set a new value.
   *
   * If you set the value while in a @ref dbtransaction (i.e. any transaction
   * that is not a @ref nontransaction), then rolling back the transaction will
   * undo the change.
   *
   * All applies to setting _session_ variables.  You can also set the same
   * variables as _local_ variables, in which case they will always revert to
   * their previous value when the transaction ends (or when you overwrite them
   * of course).  To set a local variable, simply execute an SQL statement
   * along the lines of "`SET LOCAL var = 'value'`" inside your transaction.
   *
   * @param var The variable to set.
   * @param value The new value for the variable.
   * @throw @ref variable_set_to_null if the value is null; this is not
   * allowed.
   */
  template<typename TYPE>
  void set_session_var(std::string_view var, TYPE const &value) &
  {
    if constexpr (nullness<TYPE>::has_null)
    {
      if (nullness<TYPE>::is_null(value))
        throw variable_set_to_null{
          internal::concat("Attempted to set variable ", var, " to null.")};
    }
    exec(internal::concat("SET ", quote_name(var), "=", quote(value)));
  }

  /// Read session variable, using SQL's `SHOW` command.
  /** @warning This executes an SQL query, so do not get or set variables while
   * a table stream or pipeline is active on the same connection.
   */
  [[deprecated("Use get_var instead.")]] std::string
    get_variable(std::string_view);

  /// Read currently applicable value of a variable.
  /** This function executes an SQL statement, so it won't work while a
   * @ref pipeline or query stream is active on the connection.
   *
   * @return a blank `std::optional` if the variable's value is null, or its
   * string value otherwise.
   */
  std::string get_var(std::string_view var);

  /// Read currently applicable value of a variable.
  /** This function executes an SQL statement, so it won't work while a
   * @ref pipeline or query stream is active on the connection.
   *
   * If there is any possibility that the variable is null, ensure that `TYPE`
   * can represent null values.
   */
  template<typename TYPE> TYPE get_var_as(std::string_view var)
  {
    return from_string<TYPE>(get_var(var));
  }

  /**
   * @name Notifications and Receivers
   */
  //@{
  /// Check for pending notifications and take appropriate action.
  /** This does not block.  To wait for incoming notifications, either call
   * await_notification() (it calls this function); or wait for incoming data
   * on the connection's socket (i.e. wait to read), and then call this
   * function repeatedly until it returns zero.  After that, there are no more
   * pending notifications so you may want to wait again.
   *
   * If any notifications are pending when you call this function, it
   * processes them by finding any receivers that match the notification string
   * and invoking those.  If no receivers match, there is nothing to invoke but
   * we do consider the notification processed.
   *
   * If any of the client-registered receivers throws an exception, the
   * function will report it using the connection's errorhandlers.  It does not
   * re-throw the exceptions.
   *
   * @return Number of notifications processed.
   */
  int get_notifs();

  /// Wait for a notification to come in.
  /** There are other events that will also terminate the wait, such as the
   * backend failing.  It will also wake up periodically.
   *
   * If a notification comes in, the call will process it, along with any other
   * notifications that may have been pending.
   *
   * To wait for notifications into your own event loop instead, wait until
   * there is incoming data on the connection's socket to be read, then call
   * @ref get_notifs() repeatedly until it returns zero.
   *
   * @return Number of notifications processed.
   */
  int await_notification();

  /// Wait for a notification to come in, or for given timeout to pass.
  /** There are other events that will also terminate the wait, such as the
   * backend failing, or timeout expiring.
   *
   * If a notification comes in, the call will process it, along with any other
   * notifications that may have been pending.
   *
   * To wait for notifications into your own event loop instead, wait until
   * there is incoming data on the connection's socket to be read, then call
   * @ref get_notifs repeatedly until it returns zero.
   *
   * @return Number of notifications processed
   */
  int await_notification(std::time_t seconds, long microseconds);
  //@}

  /**
   * @name Password encryption
   *
   * Use this when setting a new password for the user if password encryption
   * is enabled.  Inputs are the SQL name for the user for whom you with to
   * encrypt a password; the plaintext password; and the hash algorithm.
   *
   * The algorithm must be one of "md5", "scram-sha-256" (introduced in
   * PostgreSQL 10), or `nullptr`.  If the pointer is null, this will query
   * the `password_encryption setting` from the server, and use the default
   * algorithm as defined there.
   *
   * @return encrypted version of the password, suitable for encrypted
   * PostgreSQL authentication.
   *
   * Thus you can change a user's password with:
   * ```cxx
   * void setpw(transaction_base &t, string const &user, string const &pw)
   * {
   *   t.exec0("ALTER USER " + user + " "
   *   	"PASSWORD '" + t.conn().encrypt_password(user,pw) + "'");
   * }
   * ```
   *
   * When building this against a libpq older than version 10, this will use
   * an older function which only supports md5.  In that case, requesting a
   * different algorithm than md5 will result in a @ref feature_not_supported
   * exception.
   */
  //@{
  /// Encrypt a password for a given user.
  [[nodiscard]] std::string
  encrypt_password(zview user, zview password, zview algorithm)
  {
    return encrypt_password(user.c_str(), password.c_str(), algorithm.c_str());
  }
  /// Encrypt a password for a given user.
  [[nodiscard]] std::string encrypt_password(
    char const user[], char const password[], char const *algorithm = nullptr);
  //@}

  /**
   * @name Prepared statements
   *
   * PostgreSQL supports prepared SQL statements, i.e. statements that you can
   * register under a name you choose, optimized once by the backend, and
   * executed any number of times under the given name.
   *
   * Prepared statement definitions are not sensitive to transaction
   * boundaries. A statement defined inside a transaction will remain defined
   * outside that transaction, even if the transaction itself is subsequently
   * aborted.  Once a statement has been prepared, it will only go away if you
   * close the connection or explicitly "unprepare" the statement.
   *
   * Use the `pqxx::transaction_base::exec_prepared` functions to execute a
   * prepared statement.  See @ref prepared for a full discussion.
   *
   * @warning Using prepared statements can save time, but if your statement
   * takes parameters, it may also make your application significantly slower!
   * The reason is that the server works out a plan for executing the query
   * when you prepare it.  At that time, of course it does not know the values
   * for the parameters that you will pass.  If you execute a query without
   * preparing it, then the server works out the plan on the spot, with full
   * knowledge of the parameter values.
   *
   * A statement's definition can refer to its parameters as `$1`, `$2`, etc.
   * The first parameter you pass to the call provides a value for `$1`, and
   * so on.
   *
   * Here's an example of how to use prepared statements.
   *
   * ```cxx
   * using namespace pqxx;
   * void foo(connection &c)
   * {
   *   c.prepare("findtable", "select * from pg_tables where name=$1");
   *   work tx{c};
   *   result r = tx.exec_prepared("findtable", "mytable");
   *   if (std::empty(r)) throw runtime_error{"mytable not found!"};
   * }
   * ```
   */
  //@{

  /// Define a prepared statement.
  /**
   * @param name unique name for the new prepared statement.
   * @param definition SQL statement to prepare.
   */
  void prepare(zview name, zview definition) &
  {
    prepare(name.c_str(), definition.c_str());
  }

  /**
   * @param name unique name for the new prepared statement.
   * @param definition SQL statement to prepare.
   */
  void prepare(char const name[], char const definition[]) &;

  /// Define a nameless prepared statement.
  /**
   * This can be useful if you merely want to pass large binary parameters to a
   * statement without otherwise wishing to prepare it.  If you use this
   * feature, always keep the definition and the use close together to avoid
   * the nameless statement being redefined unexpectedly by code somewhere
   * else.
   */
  void prepare(char const definition[]) &;
  void prepare(zview definition) & { return prepare(definition.c_str()); }

  /// Drop prepared statement.
  void unprepare(std::string_view name);

  //@}

  // C++20: constexpr.  Breaks ABI.
  /// Suffix unique number to name to make it unique within session context.
  /** Used internally to generate identifiers for SQL objects (such as cursors
   * and nested transactions) based on a given human-readable base name.
   */
  [[nodiscard]] std::string adorn_name(std::string_view);

  /**
   * @defgroup escaping-functions String-escaping functions
   */
  //@{

  /// Escape string for use as SQL string literal on this connection.
  /** @warning This accepts a length, and it does not require a terminating
   * zero byte.  But if there is a zero byte, escaping stops there even if
   * it's not at the end of the string!
   */
  [[deprecated("Use std::string_view or pqxx:zview.")]] std::string
  esc(char const text[], std::size_t maxlen) const
  {
    return esc(std::string_view{text, maxlen});
  }

  /// Escape string for use as SQL string literal on this connection.
  [[nodiscard]] std::string esc(char const text[]) const
  {
    return esc(std::string_view{text});
  }

#if defined(PQXX_HAVE_SPAN)
  /// Escape string for use as SQL string literal, into `buffer`.
  /** Use this variant when you want to re-use the same buffer across multiple
   * calls.  If that's not the case, or convenience and simplicity are more
   * important, use the single-argument variant.
   *
   * For every byte in `text`, there must be at least 2 bytes of space in
   * `buffer`; plus there must be one byte of space for a trailing zero.
   * Throws @ref range_error if this space is not available.
   *
   * Returns a reference to the escaped string, which is actually stored in
   * `buffer`.
   */
  [[nodiscard]] std::string_view
  esc(std::string_view text, std::span<char> buffer)
  {
    auto const size{std::size(text)}, space{std::size(buffer)};
    auto const needed{2 * size + 1};
    if (space < needed)
      throw range_error{internal::concat(
        "Not enough room to escape string of ", size, " byte(s): need ",
        needed, " bytes of buffer space, but buffer size is ", space, ".")};
    auto const data{buffer.data()};
    return {data, esc_to_buf(text, data)};
  }
#endif

  /// Escape string for use as SQL string literal on this connection.
  /** @warning This is meant for text strings only.  It cannot contain bytes
   * whose value is zero ("nul bytes").
   */
  [[nodiscard]] std::string esc(std::string_view text) const;

#if defined(PQXX_HAVE_CONCEPTS)
  /// Escape binary string for use as SQL string literal on this connection.
  /** This is identical to `esc_raw(data)`. */
  template<binary DATA> [[nodiscard]] std::string esc(DATA const &data) const
  {
    return esc_raw(data);
  }
#endif

#if defined(PQXX_HAVE_CONCEPTS) && defined(PQXX_HAVE_SPAN)
  /// Escape binary string for use as SQL string literal, into `buffer`.
  /** Use this variant when you want to re-use the same buffer across multiple
   * calls.  If that's not the case, or convenience and simplicity are more
   * important, use the single-argument variant.
   *
   * For every byte in `data`, there must be at least two bytes of space in
   * `buffer`; plus there must be two bytes of space for a header and one for
   * a trailing zero.  Throws @ref range_error if this space is not available.
   *
   * Returns a reference to the escaped string, which is actually stored in
   * `buffer`.
   */
  template<binary DATA>
  [[nodiscard]] zview esc(DATA const &data, std::span<char> buffer) const
  {
    auto const size{std::size(data)}, space{std::size(buffer)};
    auto const needed{internal::size_esc_bin(std::size(data))};
    if (space < needed)
      throw range_error{internal::concat(
        "Not enough room to escape binary string of ", size, " byte(s): need ",
        needed, " bytes of buffer space, but buffer size is ", space, ".")};

    std::basic_string_view<std::byte> view{std::data(data), std::size(data)};
    auto const out{std::data(buffer)};
    // Actually, in the modern format, we know beforehand exactly how many
    // bytes we're going to fill.  Just leave out the trailing zero.
    internal::esc_bin(view, out);
    return zview{out, needed - 1};
  }
#endif

  /// Escape binary string for use as SQL string literal on this connection.
  [[deprecated("Use std::byte for binary data.")]] std::string
  esc_raw(unsigned char const bin[], std::size_t len) const;

  /// Escape binary string for use as SQL string literal on this connection.
  /** You can also just use @ref esc with a binary string. */
  [[nodiscard]] std::string esc_raw(std::basic_string_view<std::byte>) const;

#if defined(PQXX_HAVE_SPAN)
  /// Escape binary string for use as SQL string literal, into `buffer`.
  /** You can also just use @ref esc with a binary string. */
  [[nodiscard]] std::string
  esc_raw(std::basic_string_view<std::byte>, std::span<char> buffer) const;
#endif

#if defined(PQXX_HAVE_CONCEPTS)
  /// Escape binary string for use as SQL string literal on this connection.
  /** You can also just use @ref esc with a binary string. */
  template<binary DATA>
  [[nodiscard]] std::string esc_raw(DATA const &data) const
  {
    return esc_raw(
      std::basic_string_view<std::byte>{std::data(data), std::size(data)});
  }
#endif

#if defined(PQXX_HAVE_CONCEPTS) && defined(PQXX_HAVE_SPAN)
  /// Escape binary string for use as SQL string literal, into `buffer`.
  template<binary DATA>
  [[nodiscard]] zview esc_raw(DATA const &data, std::span<char> buffer) const
  {
    return this->esc(binary_cast(data), buffer);
  }
#endif

  /// Unescape binary data, e.g. from a table field or notification payload.
  /** Takes a binary string as escaped by PostgreSQL, and returns a restored
   * copy of the original binary data.
   */
  [[nodiscard, deprecated("Use unesc_bin() instead.")]] std::string
  unesc_raw(zview text) const
  {
#include "pqxx/internal/ignore-deprecated-pre.hxx"
    return unesc_raw(text.c_str());
#include "pqxx/internal/ignore-deprecated-post.hxx"
  }

  /// Unescape binary data, e.g. from a table field or notification payload.
  /** Takes a binary string as escaped by PostgreSQL, and returns a restored
   * copy of the original binary data.
   */
  [[nodiscard, deprecated("Use unesc_bin() instead.")]] std::string
  unesc_raw(char const text[]) const;

  // TODO: Make "into buffer" variant to eliminate a string allocation.
  /// Unescape binary data, e.g. from a table field or notification payload.
  /** Takes a binary string as escaped by PostgreSQL, and returns a restored
   * copy of the original binary data.
   *
   * (The data must be encoded in PostgreSQL's "hex" format.  The legacy
   * "bytea" escape format, used prior to PostgreSQL 9.0, is no longer
   * supported.)
   */
  [[nodiscard]] std::basic_string<std::byte>
  unesc_bin(std::string_view text) const
  {
    std::basic_string<std::byte> buf;
    buf.resize(pqxx::internal::size_unesc_bin(std::size(text)));
    pqxx::internal::unesc_bin(text, buf.data());
    return buf;
  }

  /// Escape and quote a string of binary data.
  [[deprecated("Use quote(std::basic_string_view<std::byte>).")]] std::string
  quote_raw(unsigned char const bin[], std::size_t len) const;

  /// Escape and quote a string of binary data.
  std::string quote_raw(std::basic_string_view<std::byte>) const;

#if defined(PQXX_HAVE_CONCEPTS)
  /// Escape and quote a string of binary data.
  /** You can also just use @ref quote with binary data. */
  template<binary DATA>
  [[nodiscard]] std::string quote_raw(DATA const &data) const
  {
    return quote_raw(
      std::basic_string_view<std::byte>{std::data(data), std::size(data)});
  }
#endif

  // TODO: Make "into buffer" variant to eliminate a string allocation.
  /// Escape and quote an SQL identifier for use in a query.
  [[nodiscard]] std::string quote_name(std::string_view identifier) const;

  // TODO: Make "into buffer" variant to eliminate a string allocation.
  /// Escape and quote a table name.
  /** When passing just a table name, this is just another name for
   * @ref quote_name.
   */
  [[nodiscard]] std::string quote_table(std::string_view name) const;

  // TODO: Make "into buffer" variant to eliminate a string allocation.
  /// Escape and quote a table path.
  /** A table path consists of a table name, optionally prefixed by a schema
   * name; and if both are given, they are in turn optionally prefixed by a
   * database name.
   *
   * Each portion of the path (database name, schema name, table name) will be
   * quoted separately, and they will be joined together by dots.  So for
   * example, `myschema.mytable` will become `"myschema"."mytable"`.
   */
  [[nodiscard]] std::string quote_table(table_path) const;

  // TODO: Make "into buffer" variant to eliminate a string allocation.
  /// Quote and comma-separate a series of column names.
  /** Use this to save a bit of work in cases where you repeatedly need to pass
   * the same list of column names, e.g. with @ref stream_to and @ref
   * stream_from. Some functions that need to quote the columns list
   * internally, will have a "raw" alternative which let you do the quoting
   * yourself.  It's a bit of extra work, but it can in rare cases let you
   * eliminate some duplicate work in quoting them repeatedly.
   */
  template<PQXX_CHAR_STRINGS_ARG STRINGS>
  inline std::string quote_columns(STRINGS const &columns) const;

  // TODO: Make "into buffer" variant to eliminate a string allocation.
  /// Represent object as SQL string, including quoting & escaping.
  /**
   * Recognises nulls and represents them as SQL nulls.  They get no quotes.
   */
  template<typename T>
  [[nodiscard]] inline std::string quote(T const &t) const;

  [[deprecated("Use std::byte for binary data.")]] std::string
  quote(binarystring const &) const;

  // TODO: Make "into buffer" variant to eliminate a string allocation.
  /// Escape and quote binary data for use as a BYTEA value in SQL statement.
  [[nodiscard]] std::string
  quote(std::basic_string_view<std::byte> bytes) const;

  // TODO: Make "into buffer" variant to eliminate a string allocation.
  /// Escape string for literal LIKE match.
  /** Use this when part of an SQL "LIKE" pattern should match only as a
   * literal string, not as a pattern, even if it contains "%" or "_"
   * characters that would normally act as wildcards.
   *
   * The string does not get string-escaped or quoted.  You do that later.
   *
   * For instance, let's say you have a string `name` entered by the user,
   * and you're searching a `file` column for items that match `name`
   * followed by a dot and three letters.  Even if `name` contains wildcard
   * characters "%" or "_", you only want those to match literally, so "_"
   * only matches "_" and "%" only matches a single "%".
   *
   * You do that by "like-escaping" `name`, appending the wildcard pattern
   * `".___"`, and finally, escaping and quoting the result for inclusion in
   * your query:
   *
   * ```cxx
   *    tx.exec(
   *        "SELECT file FROM item WHERE file LIKE " +
   *        tx.quote(tx.esc_like(name) + ".___"));
   * ```
   *
   * The SQL "LIKE" operator also lets you choose your own escape character.
   * This is supported, but must be a single-byte character.
   */
  [[nodiscard]] std::string
  esc_like(std::string_view text, char escape_char = '\\') const;
  //@}

  /// Attempt to cancel the ongoing query, if any.
  /** You can use this from another thread, and/or while a query is executing
   * in a pipeline, but it's up to you to ensure that you're not canceling the
   * wrong query.  This may involve locking.
   */
  void cancel_query();

#if defined(_WIN32) || __has_include(<fcntl.h>)
  /// Set socket to blocking (true) or nonblocking (false).
  /** @warning Do not use this unless you _really_ know what you're doing.
   * @warning This function is available on most systems, but not necessarily
   * all.
   */
  void set_blocking(bool block) &;
#endif // defined(_WIN32) || __has_include(<fcntl.h>)

  /// Set session verbosity.
  /** Set the verbosity of error messages to "terse", "normal" (the default),
   * or "verbose."
   *
   *  If "terse", returned messages include severity, primary text, and
   * position only; this will normally fit on a single line. "normal" produces
   * messages that include the above plus any detail, hint, or context fields
   * (these might span multiple lines).  "verbose" includes all available
   * fields.
   */
  void set_verbosity(error_verbosity verbosity) &noexcept;

  /// Return pointers to the active errorhandlers.
  /** The entries are ordered from oldest to newest handler.
   *
   * You may use this to find errorhandlers that your application wants to
   * delete when destroying the connection.  Be aware, however, that libpqxx
   * may also add errorhandlers of its own, and those will be included in the
   * list.  If this is a problem for you, derive your errorhandlers from a
   * custom base class derived from pqxx::errorhandler.  Then use dynamic_cast
   * to find which of the error handlers are yours.
   *
   * The pointers point to the real errorhandlers.  The container it returns
   * however is a copy of the one internal to the connection, not a reference.
   */
  [[nodiscard]] std::vector<errorhandler *> get_errorhandlers() const;

  /// Return a connection string encapsulating this connection's options.
  /** The connection must be currently open for this to work.
   *
   * Returns a reconstruction of this connection's connection string.  It may
   * not exactly match the connection string you passed in when creating this
   * connection.
   */
  [[nodiscard]] std::string connection_string() const;

  /// Explicitly close the connection.
  /** The destructor will do this for you automatically.  Still, there is a
   * reason to `close()` objects explicitly where possible: if an error should
   * occur while closing, `close()` can throw an exception.  A destructor
   * cannot.
   *
   * Closing a connection is idempotent.  Closing a connection that's already
   * closed does nothing.
   */
  void close();

  /// Seize control of a raw libpq connection.
  /** @warning Do not do this.  Please.  It's for very rare, very specific
   * use-cases.  The mechanism may change (or break) in unexpected ways in
   * future versions.
   *
   * @param raw_conn a raw libpq `PQconn` pointer.
   */
  static connection seize_raw_connection(internal::pq::PGconn *raw_conn)
  {
    return connection{raw_conn};
  }

  /// Release the raw connection without closing it.
  /** @warning Do not do this.  It's for very rare, very specific use-cases.
   * The mechanism may change (or break) in unexpected ways in future versions.
   *
   * The `connection` object becomes unusable after this.
   */
  internal::pq::PGconn *release_raw_connection() &&
  {
    return std::exchange(m_conn, nullptr);
  }

private:
  friend class connecting;
  enum connect_mode
  {
    connect_nonblocking
  };
  connection(connect_mode, zview connection_string);

  /// For use by @ref seize_raw_connection.
  explicit connection(internal::pq::PGconn *raw_conn) : m_conn{raw_conn} {}

  /// Poll for ongoing connection, try to progress towards completion.
  /** Returns a pair of "now please wait to read data from socket" and "now
   * please wait to write data to socket."  Both will be false when done.
   *
   * Throws an exception if polling indicates that the connection has failed.
   */
  std::pair<bool, bool> poll_connect();

  // Initialise based on connection string.
  void init(char const options[]);
  // Initialise based on parameter names and values.
  void init(char const *params[], char const *values[]);
  void complete_init();

  result make_result(
    internal::pq::PGresult *pgr, std::shared_ptr<std::string> const &query,
    std::string_view desc = ""sv);

  void PQXX_PRIVATE set_up_state();

  int PQXX_PRIVATE PQXX_PURE status() const noexcept;

  /// Escape a string, into a buffer allocated by the caller.
  /** The buffer must have room for at least `2*std::size(text) + 1` bytes.
   *
   * Returns the number of bytes written, including the trailing zero.
   */
  std::size_t esc_to_buf(std::string_view text, char *buf) const;

  friend class internal::gate::const_connection_largeobject;
  char const *PQXX_PURE err_msg() const noexcept;

  void PQXX_PRIVATE process_notice_raw(char const msg[]) noexcept;

  result exec_prepared(std::string_view statement, internal::c_params const &);

  /// Throw @ref usage_error if this connection is not in a movable state.
  void check_movable() const;
  /// Throw @ref usage_error if not in a state where it can be move-assigned.
  void check_overwritable() const;

  friend class internal::gate::connection_errorhandler;
  void PQXX_PRIVATE register_errorhandler(errorhandler *);
  void PQXX_PRIVATE unregister_errorhandler(errorhandler *) noexcept;

  friend class internal::gate::connection_transaction;
  result exec(std::string_view, std::string_view = ""sv);
  result
    PQXX_PRIVATE exec(std::shared_ptr<std::string>, std::string_view = ""sv);
  void PQXX_PRIVATE register_transaction(transaction_base *);
  void PQXX_PRIVATE unregister_transaction(transaction_base *) noexcept;

  friend class internal::gate::connection_stream_from;
  std::pair<std::unique_ptr<char, std::function<void(char *)>>, std::size_t>
    PQXX_PRIVATE read_copy_line();

  friend class internal::gate::connection_stream_to;
  void PQXX_PRIVATE write_copy_line(std::string_view);
  void PQXX_PRIVATE end_copy_write();

  friend class internal::gate::connection_largeobject;
  internal::pq::PGconn *raw_connection() const { return m_conn; }

  friend class internal::gate::connection_notification_receiver;
  void add_receiver(notification_receiver *);
  void remove_receiver(notification_receiver *) noexcept;

  friend class internal::gate::connection_pipeline;
  void PQXX_PRIVATE start_exec(char const query[]);
  bool PQXX_PRIVATE consume_input() noexcept;
  bool PQXX_PRIVATE is_busy() const noexcept;
  internal::pq::PGresult *get_result();

  friend class internal::gate::connection_dbtransaction;
  friend class internal::gate::connection_sql_cursor;

  result exec_params(std::string_view query, internal::c_params const &args);

  /// Connection handle.
  internal::pq::PGconn *m_conn = nullptr;

  /// Active transaction on connection, if any.
  /** We don't use this for anything, except to check for open transactions
   * when we close the connection or start a new transaction.
   *
   * We also don't allow move construction or move assignment while there's a
   * transaction, since moving the connection in that case would leave one or
   * more pointers back from the transaction to the connection dangling.
   */
  transaction_base const *m_trans = nullptr;

  std::list<errorhandler *> m_errorhandlers;

  using receiver_list =
    std::multimap<std::string, pqxx::notification_receiver *>;
  /// Notification receivers.
  receiver_list m_receivers;

  /// Unique number to use as suffix for identifiers (see adorn_name()).
  int m_unique_id = 0;
};


/// @deprecated Old base class for connection.  They are now the same class.
using connection_base = connection;


/// An ongoing, non-blocking stepping stone to a connection.
/** Use this when you want to create a connection to the database, but without
 * blocking your whole thread.   It is only available on systems that have
 * the `<fcntl.h>` header, and Windows.
 *
 * Connecting in this way is probably not "faster" (it's more complicated and
 * has some extra overhead), but in some situations you can use it to make your
 * application as a whole faster.  It all depends on having other useful work
 * to do in the same thread, and being able to wait on a socket.  If you have
 * other I/O going on at the same time, your event loop can wait for both the
 * libpqxx socket and your own sockets, and wake up whenever any of them is
 * ready to do work.
 *
 * Connecting in this way is not properly "asynchronous;" it's merely
 * "nonblocking."  This means it's not a super-high-performance mechanism like
 * you might get with e.g. `io_uring`.  In particular, if we need to look up
 * the database hostname in DNS, that will happen synchronously.
 *
 * To use this, create the `connecting` object, passing a connection string.
 * Then loop: If @ref wait_to_read returns true, wait for the socket to have
 * incoming data on it.  If @ref wait_to_write returns true, wait for the
 * socket to be ready for writing.  Then call @ref process to process any
 * incoming or outgoing data.  Do all of this until @ref done returns true (or
 * there is an exception).  Finally, call @ref produce to get the completed
 * connection.
 *
 * For example:
 *
 * ```cxx
 *     pqxx::connecting cg{};
 *
 *     // Loop until we're done connecting.
 *     while (!cg.done())
 *     {
 *         wait_for_fd(cg.sock(), cg.wait_to_read(), cg.wait_to_write());
 *         cg.process();
 *     }
 *
 *     pqxx::connection conn = std::move(cg).produce();
 *
 *     // At this point, conn is a working connection.  You can no longer use
 *     // cg at all.
 * ```
 */
class PQXX_LIBEXPORT connecting
{
public:
  /// Start connecting.
  connecting(zview connection_string = ""_zv);

  connecting(connecting const &) = delete;
  connecting(connecting &&) = default;
  connecting &operator=(connecting const &) = delete;
  connecting &operator=(connecting &&) = default;

  /// Get the socket.  The socket may change during the connection process.
  [[nodiscard]] int sock() const &noexcept { return m_conn.sock(); }

  /// Should we currently wait to be able to _read_ from the socket?
  [[nodiscard]] constexpr bool wait_to_read() const &noexcept
  {
    return m_reading;
  }

  /// Should we currently wait to be able to _write_ to the socket?
  [[nodiscard]] constexpr bool wait_to_write() const &noexcept
  {
    return m_writing;
  }

  /// Progress towards completion (but don't block).
  void process() &;

  /// Is our connection finished?
  [[nodiscard]] constexpr bool done() const &noexcept
  {
    return not m_reading and not m_writing;
  }

  /// Produce the completed connection object.
  /** Use this only once, after @ref done returned `true`.  Once you have
   * called this, the `connecting` instance has no more use or meaning.  You
   * can't call any of its member functions afterwards.
   *
   * This member function is rvalue-qualified, meaning that you can only call
   * it on an rvalue instance of the class.  If what you have is not an rvalue,
   * turn it into one by wrapping it in `std::move()`.
   */
  [[nodiscard]] connection produce() &&;

private:
  connection m_conn;
  bool m_reading{false};
  bool m_writing{true};
};


template<typename T> inline std::string connection::quote(T const &t) const
{
  if constexpr (nullness<T>::always_null)
  {
    return "NULL";
  }
  else
  {
    if (is_null(t))
      return "NULL";
    auto const text{to_string(t)};

    // Okay, there's an easy way to do this and there's a hard way.  The easy
    // way was "quote, esc(to_string(t)), quote".  I'm going with the hard way
    // because it's going to save some string manipulation that will probably
    // incur some unnecessary memory allocations and deallocations.
    std::string buf{'\''};
    buf.resize(2 + 2 * std::size(text) + 1);
    auto const content_bytes{esc_to_buf(text, buf.data() + 1)};
    auto const closing_quote{1 + content_bytes};
    buf[closing_quote] = '\'';
    auto const end{closing_quote + 1};
    buf.resize(end);
    return buf;
  }
}


template<PQXX_CHAR_STRINGS_ARG STRINGS>
inline std::string connection::quote_columns(STRINGS const &columns) const
{
  return separated_list(
    ","sv, std::cbegin(columns), std::cend(columns),
    [this](auto col) { return this->quote_name(*col); });
}


#if defined(PQXX_HAVE_CONCEPTS)
template<internal::ZKey_ZValues MAPPING>
inline connection::connection(MAPPING const &params)
{
  check_version();

  std::vector<char const *> keys, values;
  if constexpr (std::ranges::sized_range<MAPPING>)
  {
    auto const size{std::ranges::size(params) + 1};
    keys.reserve(size);
    values.reserve(size);
  }
  for (auto const &[key, value] : params)
  {
    keys.push_back(internal::as_c_string(key));
    values.push_back(internal::as_c_string(value));
  }
  keys.push_back(nullptr);
  values.push_back(nullptr);
  init(std::data(keys), std::data(values));
}
#endif // PQXX_HAVE_CONCEPTS
} // namespace pqxx
#endif
