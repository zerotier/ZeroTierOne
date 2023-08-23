/** Transaction focus: types which monopolise a transaction's attention.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_TRANSACTION_FOCUS
#define PQXX_H_TRANSACTION_FOCUS

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include "pqxx/util.hxx"

namespace pqxx
{
/// Base class for things that monopolise a transaction's attention.
/** You probably won't need to use this class.  But it can be useful to _know_
 * that a given libpqxx class is derived from it.
 *
 * Pipelines, SQL statements, and data streams are examples of classes derived
 * from `transaction_focus`.  For any given transaction, only one object of
 * such a class can be active at any given time.
 */
class PQXX_LIBEXPORT transaction_focus
{
public:
  transaction_focus(
    transaction_base &t, std::string_view cname, std::string_view oname) :
          m_trans{t}, m_classname{cname}, m_name{oname}
  {}

  transaction_focus(
    transaction_base &t, std::string_view cname, std::string &&oname) :
          m_trans{t}, m_classname{cname}, m_name{std::move(oname)}
  {}

  transaction_focus(transaction_base &t, std::string_view cname) :
          m_trans{t}, m_classname{cname}
  {}

  transaction_focus() = delete;
  transaction_focus(transaction_focus const &) = delete;
  transaction_focus &operator=(transaction_focus const &) = delete;

  /// Class name, for human consumption.
  [[nodiscard]] constexpr std::string_view classname() const noexcept
  {
    return m_classname;
  }

  /// Name for this object, if the caller passed one; empty string otherwise.
  [[nodiscard]] std::string_view name() const &noexcept { return m_name; }

  [[nodiscard]] std::string description() const
  {
    return pqxx::internal::describe_object(m_classname, m_name);
  }

  /// Can't move a transaction_focus.
  /** Moving the transaction_focus would break the transaction's reference back
   * to the object.
   */
  transaction_focus(transaction_focus &&) = delete;

  /// Can't move a transaction_focus.
  /** Moving the transaction_focus would break the transaction's reference back
   * to the object.
   */
  transaction_focus &operator=(transaction_focus &&) = delete;

protected:
  void register_me();
  void unregister_me() noexcept;
  void reg_pending_error(std::string const &) noexcept;
  bool registered() const noexcept { return m_registered; }

  transaction_base &m_trans;

private:
  bool m_registered = false;
  std::string_view m_classname;
  std::string m_name;
};
} // namespace pqxx
#endif
