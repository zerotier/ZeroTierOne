/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_CONTROLLER_RABBITMQ_HPP
#define ZT_CONTROLLER_RABBITMQ_HPP

#include "DB.hpp"
#include <string>

namespace ZeroTier
{
struct MQConfig {
	std::string host;
	int port;
	std::string username;
	std::string password;
};
}

#ifdef ZT_CONTROLLER_USE_LIBPQ

#include "../node/Mutex.hpp"

#include <amqp.h>
#include <amqp_tcp_socket.h>


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

