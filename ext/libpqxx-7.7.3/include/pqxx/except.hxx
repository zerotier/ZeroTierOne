/* Definition of libpqxx exception classes.
 *
 * pqxx::sql_error, pqxx::broken_connection, pqxx::in_doubt_error, ...
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/except instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_EXCEPT
#define PQXX_H_EXCEPT

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <stdexcept>
#include <string>


namespace pqxx
{
/**
 * @addtogroup exception Exception classes
 *
 * These exception classes follow, roughly, the two-level hierarchy defined by
 * the PostgreSQL SQLSTATE error codes (see Appendix A of the PostgreSQL
 * documentation corresponding to your server version).  This is not a complete
 * mapping though.  There are other differences as well, e.g. the error code
 * for `statement_completion_unknown` has a separate status in libpqxx as
 * @ref in_doubt_error, and `too_many_connections` is classified as a
 * `broken_connection` rather than a subtype of `insufficient_resources`.
 *
 * @see http://www.postgresql.org/docs/9.4/interactive/errcodes-appendix.html
 *
 * @{
 */

/// Run-time failure encountered by libpqxx, similar to std::runtime_error.
struct PQXX_LIBEXPORT failure : std::runtime_error
{
  explicit failure(std::string const &);
};


/// Exception class for lost or failed backend connection.
/**
 * @warning When this happens on Unix-like systems, you may also get a SIGPIPE
 * signal.  That signal aborts the program by default, so if you wish to be
 * able to continue after a connection breaks, be sure to disarm this signal.
 *
 * If you're working on a Unix-like system, see the manual page for
 * `signal` (2) on how to deal with SIGPIPE.  The easiest way to make this
 * signal harmless is to make your program ignore it:
 *
 * ```cxx
 * #include <signal.h>
 *
 * int main()
 * {
 *   signal(SIGPIPE, SIG_IGN);
 *   // ...
 * ```
 */
struct PQXX_LIBEXPORT broken_connection : failure
{
  broken_connection();
  explicit broken_connection(std::string const &);
};


/// The caller attempted to set a variable to null, which is not allowed.
struct PQXX_LIBEXPORT variable_set_to_null : failure
{
  variable_set_to_null();
  explicit variable_set_to_null(std::string const &);
};


/// Exception class for failed queries.
/** Carries, in addition to a regular error message, a copy of the failed query
 * and (if available) the SQLSTATE value accompanying the error.
 */
class PQXX_LIBEXPORT sql_error : public failure
{
  /// Query string.  Empty if unknown.
  std::string const m_query;
  /// SQLSTATE string describing the error type, if known; or empty string.
  std::string const m_sqlstate;

public:
  explicit sql_error(
    std::string const &whatarg = "", std::string const &Q = "",
    char const sqlstate[] = nullptr);
  virtual ~sql_error() noexcept override;

  /// The query whose execution triggered the exception
  [[nodiscard]] PQXX_PURE std::string const &query() const noexcept;

  /// SQLSTATE error code if known, or empty string otherwise.
  [[nodiscard]] PQXX_PURE std::string const &sqlstate() const noexcept;
};


/// "Help, I don't know whether transaction was committed successfully!"
/** Exception that might be thrown in rare cases where the connection to the
 * database is lost while finishing a database transaction, and there's no way
 * of telling whether it was actually executed by the backend.  In this case
 * the database is left in an indeterminate (but consistent) state, and only
 * manual inspection will tell which is the case.
 */
struct PQXX_LIBEXPORT in_doubt_error : failure
{
  explicit in_doubt_error(std::string const &);
};


/// The backend saw itself forced to roll back the ongoing transaction.
struct PQXX_LIBEXPORT transaction_rollback : sql_error
{
  explicit transaction_rollback(
    std::string const &whatarg, std::string const &q = "",
    char const sqlstate[] = nullptr);
};


/// Transaction failed to serialize.  Please retry it.
/** Can only happen at transaction isolation levels REPEATABLE READ and
 * SERIALIZABLE.
 *
 * The current transaction cannot be committed without violating the guarantees
 * made by its isolation level.  This is the effect of a conflict with another
 * ongoing transaction.  The transaction may still succeed if you try to
 * perform it again.
 */
struct PQXX_LIBEXPORT serialization_failure : transaction_rollback
{
  explicit serialization_failure(
    std::string const &whatarg, std::string const &q,
    char const sqlstate[] = nullptr);
};


/// We can't tell whether our last statement succeeded.
struct PQXX_LIBEXPORT statement_completion_unknown : transaction_rollback
{
  explicit statement_completion_unknown(
    std::string const &whatarg, std::string const &q,
    char const sqlstate[] = nullptr);
};


/// The ongoing transaction has deadlocked.  Retrying it may help.
struct PQXX_LIBEXPORT deadlock_detected : transaction_rollback
{
  explicit deadlock_detected(
    std::string const &whatarg, std::string const &q,
    char const sqlstate[] = nullptr);
};


