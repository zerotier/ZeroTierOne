/* Definition of the pqxx::subtransaction class.
 *
 * pqxx::subtransaction is a nested transaction, i.e. one within a transaction.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/subtransaction instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_SUBTRANSACTION
#define PQXX_H_SUBTRANSACTION

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include "pqxx/dbtransaction.hxx"

namespace pqxx
{
/**
 * @ingroup transactions
 */
/// "Transaction" nested within another transaction
/** A subtransaction can be executed inside a backend transaction, or inside
 * another subtransaction.  This can be useful when, for example, statements in
 * a transaction may harmlessly fail and you don't want them to abort the
 * entire transaction.  Here's an example of how a temporary table may be
 * dropped before re-creating it, without failing if the table did not exist:
 *
 * ```cxx
 * void do_job(connection &C)
 * {
 *   string const temptable = "fleetingtable";
 *
 *   work W(C, "do_job");
 *   do_firstpart(W);
 *
 *   // Attempt to delete our temporary table if it already existed.
 *   try
 *   {
 *     subtransaction S(W, "droptemp");
 *     S.exec0("DROP TABLE " + temptable);
 *     S.commit();
 *   }
 *   catch (undefined_table const &)
 *   {
 *     // Table did not exist.  Which is what we were hoping to achieve anyway.
 *     // Carry on without regrets.
 *   }
 *
 *   // S may have gone into a failed state and been destroyed, but the
 *   // upper-level transaction W is still fine.  We can continue to use it.
 *   W.exec0("CREATE TEMP TABLE " + temptable + "(bar integer, splat
 * varchar)");
 *
 *   do_lastpart(W);
 * }
 * ```
 *
 * (This is just an example.  If you really wanted to do drop a table without
 * an error if it doesn't exist, you'd use DROP TABLE IF EXISTS.)
 *
 * There are no isolation levels inside a transaction.  They are not needed
 * because all actions within the same backend transaction are always performed
 * sequentially anyway.
 *
 * @warning While the subtransaction is "live," you cannot execute queries or
 * open streams etc. on its parent transaction.  A transaction can have at most
 * one object of a type derived from @ref pqxx::transaction_focus active on it
 * at a time.
 */
class PQXX_LIBEXPORT subtransaction : public transaction_focus,
                                      public dbtransaction
{
public:
  /// Nest a subtransaction nested in another transaction.
  explicit subtransaction(dbtransaction &t, std::string_view tname = ""sv);

  /// Nest a subtransaction in another subtransaction.
  explicit subtransaction(subtransaction &t, std::string_view name = ""sv);

  virtual ~subtransaction() noexcept override;

private:
  std::string quoted_name() const
  {
    return quote_name(transaction_focus::name());
  }
  virtual void do_commit() override;
};
} // namespace pqxx
#endif
