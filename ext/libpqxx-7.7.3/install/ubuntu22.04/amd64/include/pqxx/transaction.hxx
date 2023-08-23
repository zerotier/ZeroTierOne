/* Definition of the pqxx::transaction class.
 * pqxx::transaction represents a standard database transaction.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/transaction instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_TRANSACTION
#define PQXX_H_TRANSACTION

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include "pqxx/dbtransaction.hxx"

namespace pqxx::internal
{
/// Helper base class for the @ref transaction class template.
class PQXX_LIBEXPORT basic_transaction : public dbtransaction
{
protected:
  basic_transaction(
    connection &c, zview begin_command, std::string_view tname);
  basic_transaction(connection &c, zview begin_command, std::string &&tname);
  basic_transaction(connection &c, zview begin_command);

  virtual ~basic_transaction() noexcept override = 0;

private:
  virtual void do_commit() override;
};
} // namespace pqxx::internal


namespace pqxx
{
/**
 * @ingroup transactions
 */
//@{

/// Standard back-end transaction, templatised on isolation level.
/** This is the type you'll normally want to use to represent a transaction on
 * the database.
 *
 * Usage example: double all wages.
 *
 * ```cxx
 * extern connection C;
 * work T(C);
 * try
 * {
 *   T.exec0("UPDATE employees SET wage=wage*2");
 *   T.commit();	// NOTE: do this inside try block
 * }
 * catch (exception const &e)
 * {
 *   cerr << e.what() << endl;
 *   T.abort();		// Usually not needed; same happens when T's life ends.
 * }
 * ```
 */
template<
  isolation_level ISOLATION = isolation_level::read_committed,
  write_policy READWRITE = write_policy::read_write>
class transaction final : public internal::basic_transaction
{
public:
  /// Begin a transaction.
  /**
   * @param c Connection for this transaction to operate on.
   * @param tname Optional name for transaction.  Must begin with a letter and
   * may contain letters and digits only.
   */
  transaction(connection &c, std::string_view tname) :
          internal::basic_transaction{
            c, internal::begin_cmd<ISOLATION, READWRITE>, tname}
  {}

  /// Begin a transaction.
  /**
   * @param c Connection for this transaction to operate on.
   * may contain letters and digits only.
   */
  explicit transaction(connection &c) :
          internal::basic_transaction{
            c, internal::begin_cmd<ISOLATION, READWRITE>}
  {}

  virtual ~transaction() noexcept override { close(); }
};


/// The default transaction type.
using work = transaction<>;

/// Read-only transaction.
using read_transaction =
  transaction<isolation_level::read_committed, write_policy::read_only>;

//@}
} // namespace pqxx
#endif
