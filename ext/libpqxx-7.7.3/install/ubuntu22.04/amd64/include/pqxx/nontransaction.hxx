/* Definition of the pqxx::nontransaction class.
 *
 * pqxx::nontransaction provides nontransactional database access
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/nontransaction instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_NONTRANSACTION
#define PQXX_H_NONTRANSACTION

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include "pqxx/connection.hxx"
#include "pqxx/result.hxx"
#include "pqxx/transaction.hxx"

namespace pqxx
{
using namespace std::literals;

/// Simple "transaction" class offering no transactional integrity.
/**
 * @ingroup transactions
 *
 * nontransaction, like transaction or any other transaction_base-derived
 * class, provides access to a database through a connection.  Unlike its
 * siblings, however, nontransaction does not maintain any kind of
 * transactional integrity.  This may be useful eg. for read-only access to the
 * database that does not require a consistent, atomic view on its data; or for
 * operations that are not allowed within a backend transaction, such as
 * creating tables.
 *
 * For queries that update the database, however, a real transaction is likely
 * to be faster unless the transaction consists of only a single record update.
 *
 * Also, you can keep a nontransaction open for as long as you like.  Actual
 * back-end transactions are limited in lifespan, and will sometimes fail just
 * because they took too long to execute or were left idle for too long.  This
 * will not happen with a nontransaction (although the connection may still
 * time out, e.g. when the network is unavailable for a very long time).
 *
 * Any query executed in a nontransaction is committed immediately, and neither
 * commit() nor abort() has any effect.
 *
 * Database features that require a backend transaction, such as cursors or
 * large objects, will not work in a nontransaction.
 */
class PQXX_LIBEXPORT nontransaction final : public transaction_base
{
public:
  /// Constructor.
  /** Create a "dummy" transaction.
   * @param c Connection in which this "transaction" will operate.
   * @param tname Optional tname for the transaction, beginning with a letter
   * and containing only letters and digits.
   */
  nontransaction(connection &c, std::string_view tname = ""sv) :
          transaction_base{c, tname, std::shared_ptr<std::string>{}}
  {
    register_transaction();
  }

  virtual ~nontransaction() override { close(); }

private:
  virtual void do_commit() override {}
};
} // namespace pqxx
#endif
