/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "RabbitMQ.hpp"

#ifdef ZT_CONTROLLER_USE_LIBPQ

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <stdexcept>
#include <cstring>

namespace ZeroTier
{

RabbitMQ::RabbitMQ(MQConfig *cfg, const char *queueName)
	: _mqc(cfg)
	, _qName(queueName)
	, _socket(NULL)
	, _status(0)
{
}

RabbitMQ::~RabbitMQ()
{
	amqp_channel_close(_conn, _channel, AMQP_REPLY_SUCCESS);
	amqp_connection_close(_conn, AMQP_REPLY_SUCCESS);
	amqp_destroy_connection(_conn);
}

void RabbitMQ::init()
{
	struct timeval tval;
	memset(&tval, 0, sizeof(struct timeval));
	tval.tv_sec = 5;

	fprintf(stderr, "Initializing RabbitMQ %s\n", _qName);
	_conn = amqp_new_connection();
	_socket = amqp_tcp_socket_new(_conn);
	if (!_socket) {
		throw std::runtime_error("Can't create socket for RabbitMQ");
	}

	_status = amqp_socket_open_noblock(_socket, _mqc->host, _mqc->port, &tval);
	if (_status) {
		throw std::runtime_error("Can't connect to RabbitMQ");
	}

	amqp_rpc_reply_t r = amqp_login(_conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
		_mqc->username, _mqc->password);
	if (r.reply_type != AMQP_RESPONSE_NORMAL) {
		throw std::runtime_error("RabbitMQ Login Error");
	}

	static int chan = 0;
	{
		Mutex::Lock l(_chan_m);
		_channel = ++chan;
	}
	amqp_channel_open(_conn, _channel);
	r = amqp_get_rpc_reply(_conn);
	if(r.reply_type != AMQP_RESPONSE_NORMAL) {
		throw std::runtime_error("Error opening communication channel");
	}

	_q = amqp_queue_declare(_conn, _channel, amqp_cstring_bytes(_qName), 0, 0, 0, 0, amqp_empty_table);
	r = amqp_get_rpc_reply(_conn);
	if (r.reply_type != AMQP_RESPONSE_NORMAL) {
		throw std::runtime_error("Error declaring queue " + std::string(_qName));
	}

	amqp_basic_consume(_conn, _channel, amqp_cstring_bytes(_qName), amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
	r = amqp_get_rpc_reply(_conn);
	if (r.reply_type != AMQP_RESPONSE_NORMAL) {
		throw std::runtime_error("Error consuming queue " + std::string(_qName));
	}
	fprintf(stderr, "RabbitMQ Init OK %s\n", _qName);
}

std::string RabbitMQ::consume()
{
	amqp_rpc_reply_t res;
	amqp_envelope_t envelope;
	amqp_maybe_release_buffers(_conn);

	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	res = amqp_consume_message(_conn, &envelope, &timeout, 0);
	if (res.reply_type != AMQP_RESPONSE_NORMAL) {
		if (res.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION && res.library_error == AMQP_STATUS_TIMEOUT) {
			// timeout waiting for message.  Return empty string
			return "";
		} else {
			throw std::runtime_error("Error getting message");
		}
	}

	std::string msg(
		(const char*)envelope.message.body.bytes,
		envelope.message.body.len
	);
	amqp_destroy_envelope(&envelope);
	return msg;
}

}

#endif // ZT_CONTROLLER_USE_LIBPQ
