/** Implementation of pqxx::errorhandler and helpers.
 *
 * pqxx::errorhandler allows programs to receive errors and warnings.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/connection.hxx"
#include "pqxx/errorhandler.hxx"
#include "pqxx/internal/gates/connection-errorhandler.hxx"

#include "pqxx/internal/header-post.hxx"


pqxx::errorhandler::errorhandler(connection &conn) : m_home{&conn}
{
  pqxx::internal::gate::connection_errorhandler{*m_home}.register_errorhandler(
    this);
}


pqxx::errorhandler::~errorhandler()
{
  unregister();
}


void pqxx::errorhandler::unregister() noexcept
{
  if (m_home != nullptr)
  {
    pqxx::internal::gate::connection_errorhandler connection_gate{*m_home};
    m_home = nullptr;
    connection_gate.unregister_errorhandler(this);
  }
}
