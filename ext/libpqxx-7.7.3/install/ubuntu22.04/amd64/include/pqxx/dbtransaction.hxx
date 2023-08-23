/* Definition of the pqxx::dbtransaction abstract base class.
 *
 * pqxx::dbransaction defines a real transaction on the database.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/dbtransaction instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_DBTRANSACTION
#define PQXX_H_DBTRANSACTION

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include "pqxx/transaction_base.hxx"

namespace pqxx
{
/// Abstract transaction base class: bracket transactions on the database.
/**
 * @ingroup transactions
 *
 * Use a dbtransaction-derived object such as "work" (transaction<>) to enclose
 * operations on a database in a single "unit of work."  This ensures that the
 * whole series of operations either succeeds as a whole or fails completely.
 * In no case will it leave half-finished work behind in the database.
 *
 * Once processing on a transaction has succeeded and any changes should be
 * allowed to become permanent in the database, call commit().  If something
 * has gone wrong and the changes should be forgotten, call abort() instead.
 * If you do neither, an implicit abort() is executed at destruction time.
 *
 * It is an error to abort a transaction that has already been committed, or to
 * commit a transaction that has already been aborted.  Aborting an already
 * aborted transaction or committing an already committed one is allowed, to
 * make error handling easier.  Repeated aborts or commits have no effect after
 * the first one.
 *
 * Database transactions are not suitable for guarding long-running processes.
 * If your transaction code becomes too long or too complex, consider ways to
 * break it up into smaller ones.  Unfortunately there is no universal recipe
 * for this.
 *
 * The actual operations for committing/aborting the backend transaction are
 * implemented by a derived class.  The implementing concrete class must also
 * call @ref close from its destructor.
 */
class PQXX_LIBEXPORT PQXX_NOVTABLE dbtransaction : public transaction_base
{
protected:
  /// Begin transaction.
  explicit dbtransaction(connection &c) : transaction_base{c} {}
  /// Begin transaction.
  dbtransaction(connection &c, std::string_view tname) :
          transaction_base{c, tname}
  {}
  /// Begin transaction.
  dbtransaction(
    connection &c, std::string_view tname,
    std::shared_ptr<std::string> rollback_cmd) :
          transaction_base{c, tname, rollback_cmd}
  {}
};
} // namespace pqxx
#endif
