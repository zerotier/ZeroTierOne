/** Implementation of the pqxx::subtransaction class.
 *
 * pqxx::transaction is a nested transaction, i.e. one within a transaction
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include <memory>
#include <stdexcept>

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/connection.hxx"
#include "pqxx/internal/concat.hxx"
#include "pqxx/subtransaction.hxx"

#include "pqxx/internal/header-post.hxx"


namespace
{
using namespace std::literals;
constexpr std::string_view class_name{"subtransaction"sv};
} // namespace


pqxx::subtransaction::subtransaction(
  dbtransaction &t, std::string_view tname) :
        transaction_focus{t, class_name, t.conn().adorn_name(tname)},
        // We can't initialise the rollback command here, because we don't yet
        // have a full object to implement quoted_name().
        dbtransaction{t.conn(), tname, std::shared_ptr<std::string>{}}
{
  set_rollback_cmd(std::make_shared<std::string>(
    internal::concat("ROLLBACK TO SAVEPOINT ", quoted_name())));
  direct_exec(std::make_shared<std::string>(
    internal::concat("SAVEPOINT ", quoted_name())));
}


namespace
{
using dbtransaction_ref = pqxx::dbtransaction &;
}


pqxx::subtransaction::subtransaction(
  subtransaction &t, std::string_view tname) :
        subtransaction(dbtransaction_ref(t), tname)
{}


pqxx::subtransaction::~subtransaction() noexcept
{
  close();
}


void pqxx::subtransaction::do_commit()
{
  direct_exec(std::make_shared<std::string>(
    internal::concat("RELEASE SAVEPOINT ", quoted_name())));
}
