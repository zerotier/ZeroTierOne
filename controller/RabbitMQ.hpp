/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */
#ifndef ZT_CONTROLLER_RABBITMQ_HPP
#define ZT_CONTROLLER_RABBITMQ_HPP

namespace ZeroTier
{
struct MQConfig {
    const char *host;
    int port;
    const char *username;
    const char *password;
};
}

#ifdef ZT_CONTROLLER_USE_LIBPQ

#include "../node/Mutex.hpp"

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <string>

namespace ZeroTier
{

class RabbitMQ {
public:
    RabbitMQ(MQConfig *cfg, const char *queueName);
    ~RabbitMQ();

    void init();

    std::string consume();

private:
    MQConfig *_mqc;
    const char *_qName;

    amqp_socket_t *_socket;
    amqp_connection_state_t _conn;
    amqp_queue_declare_ok_t *_q;
    int _status;

    int _channel;

	Mutex _chan_m;

};

}

#endif // ZT_CONTROLLER_USE_LIBPQ

#endif // ZT_CONTROLLER_RABBITMQ_HPP

