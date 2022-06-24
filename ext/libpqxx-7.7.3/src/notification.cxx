/** Implementation of the pqxx::notification_receiever class.
 *
 * pqxx::notification_receiver processes notifications.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include <string>

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/internal/gates/connection-notification_receiver.hxx"
#include "pqxx/notification.hxx"

#include "pqxx/internal/header-post.hxx"


pqxx::notification_receiver::notification_receiver(
  connection &c, std::string_view channel) :
        m_conn{c}, m_channel{channel}
{
  pqxx::internal::gate::connection_notification_receiver{c}.add_receiver(this);
}


pqxx::notification_receiver::~notification_receiver()
{
  pqxx::internal::gate::connection_notification_receiver{this->conn()}
    .remove_receiver(this);
}
