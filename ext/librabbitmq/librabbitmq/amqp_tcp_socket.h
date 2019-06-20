/** \file */
/*
 * Portions created by Alan Antonuk are Copyright (c) 2013-2014 Alan Antonuk.
 * All Rights Reserved.
 *
 * Portions created by Michael Steinert are Copyright (c) 2012-2013 Michael
 * Steinert. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * A TCP socket connection.
 */

#ifndef AMQP_TCP_SOCKET_H
#define AMQP_TCP_SOCKET_H

#include <amqp.h>

AMQP_BEGIN_DECLS

/**
 * Create a new TCP socket.
 *
 * Call amqp_connection_close() to release socket resources.
 *
 * \return A new socket object or NULL if an error occurred.
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
amqp_socket_t *AMQP_CALL amqp_tcp_socket_new(amqp_connection_state_t state);

/**
 * Assign an open file descriptor to a socket object.
 *
 * This function must not be used in conjunction with amqp_socket_open(), i.e.
 * the socket connection should already be open(2) when this function is
 * called.
 *
 * \param [in,out] self A TCP socket object.
 * \param [in] sockfd An open socket descriptor.
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
void AMQP_CALL amqp_tcp_socket_set_sockfd(amqp_socket_t *self, int sockfd);

AMQP_END_DECLS

#endif /* AMQP_TCP_SOCKET_H */