/// Internal error in libpqxx library
struct PQXX_LIBEXPORT internal_error : std::logic_error
{
  explicit internal_error(std::string const &);
};


/// Error in usage of libpqxx library, similar to std::logic_error
struct PQXX_LIBEXPORT usage_error : std::logic_error
{
  explicit usage_error(std::string const &);
};


/// Invalid argument passed to libpqxx, similar to std::invalid_argument
struct PQXX_LIBEXPORT argument_error : std::invalid_argument
{
  explicit argument_error(std::string const &);
};


/// Value conversion failed, e.g. when converting "Hello" to int.
struct PQXX_LIBEXPORT conversion_error : std::domain_error
{
  explicit conversion_error(std::string const &);
};


/// Could not convert value to string: not enough buffer space.
struct PQXX_LIBEXPORT conversion_overrun : conversion_error
{
  explicit conversion_overrun(std::string const &);
};


/// Something is out of range, similar to std::out_of_range
struct PQXX_LIBEXPORT range_error : std::out_of_range
{
  explicit range_error(std::string const &);
};


/// Query returned an unexpected number of rows.
struct PQXX_LIBEXPORT unexpected_rows : public range_error
{
  explicit unexpected_rows(std::string const &msg) : range_error{msg} {}
};


/// Database feature not supported in current setup.
struct PQXX_LIBEXPORT feature_not_supported : sql_error
{
  explicit feature_not_supported(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          sql_error{err, Q, sqlstate}
  {}
};

/// Error in data provided to SQL statement.
struct PQXX_LIBEXPORT data_exception : sql_error
{
  explicit data_exception(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          sql_error{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT integrity_constraint_violation : sql_error
{
  explicit integrity_constraint_violation(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          sql_error{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT restrict_violation : integrity_constraint_violation
{
  explicit restrict_violation(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          integrity_constraint_violation{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT not_null_violation : integrity_constraint_violation
{
  explicit not_null_violation(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          integrity_constraint_violation{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT foreign_key_violation : integrity_constraint_violation
{
  explicit foreign_key_violation(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          integrity_constraint_violation{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT unique_violation : integrity_constraint_violation
{
  explicit unique_violation(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          integrity_constraint_violation{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT check_violation : integrity_constraint_violation
{
  explicit check_violation(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          integrity_constraint_violation{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT invalid_cursor_state : sql_error
{
  explicit invalid_cursor_state(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          sql_error{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT invalid_sql_statement_name : sql_error
{
  explicit invalid_sql_statement_name(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          sql_error{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT invalid_cursor_name : sql_error
{
  explicit invalid_cursor_name(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          sql_error{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT syntax_error : sql_error
{
  /// Approximate position in string where error occurred, or -1 if unknown.
  int const error_position;

  explicit syntax_error(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr, int pos = -1) :
          sql_error{err, Q, sqlstate}, error_position{pos}
  {}
};

struct PQXX_LIBEXPORT undefined_column : syntax_error
{
  explicit undefined_column(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          syntax_error{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT undefined_function : syntax_error
{
  explicit undefined_function(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          syntax_error{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT undefined_table : syntax_error
{
  explicit undefined_table(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          syntax_error{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT insufficient_privilege : sql_error
{
  explicit insufficient_privilege(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          sql_error{err, Q, sqlstate}
  {}
};

/// Resource shortage on the server
struct PQXX_LIBEXPORT insufficient_resources : sql_error
{
  explicit insufficient_resources(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          sql_error{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT disk_full : insufficient_resources
{
  explicit disk_full(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          insufficient_resources{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT out_of_memory : insufficient_resources
{
  explicit out_of_memory(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          insufficient_resources{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT too_many_connections : broken_connection
{
  explicit too_many_connections(std::string const &err) :
          broken_connection{err}
  {}
};

/// PL/pgSQL error
/** Exceptions derived from this class are errors from PL/pgSQL procedures.
 */
struct PQXX_LIBEXPORT plpgsql_error : sql_error
{
  explicit plpgsql_error(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          sql_error{err, Q, sqlstate}
  {}
};

/// Exception raised in PL/pgSQL procedure
struct PQXX_LIBEXPORT plpgsql_raise : plpgsql_error
{
  explicit plpgsql_raise(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          plpgsql_error{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT plpgsql_no_data_found : plpgsql_error
{
  explicit plpgsql_no_data_found(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          plpgsql_error{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT plpgsql_too_many_rows : plpgsql_error
{
  explicit plpgsql_too_many_rows(
    std::string const &err, std::string const &Q = "",
    char const sqlstate[] = nullptr) :
          plpgsql_error{err, Q, sqlstate}
  {}
};

struct PQXX_LIBEXPORT blob_already_exists : failure
{
  explicit blob_already_exists(std::string const &);
};

/**
 * @}
 */
} // namespace pqxx
#endif
