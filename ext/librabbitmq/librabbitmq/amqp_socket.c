/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MIT
 *
 * Portions created by Alan Antonuk are Copyright (c) 2012-2014
 * Alan Antonuk. All Rights Reserved.
 *
 * Portions created by VMware are Copyright (c) 2007-2012 VMware, Inc.
 * All Rights Reserved.
 *
 * Portions created by Tony Garnock-Jones are Copyright (c) 2009-2010
 * VMware, Inc. and Tony Garnock-Jones. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ***** END LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "amqp_private.h"
#include "amqp_socket.h"
#include "amqp_table.h"
#include "amqp_time.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#if ((defined(_WIN32)) || (defined(__MINGW32__)) || (defined(__MINGW64__)))
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
/* On older BSD types.h must come before net includes */
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifdef HAVE_SELECT
#include <sys/select.h>
#endif
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/uio.h>
#ifdef HAVE_POLL
#include <poll.h>
#endif
#include <unistd.h>
#endif

static int amqp_id_in_reply_list(amqp_method_number_t expected,
                                 amqp_method_number_t *list);

static int amqp_os_socket_init(void) {
#ifdef _WIN32
  static int called_wsastartup = 0;
  if (!called_wsastartup) {
    WSADATA data;
    int res = WSAStartup(0x0202, &data);
    if (res) {
      return AMQP_STATUS_TCP_SOCKETLIB_INIT_ERROR;
    }

    called_wsastartup = 1;
  }
  return AMQP_STATUS_OK;

#else
  return AMQP_STATUS_OK;
#endif
}

int amqp_os_socket_error(void) {
#ifdef _WIN32
  return WSAGetLastError();
#else
  return errno;
#endif
}

int amqp_os_socket_close(int sockfd) {
#ifdef _WIN32
  return closesocket(sockfd);
#else
  return close(sockfd);
#endif
}

ssize_t amqp_socket_send(amqp_socket_t *self, const void *buf, size_t len,
                         int flags) {
  assert(self);
  assert(self->klass->send);
  return self->klass->send(self, buf, len, flags);
}

ssize_t amqp_socket_recv(amqp_socket_t *self, void *buf, size_t len,
                         int flags) {
  assert(self);
  assert(self->klass->recv);
  return self->klass->recv(self, buf, len, flags);
}

int amqp_socket_open(amqp_socket_t *self, const char *host, int port) {
  assert(self);
  assert(self->klass->open);
  return self->klass->open(self, host, port, NULL);
}

int amqp_socket_open_noblock(amqp_socket_t *self, const char *host, int port,
                             struct timeval *timeout) {
  assert(self);
  assert(self->klass->open);
  return self->klass->open(self, host, port, timeout);
}

int amqp_socket_close(amqp_socket_t *self, amqp_socket_close_enum force) {
  assert(self);
  assert(self->klass->close);
  return self->klass->close(self, force);
}

void amqp_socket_delete(amqp_socket_t *self) {
  if (self) {
    assert(self->klass->delete);
    self->klass->delete (self);
  }
}

int amqp_socket_get_sockfd(amqp_socket_t *self) {
  assert(self);
  assert(self->klass->get_sockfd);
  return self->klass->get_sockfd(self);
}

int amqp_poll(int fd, int event, amqp_time_t deadline) {
#ifdef HAVE_POLL
  struct pollfd pfd;
  int res;
  int timeout_ms;

  /* Function should only ever be called with one of these two */
  assert(event == AMQP_SF_POLLIN || event == AMQP_SF_POLLOUT);

start_poll:
  pfd.fd = fd;
  switch (event) {
    case AMQP_SF_POLLIN:
      pfd.events = POLLIN;
      break;
    case AMQP_SF_POLLOUT:
      pfd.events = POLLOUT;
      break;
  }

  timeout_ms = amqp_time_ms_until(deadline);
  if (-1 > timeout_ms) {
    return timeout_ms;
  }

  res = poll(&pfd, 1, timeout_ms);

  if (0 < res) {
    /* TODO: optimize this a bit by returning the AMQP_STATUS_SOCKET_ERROR or
     * equivalent when pdf.revent is POLLHUP or POLLERR, so an extra syscall
     * doesn't need to be made. */
    return AMQP_STATUS_OK;
  } else if (0 == res) {
    return AMQP_STATUS_TIMEOUT;
  } else {
    switch (amqp_os_socket_error()) {
      case EINTR:
        goto start_poll;
      default:
        return AMQP_STATUS_SOCKET_ERROR;
    }
  }
  return AMQP_STATUS_OK;
#elif defined(HAVE_SELECT)
  fd_set fds;
  fd_set exceptfds;
  fd_set *exceptfdsp;
  int res;
  struct timeval tv;
  struct timeval *tvp;

  assert((0 != (event & AMQP_SF_POLLIN)) || (0 != (event & AMQP_SF_POLLOUT)));
#ifndef _WIN32
  /* On Win32 connect() failure is indicated through the exceptfds, it does not
   * make any sense to allow POLLERR on any other platform or condition */
  assert(0 == (event & AMQP_SF_POLLERR));
#endif

start_select:
  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  if (event & AMQP_SF_POLLERR) {
    FD_ZERO(&exceptfds);
    FD_SET(fd, &exceptfds);
    exceptfdsp = &exceptfds;
  } else {
    exceptfdsp = NULL;
  }

  res = amqp_time_tv_until(deadline, &tv, &tvp);
  if (res != AMQP_STATUS_OK) {
    return res;
  }

  if (event & AMQP_SF_POLLIN) {
    res = select(fd + 1, &fds, NULL, exceptfdsp, tvp);
  } else if (event & AMQP_SF_POLLOUT) {
    res = select(fd + 1, NULL, &fds, exceptfdsp, tvp);
  }

  if (0 < res) {
    return AMQP_STATUS_OK;
  } else if (0 == res) {
    return AMQP_STATUS_TIMEOUT;
  } else {
    switch (amqp_os_socket_error()) {
      case EINTR:
        goto start_select;
      default:
        return AMQP_STATUS_SOCKET_ERROR;
    }
  }
#else
#error "poll() or select() is needed to compile rabbitmq-c"
#endif
}

static ssize_t do_poll(amqp_connection_state_t state, ssize_t res,
                       amqp_time_t deadline) {
  int fd = amqp_get_sockfd(state);
  if (-1 == fd) {
    return AMQP_STATUS_SOCKET_CLOSED;
  }
  switch (res) {
    case AMQP_PRIVATE_STATUS_SOCKET_NEEDREAD:
      res = amqp_poll(fd, AMQP_SF_POLLIN, deadline);
      break;
    case AMQP_PRIVATE_STATUS_SOCKET_NEEDWRITE:
      res = amqp_poll(fd, AMQP_SF_POLLOUT, deadline);
      break;
  }
  return res;
}

ssize_t amqp_try_send(amqp_connection_state_t state, const void *buf,
                      size_t len, amqp_time_t deadline, int flags) {
  ssize_t res;
  void *buf_left = (void *)buf;
  /* Assume that len is not going to be larger than ssize_t can hold. */
  ssize_t len_left = (size_t)len;

start_send:
  res = amqp_socket_send(state->socket, buf_left, len_left, flags);

  if (res > 0) {
    len_left -= res;
    buf_left = (char *)buf_left + res;
    if (0 == len_left) {
      return (ssize_t)len;
    }
    goto start_send;
  }
  res = do_poll(state, res, deadline);
  if (AMQP_STATUS_OK == res) {
    goto start_send;
  }
  if (AMQP_STATUS_TIMEOUT == res) {
    return (ssize_t)len - len_left;
  }
  return res;
}

int amqp_open_socket(char const *hostname, int portnumber) {
  return amqp_open_socket_inner(hostname, portnumber, amqp_time_infinite());
}

int amqp_open_socket_noblock(char const *hostname, int portnumber,
                             struct timeval *timeout) {
  amqp_time_t deadline;
  int res = amqp_time_from_now(&deadline, timeout);
  if (AMQP_STATUS_OK != res) {
    return res;
  }
  return amqp_open_socket_inner(hostname, portnumber, deadline);
}

#ifdef _WIN32
static int connect_socket(struct addrinfo *addr, amqp_time_t deadline) {
  int one = 1;
  SOCKET sockfd;
  int last_error;

  /*
   * This cast is to squash warnings on Win64, see:
   * http://stackoverflow.com/questions/1953639/is-it-safe-to-cast-socket-to-int-under-win64
   */

  sockfd = (int)socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
  if (INVALID_SOCKET == sockfd) {
    return AMQP_STATUS_SOCKET_ERROR;
  }

  /* Set the socket to be non-blocking */
  if (SOCKET_ERROR == ioctlsocket(sockfd, FIONBIO, &one)) {
    last_error = AMQP_STATUS_SOCKET_ERROR;
    goto err;
  }

  /* Disable nagle */
  if (SOCKET_ERROR == setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
                                 (const char *)&one, sizeof(one))) {
    last_error = AMQP_STATUS_SOCKET_ERROR;
    goto err;
  }

  /* Enable TCP keepalives */
  if (SOCKET_ERROR == setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE,
                                 (const char *)&one, sizeof(one))) {
    last_error = AMQP_STATUS_SOCKET_ERROR;
    goto err;
  }

  if (SOCKET_ERROR != connect(sockfd, addr->ai_addr, (int)addr->ai_addrlen)) {
    return (int)sockfd;
  }

  if (WSAEWOULDBLOCK != WSAGetLastError()) {
    last_error = AMQP_STATUS_SOCKET_ERROR;
    goto err;
  }

  last_error =
      amqp_poll((int)sockfd, AMQP_SF_POLLOUT | AMQP_SF_POLLERR, deadline);
  if (AMQP_STATUS_OK != last_error) {
    goto err;
  }

  {
    int result;
    int result_len = sizeof(result);

    if (SOCKET_ERROR == getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
                                   (char *)&result, &result_len) ||
        result != 0) {
      last_error = AMQP_STATUS_SOCKET_ERROR;
      goto err;
    }
  }

  return (int)sockfd;

err:
  closesocket(sockfd);
  return last_error;
}
#else
static int connect_socket(struct addrinfo *addr, amqp_time_t deadline) {
  int one = 1;
  int sockfd;
  int flags;
  int last_error;

  sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
  if (-1 == sockfd) {
    return AMQP_STATUS_SOCKET_ERROR;
  }

  /* Enable CLOEXEC on socket */
  flags = fcntl(sockfd, F_GETFD);
  if (flags == -1 || fcntl(sockfd, F_SETFD, (long)(flags | FD_CLOEXEC)) == -1) {
    last_error = AMQP_STATUS_SOCKET_ERROR;
    goto err;
  }

  /* Set the socket as non-blocking */
  flags = fcntl(sockfd, F_GETFL);
  if (flags == -1 || fcntl(sockfd, F_SETFL, (long)(flags | O_NONBLOCK)) == -1) {
    last_error = AMQP_STATUS_SOCKET_ERROR;
    goto err;
  }

#ifdef SO_NOSIGPIPE
  /* Turn off SIGPIPE on platforms that support it, BSD, MacOSX */
  if (0 != setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one))) {
    last_error = AMQP_STATUS_SOCKET_ERROR;
    goto err;
  }
#endif /* SO_NOSIGPIPE */

  /* Disable nagle */
  if (0 != setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one))) {
    last_error = AMQP_STATUS_SOCKET_ERROR;
    goto err;
  }

  /* Enable TCP keepalives */
  if (0 != setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one))) {
    last_error = AMQP_STATUS_SOCKET_ERROR;
    goto err;
  }

  if (0 == connect(sockfd, addr->ai_addr, addr->ai_addrlen)) {
    return sockfd;
  }

  if (EINPROGRESS != errno) {
    last_error = AMQP_STATUS_SOCKET_ERROR;
    goto err;
  }

  last_error = amqp_poll(sockfd, AMQP_SF_POLLOUT, deadline);
  if (AMQP_STATUS_OK != last_error) {
    goto err;
  }

  {
    int result;
    socklen_t result_len = sizeof(result);

    if (-1 == getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &result, &result_len) ||
        result != 0) {
      last_error = AMQP_STATUS_SOCKET_ERROR;
      goto err;
    }
  }

  return sockfd;

err:
  close(sockfd);
  return last_error;
}
#endif

int amqp_open_socket_inner(char const *hostname, int portnumber,
                           amqp_time_t deadline) {
  struct addrinfo hint;
  struct addrinfo *address_list;
  struct addrinfo *addr;
  char portnumber_string[33];
  int sockfd = -1;
  int last_error;

  last_error = amqp_os_socket_init();
  if (AMQP_STATUS_OK != last_error) {
    return last_error;
  }

  memset(&hint, 0, sizeof(hint));
  hint.ai_family = PF_UNSPEC; /* PF_INET or PF_INET6 */
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_protocol = IPPROTO_TCP;

  (void)sprintf(portnumber_string, "%d", portnumber);

  last_error = getaddrinfo(hostname, portnumber_string, &hint, &address_list);
  if (0 != last_error) {
    return AMQP_STATUS_HOSTNAME_RESOLUTION_FAILED;
  }

  for (addr = address_list; addr; addr = addr->ai_next) {
    sockfd = connect_socket(addr, deadline);

    if (sockfd >= 0) {
      last_error = AMQP_STATUS_OK;
      break;
    } else if (sockfd == AMQP_STATUS_TIMEOUT) {
      last_error = sockfd;
      break;
    }
  }

  freeaddrinfo(address_list);
  if (last_error != AMQP_STATUS_OK || sockfd == -1) {
    return last_error;
  }
  return sockfd;
}

static int send_header_inner(amqp_connection_state_t state,
                             amqp_time_t deadline) {
  ssize_t res;
  static const uint8_t header[8] = {'A',
                                    'M',
                                    'Q',
                                    'P',
                                    0,
                                    AMQP_PROTOCOL_VERSION_MAJOR,
                                    AMQP_PROTOCOL_VERSION_MINOR,
                                    AMQP_PROTOCOL_VERSION_REVISION};
  res = amqp_try_send(state, header, sizeof(header), deadline, AMQP_SF_NONE);
  if (sizeof(header) == res) {
    return AMQP_STATUS_OK;
  }
  return (int)res;
}

int amqp_send_header(amqp_connection_state_t state) {
  return send_header_inner(state, amqp_time_infinite());
}

static amqp_bytes_t sasl_method_name(amqp_sasl_method_enum method) {
  amqp_bytes_t res;

  switch (method) {
    case AMQP_SASL_METHOD_PLAIN:
      res = amqp_cstring_bytes("PLAIN");
      break;
    case AMQP_SASL_METHOD_EXTERNAL:
      res = amqp_cstring_bytes("EXTERNAL");
      break;

    default:
      amqp_abort("Invalid SASL method: %d", (int)method);
  }

  return res;
}

static int bytes_equal(amqp_bytes_t l, amqp_bytes_t r) {
  if (l.len == r.len) {
    if (l.bytes && r.bytes) {
      if (0 == memcmp(l.bytes, r.bytes, l.len)) {
        return 1;
      }
    }
  }
  return 0;
}

int sasl_mechanism_in_list(amqp_bytes_t mechanisms,
                           amqp_sasl_method_enum method) {
  amqp_bytes_t mechanism;
  amqp_bytes_t supported_mechanism;
  uint8_t *start;
  uint8_t *end;
  uint8_t *current;

  assert(NULL != mechanisms.bytes);

  mechanism = sasl_method_name(method);

  start = (uint8_t *)mechanisms.bytes;
  current = start;
  end = start + mechanisms.len;

  for (; current != end; start = current + 1) {
    /* HACK: SASL states that we should be parsing this string as a UTF-8
     * string, which we're plainly not doing here. At this point its not worth
     * dragging an entire UTF-8 parser for this one case, and this should work
     * most of the time */
    current = memchr(start, ' ', end - start);
    if (NULL == current) {
      current = end;
    }
    supported_mechanism.bytes = start;
    supported_mechanism.len = current - start;
    if (bytes_equal(mechanism, supported_mechanism)) {
      return 1;
    }
  }

  return 0;
}

static amqp_bytes_t sasl_response(amqp_pool_t *pool,
                                  amqp_sasl_method_enum method, va_list args) {
  amqp_bytes_t response;

  switch (method) {
    case AMQP_SASL_METHOD_PLAIN: {
      char *username = va_arg(args, char *);
      size_t username_len = strlen(username);
      char *password = va_arg(args, char *);
      size_t password_len = strlen(password);
      char *response_buf;

      amqp_pool_alloc_bytes(pool, strlen(username) + strlen(password) + 2,
                            &response);
      if (response.bytes == NULL)
      /* We never request a zero-length block, because of the +2
         above, so a NULL here really is ENOMEM. */
      {
        return response;
      }

      response_buf = response.bytes;
      response_buf[0] = 0;
      memcpy(response_buf + 1, username, username_len);
      response_buf[username_len + 1] = 0;
      memcpy(response_buf + username_len + 2, password, password_len);
      break;
    }
    case AMQP_SASL_METHOD_EXTERNAL: {
      char *identity = va_arg(args, char *);
      size_t identity_len = strlen(identity);

      amqp_pool_alloc_bytes(pool, identity_len, &response);
      if (response.bytes == NULL) {
        return response;
      }

      memcpy(response.bytes, identity, identity_len);
      break;
    }
    default:
      amqp_abort("Invalid SASL method: %d", (int)method);
  }

  return response;
}

amqp_boolean_t amqp_frames_enqueued(amqp_connection_state_t state) {
  return (state->first_queued_frame != NULL);
}

/*
 * Check to see if we have data in our buffer. If this returns 1, we
 * will avoid an immediate blocking read in amqp_simple_wait_frame.
 */
amqp_boolean_t amqp_data_in_buffer(amqp_connection_state_t state) {
  return (state->sock_inbound_offset < state->sock_inbound_limit);
}

static int consume_one_frame(amqp_connection_state_t state,
                             amqp_frame_t *decoded_frame) {
  int res;

  amqp_bytes_t buffer;
  buffer.len = state->sock_inbound_limit - state->sock_inbound_offset;
  buffer.bytes =
      ((char *)state->sock_inbound_buffer.bytes) + state->sock_inbound_offset;

  res = amqp_handle_input(state, buffer, decoded_frame);
  if (res < 0) {
    return res;
  }

  state->sock_inbound_offset += res;

  return AMQP_STATUS_OK;
}

static int recv_with_timeout(amqp_connection_state_t state,
                             amqp_time_t timeout) {
  ssize_t res;
  int fd;

start_recv:
  res = amqp_socket_recv(state->socket, state->sock_inbound_buffer.bytes,
                         state->sock_inbound_buffer.len, 0);

  if (res < 0) {
    fd = amqp_get_sockfd(state);
    if (-1 == fd) {
      return AMQP_STATUS_CONNECTION_CLOSED;
    }
    switch (res) {
      default:
        return (int)res;
      case AMQP_PRIVATE_STATUS_SOCKET_NEEDREAD:
        res = amqp_poll(fd, AMQP_SF_POLLIN, timeout);
        break;
      case AMQP_PRIVATE_STATUS_SOCKET_NEEDWRITE:
        res = amqp_poll(fd, AMQP_SF_POLLOUT, timeout);
        break;
    }
    if (AMQP_STATUS_OK == res) {
      goto start_recv;
    }
    return (int)res;
  }

  state->sock_inbound_limit = res;
  state->sock_inbound_offset = 0;

  res = amqp_time_s_from_now(&state->next_recv_heartbeat,
                             amqp_heartbeat_recv(state));
  if (AMQP_STATUS_OK != res) {
    return (int)res;
  }
  return AMQP_STATUS_OK;
}

int amqp_try_recv(amqp_connection_state_t state) {
  amqp_time_t timeout;

  while (amqp_data_in_buffer(state)) {
    amqp_frame_t frame;
    int res = consume_one_frame(state, &frame);

    if (AMQP_STATUS_OK != res) {
      return res;
    }

    if (frame.frame_type != 0) {
      amqp_pool_t *channel_pool;
      amqp_frame_t *frame_copy;
      amqp_link_t *link;

      channel_pool = amqp_get_or_create_channel_pool(state, frame.channel);
      if (NULL == channel_pool) {
        return AMQP_STATUS_NO_MEMORY;
      }

      frame_copy = amqp_pool_alloc(channel_pool, sizeof(amqp_frame_t));
      link = amqp_pool_alloc(channel_pool, sizeof(amqp_link_t));

      if (frame_copy == NULL || link == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }

      *frame_copy = frame;

      link->next = NULL;
      link->data = frame_copy;

      if (state->last_queued_frame == NULL) {
        state->first_queued_frame = link;
      } else {
        state->last_queued_frame->next = link;
      }
      state->last_queued_frame = link;
    }
  }
  timeout = amqp_time_immediate();

  return recv_with_timeout(state, timeout);
}

static int wait_frame_inner(amqp_connection_state_t state,
                            amqp_frame_t *decoded_frame,
                            amqp_time_t timeout_deadline) {
  amqp_time_t deadline;
  int res;

  for (;;) {
    while (amqp_data_in_buffer(state)) {
      res = consume_one_frame(state, decoded_frame);

      if (AMQP_STATUS_OK != res) {
        return res;
      }

      if (AMQP_FRAME_HEARTBEAT == decoded_frame->frame_type) {
        amqp_maybe_release_buffers_on_channel(state, 0);
        continue;
      }

      if (decoded_frame->frame_type != 0) {
        /* Complete frame was read. Return it. */
        return AMQP_STATUS_OK;
      }
    }

  beginrecv:
    res = amqp_time_has_past(state->next_send_heartbeat);
    if (AMQP_STATUS_TIMER_FAILURE == res) {
      return res;
    } else if (AMQP_STATUS_TIMEOUT == res) {
      amqp_frame_t heartbeat;
      heartbeat.channel = 0;
      heartbeat.frame_type = AMQP_FRAME_HEARTBEAT;

      res = amqp_send_frame(state, &heartbeat);
      if (AMQP_STATUS_OK != res) {
        return res;
      }
    }
    deadline = amqp_time_first(timeout_deadline,
                               amqp_time_first(state->next_recv_heartbeat,
                                               state->next_send_heartbeat));

    /* TODO this needs to wait for a _frame_ and not anything written from the
     * socket */
    res = recv_with_timeout(state, deadline);

    if (AMQP_STATUS_TIMEOUT == res) {
      if (amqp_time_equal(deadline, state->next_recv_heartbeat)) {
        amqp_socket_close(state->socket, AMQP_SC_FORCE);
        return AMQP_STATUS_HEARTBEAT_TIMEOUT;
      } else if (amqp_time_equal(deadline, timeout_deadline)) {
        return AMQP_STATUS_TIMEOUT;
      } else if (amqp_time_equal(deadline, state->next_send_heartbeat)) {
        /* send heartbeat happens before we do recv_with_timeout */
        goto beginrecv;
      } else {
        amqp_abort("Internal error: unable to determine timeout reason");
      }
    } else if (AMQP_STATUS_OK != res) {
      return res;
    }
  }
}

static amqp_link_t *amqp_create_link_for_frame(amqp_connection_state_t state,
                                               amqp_frame_t *frame) {
  amqp_link_t *link;
  amqp_frame_t *frame_copy;

  amqp_pool_t *channel_pool =
      amqp_get_or_create_channel_pool(state, frame->channel);

  if (NULL == channel_pool) {
    return NULL;
  }

  link = amqp_pool_alloc(channel_pool, sizeof(amqp_link_t));
  frame_copy = amqp_pool_alloc(channel_pool, sizeof(amqp_frame_t));

  if (NULL == link || NULL == frame_copy) {
    return NULL;
  }

  *frame_copy = *frame;
  link->data = frame_copy;

  return link;
}

int amqp_queue_frame(amqp_connection_state_t state, amqp_frame_t *frame) {
  amqp_link_t *link = amqp_create_link_for_frame(state, frame);
  if (NULL == link) {
    return AMQP_STATUS_NO_MEMORY;
  }

  if (NULL == state->first_queued_frame) {
    state->first_queued_frame = link;
  } else {
    state->last_queued_frame->next = link;
  }

  link->next = NULL;
  state->last_queued_frame = link;

  return AMQP_STATUS_OK;
}

int amqp_put_back_frame(amqp_connection_state_t state, amqp_frame_t *frame) {
  amqp_link_t *link = amqp_create_link_for_frame(state, frame);
  if (NULL == link) {
    return AMQP_STATUS_NO_MEMORY;
  }

  if (NULL == state->first_queued_frame) {
    state->first_queued_frame = link;
    state->last_queued_frame = link;
    link->next = NULL;
  } else {
    link->next = state->first_queued_frame;
    state->first_queued_frame = link;
  }

  return AMQP_STATUS_OK;
}

int amqp_simple_wait_frame_on_channel(amqp_connection_state_t state,
                                      amqp_channel_t channel,
                                      amqp_frame_t *decoded_frame) {
  amqp_frame_t *frame_ptr;
  amqp_link_t *cur;
  int res;

  for (cur = state->first_queued_frame; NULL != cur; cur = cur->next) {
    frame_ptr = cur->data;

    if (channel == frame_ptr->channel) {
      state->first_queued_frame = cur->next;
      if (NULL == state->first_queued_frame) {
        state->last_queued_frame = NULL;
      }

      *decoded_frame = *frame_ptr;

      return AMQP_STATUS_OK;
    }
  }

  for (;;) {
    res = wait_frame_inner(state, decoded_frame, amqp_time_infinite());

    if (AMQP_STATUS_OK != res) {
      return res;
    }

    if (channel == decoded_frame->channel) {
      return AMQP_STATUS_OK;
    } else {
      res = amqp_queue_frame(state, decoded_frame);
      if (res != AMQP_STATUS_OK) {
        return res;
      }
    }
  }
}

int amqp_simple_wait_frame(amqp_connection_state_t state,
                           amqp_frame_t *decoded_frame) {
  return amqp_simple_wait_frame_noblock(state, decoded_frame, NULL);
}

int amqp_simple_wait_frame_noblock(amqp_connection_state_t state,
                                   amqp_frame_t *decoded_frame,
                                   struct timeval *timeout) {
  amqp_time_t deadline;

  int res = amqp_time_from_now(&deadline, timeout);
  if (AMQP_STATUS_OK != res) {
    return res;
  }

  if (state->first_queued_frame != NULL) {
    amqp_frame_t *f = (amqp_frame_t *)state->first_queued_frame->data;
    state->first_queued_frame = state->first_queued_frame->next;
    if (state->first_queued_frame == NULL) {
      state->last_queued_frame = NULL;
    }
    *decoded_frame = *f;
    return AMQP_STATUS_OK;
  } else {
    return wait_frame_inner(state, decoded_frame, deadline);
  }
}

static int amqp_simple_wait_method_list(amqp_connection_state_t state,
                                        amqp_channel_t expected_channel,
                                        amqp_method_number_t *expected_methods,
                                        amqp_time_t deadline,
                                        amqp_method_t *output) {
  amqp_frame_t frame;
  struct timeval tv;
  struct timeval *tvp;

  int res = amqp_time_tv_until(deadline, &tv, &tvp);
  if (res != AMQP_STATUS_OK) {
    return res;
  }

  res = amqp_simple_wait_frame_noblock(state, &frame, tvp);
  if (AMQP_STATUS_OK != res) {
    return res;
  }

  if (AMQP_FRAME_METHOD != frame.frame_type ||
      expected_channel != frame.channel ||
      !amqp_id_in_reply_list(frame.payload.method.id, expected_methods)) {
    return AMQP_STATUS_WRONG_METHOD;
  }
  *output = frame.payload.method;
  return AMQP_STATUS_OK;
}

static int simple_wait_method_inner(amqp_connection_state_t state,
                                    amqp_channel_t expected_channel,
                                    amqp_method_number_t expected_method,
                                    amqp_time_t deadline,
                                    amqp_method_t *output) {
  amqp_method_number_t expected_methods[] = {expected_method, 0};
  return amqp_simple_wait_method_list(state, expected_channel, expected_methods,
                                      deadline, output);
}

int amqp_simple_wait_method(amqp_connection_state_t state,
                            amqp_channel_t expected_channel,
                            amqp_method_number_t expected_method,
                            amqp_method_t *output) {
  return simple_wait_method_inner(state, expected_channel, expected_method,
                                  amqp_time_infinite(), output);
}

int amqp_send_method(amqp_connection_state_t state, amqp_channel_t channel,
                     amqp_method_number_t id, void *decoded) {
  return amqp_send_method_inner(state, channel, id, decoded, AMQP_SF_NONE,
                                amqp_time_infinite());
}

int amqp_send_method_inner(amqp_connection_state_t state,
                           amqp_channel_t channel, amqp_method_number_t id,
                           void *decoded, int flags, amqp_time_t deadline) {
  amqp_frame_t frame;

  frame.frame_type = AMQP_FRAME_METHOD;
  frame.channel = channel;
  frame.payload.method.id = id;
  frame.payload.method.decoded = decoded;
  return amqp_send_frame_inner(state, &frame, flags, deadline);
}

static int amqp_id_in_reply_list(amqp_method_number_t expected,
                                 amqp_method_number_t *list) {
  while (*list != 0) {
    if (*list == expected) {
      return 1;
    }
    list++;
  }
  return 0;
}

static amqp_rpc_reply_t simple_rpc_inner(
    amqp_connection_state_t state, amqp_channel_t channel,
    amqp_method_number_t request_id, amqp_method_number_t *expected_reply_ids,
    void *decoded_request_method, amqp_time_t deadline) {
  int status;
  amqp_rpc_reply_t result;

  memset(&result, 0, sizeof(result));

  status = amqp_send_method(state, channel, request_id, decoded_request_method);
  if (status < 0) {
    return amqp_rpc_reply_error(status);
  }

  {
    amqp_frame_t frame;

  retry:
    status = wait_frame_inner(state, &frame, deadline);
    if (status < 0) {
      if (status == AMQP_STATUS_TIMEOUT) {
        amqp_socket_close(state->socket, AMQP_SC_FORCE);
      }
      return amqp_rpc_reply_error(status);
    }

    /*
     * We store the frame for later processing unless it's something
     * that directly affects us here, namely a method frame that is
     * either
     *  - on the channel we want, and of the expected type, or
     *  - on the channel we want, and a channel.close frame, or
     *  - on channel zero, and a connection.close frame.
     */
    if (!((frame.frame_type == AMQP_FRAME_METHOD) &&
          (((frame.channel == channel) &&
            (amqp_id_in_reply_list(frame.payload.method.id,
                                   expected_reply_ids) ||
             (frame.payload.method.id == AMQP_CHANNEL_CLOSE_METHOD))) ||
           ((frame.channel == 0) &&
            (frame.payload.method.id == AMQP_CONNECTION_CLOSE_METHOD))))) {
      amqp_pool_t *channel_pool;
      amqp_frame_t *frame_copy;
      amqp_link_t *link;

      channel_pool = amqp_get_or_create_channel_pool(state, frame.channel);
      if (NULL == channel_pool) {
        return amqp_rpc_reply_error(AMQP_STATUS_NO_MEMORY);
      }

      frame_copy = amqp_pool_alloc(channel_pool, sizeof(amqp_frame_t));
      link = amqp_pool_alloc(channel_pool, sizeof(amqp_link_t));

      if (frame_copy == NULL || link == NULL) {
        return amqp_rpc_reply_error(AMQP_STATUS_NO_MEMORY);
      }

      *frame_copy = frame;

      link->next = NULL;
      link->data = frame_copy;

      if (state->last_queued_frame == NULL) {
        state->first_queued_frame = link;
      } else {
        state->last_queued_frame->next = link;
      }
      state->last_queued_frame = link;

      goto retry;
    }

    result.reply_type =
        (amqp_id_in_reply_list(frame.payload.method.id, expected_reply_ids))
            ? AMQP_RESPONSE_NORMAL
            : AMQP_RESPONSE_SERVER_EXCEPTION;

    result.reply = frame.payload.method;
    return result;
  }
}

amqp_rpc_reply_t amqp_simple_rpc(amqp_connection_state_t state,
                                 amqp_channel_t channel,
                                 amqp_method_number_t request_id,
                                 amqp_method_number_t *expected_reply_ids,
                                 void *decoded_request_method) {
  amqp_time_t deadline;
  int res;

  res = amqp_time_from_now(&deadline, state->rpc_timeout);
  if (res != AMQP_STATUS_OK) {
    return amqp_rpc_reply_error(res);
  }

  return simple_rpc_inner(state, channel, request_id, expected_reply_ids,
                          decoded_request_method, deadline);
}

void *amqp_simple_rpc_decoded(amqp_connection_state_t state,
                              amqp_channel_t channel,
                              amqp_method_number_t request_id,
                              amqp_method_number_t reply_id,
                              void *decoded_request_method) {
  amqp_time_t deadline;
  int res;
  amqp_method_number_t replies[2];

  res = amqp_time_from_now(&deadline, state->rpc_timeout);
  if (res != AMQP_STATUS_OK) {
    state->most_recent_api_result = amqp_rpc_reply_error(res);
    return NULL;
  }

  replies[0] = reply_id;
  replies[1] = 0;

  state->most_recent_api_result = simple_rpc_inner(
      state, channel, request_id, replies, decoded_request_method, deadline);

  if (state->most_recent_api_result.reply_type == AMQP_RESPONSE_NORMAL) {
    return state->most_recent_api_result.reply.decoded;
  } else {
    return NULL;
  }
}

amqp_rpc_reply_t amqp_get_rpc_reply(amqp_connection_state_t state) {
  return state->most_recent_api_result;
}

/*
 * Merge base and add tables. If the two tables contain an entry with the same
 * key, the entry from the add table takes precedence. For entries that are both
 * tables with the same key, the table is recursively merged.
 */
int amqp_merge_capabilities(const amqp_table_t *base, const amqp_table_t *add,
                            amqp_table_t *result, amqp_pool_t *pool) {
  int i;
  int res;
  amqp_pool_t temp_pool;
  amqp_table_t temp_result;
  assert(base != NULL);
  assert(result != NULL);
  assert(pool != NULL);

  if (NULL == add) {
    return amqp_table_clone(base, result, pool);
  }

  init_amqp_pool(&temp_pool, 4096);
  temp_result.num_entries = 0;
  temp_result.entries =
      amqp_pool_alloc(&temp_pool, sizeof(amqp_table_entry_t) *
                                      (base->num_entries + add->num_entries));
  if (NULL == temp_result.entries) {
    res = AMQP_STATUS_NO_MEMORY;
    goto error_out;
  }
  for (i = 0; i < base->num_entries; ++i) {
    temp_result.entries[temp_result.num_entries] = base->entries[i];
    temp_result.num_entries++;
  }
  for (i = 0; i < add->num_entries; ++i) {
    amqp_table_entry_t *e =
        amqp_table_get_entry_by_key(&temp_result, add->entries[i].key);
    if (NULL != e) {
      if (AMQP_FIELD_KIND_TABLE == add->entries[i].value.kind &&
          AMQP_FIELD_KIND_TABLE == e->value.kind) {
        amqp_table_entry_t *be =
            amqp_table_get_entry_by_key(base, add->entries[i].key);

        res = amqp_merge_capabilities(&be->value.value.table,
                                      &add->entries[i].value.value.table,
                                      &e->value.value.table, &temp_pool);
        if (AMQP_STATUS_OK != res) {
          goto error_out;
        }
      } else {
        e->value = add->entries[i].value;
      }
    } else {
      temp_result.entries[temp_result.num_entries] = add->entries[i];
      temp_result.num_entries++;
    }
  }
  res = amqp_table_clone(&temp_result, result, pool);
error_out:
  empty_amqp_pool(&temp_pool);
  return res;
}

static amqp_rpc_reply_t amqp_login_inner(
    amqp_connection_state_t state, char const *vhost, int channel_max,
    int frame_max, int heartbeat, const amqp_table_t *client_properties,
    struct timeval *timeout, amqp_sasl_method_enum sasl_method, va_list vl) {
  int res;
  amqp_method_t method;

  uint16_t client_channel_max;
  uint32_t client_frame_max;
  uint16_t client_heartbeat;

  uint16_t server_channel_max;
  uint32_t server_frame_max;
  uint16_t server_heartbeat;

  amqp_rpc_reply_t result;
  amqp_time_t deadline;

  if (channel_max < 0 || channel_max > UINT16_MAX) {
    return amqp_rpc_reply_error(AMQP_STATUS_INVALID_PARAMETER);
  }
  client_channel_max = (uint16_t)channel_max;

  if (frame_max < 0) {
    return amqp_rpc_reply_error(AMQP_STATUS_INVALID_PARAMETER);
  }
  client_frame_max = (uint32_t)frame_max;

  if (heartbeat < 0 || heartbeat > UINT16_MAX) {
    return amqp_rpc_reply_error(AMQP_STATUS_INVALID_PARAMETER);
  }
  client_heartbeat = (uint16_t)heartbeat;

  res = amqp_time_from_now(&deadline, timeout);
  if (AMQP_STATUS_OK != res) {
    goto error_res;
  }

  res = send_header_inner(state, deadline);
  if (AMQP_STATUS_OK != res) {
    goto error_res;
  }

  res = simple_wait_method_inner(state, 0, AMQP_CONNECTION_START_METHOD,
                                 deadline, &method);
  if (AMQP_STATUS_OK != res) {
    goto error_res;
  }

  {
    amqp_connection_start_t *s = (amqp_connection_start_t *)method.decoded;
    if ((s->version_major != AMQP_PROTOCOL_VERSION_MAJOR) ||
        (s->version_minor != AMQP_PROTOCOL_VERSION_MINOR)) {
      res = AMQP_STATUS_INCOMPATIBLE_AMQP_VERSION;
      goto error_res;
    }

    res = amqp_table_clone(&s->server_properties, &state->server_properties,
                           &state->properties_pool);

    if (AMQP_STATUS_OK != res) {
      goto error_res;
    }

    /* TODO: check that our chosen SASL mechanism is in the list of
       acceptable mechanisms. Or even let the application choose from
       the list! */
    if (!sasl_mechanism_in_list(s->mechanisms, sasl_method)) {
      res = AMQP_STATUS_BROKER_UNSUPPORTED_SASL_METHOD;
      goto error_res;
    }
  }

  {
    amqp_table_entry_t default_properties[6];
    amqp_table_t default_table;
    amqp_table_entry_t client_capabilities[2];
    amqp_table_t client_capabilities_table;
    amqp_connection_start_ok_t s;
    amqp_pool_t *channel_pool;
    amqp_bytes_t response_bytes;

    channel_pool = amqp_get_or_create_channel_pool(state, 0);
    if (NULL == channel_pool) {
      res = AMQP_STATUS_NO_MEMORY;
      goto error_res;
    }

    response_bytes = sasl_response(channel_pool, sasl_method, vl);
    if (response_bytes.bytes == NULL) {
      res = AMQP_STATUS_NO_MEMORY;
      goto error_res;
    }

    client_capabilities[0] =
        amqp_table_construct_bool_entry("authentication_failure_close", 1);
    client_capabilities[1] =
        amqp_table_construct_bool_entry("exchange_exchange_bindings", 1);

    client_capabilities_table.entries = client_capabilities;
    client_capabilities_table.num_entries =
        sizeof(client_capabilities) / sizeof(amqp_table_entry_t);

    default_properties[0] =
        amqp_table_construct_utf8_entry("product", "rabbitmq-c");
    default_properties[1] =
        amqp_table_construct_utf8_entry("version", AMQP_VERSION_STRING);
    default_properties[2] =
        amqp_table_construct_utf8_entry("platform", AMQ_PLATFORM);
    default_properties[3] =
        amqp_table_construct_utf8_entry("copyright", AMQ_COPYRIGHT);
    default_properties[4] = amqp_table_construct_utf8_entry(
        "information", "See https://github.com/alanxz/rabbitmq-c");
    default_properties[5] = amqp_table_construct_table_entry(
        "capabilities", &client_capabilities_table);

    default_table.entries = default_properties;
    default_table.num_entries =
        sizeof(default_properties) / sizeof(amqp_table_entry_t);

    res = amqp_merge_capabilities(&default_table, client_properties,
                                  &state->client_properties, channel_pool);
    if (AMQP_STATUS_OK != res) {
      goto error_res;
    }

    s.client_properties = state->client_properties;
    s.mechanism = sasl_method_name(sasl_method);
    s.response = response_bytes;
    s.locale = amqp_cstring_bytes("en_US");

    res = amqp_send_method_inner(state, 0, AMQP_CONNECTION_START_OK_METHOD, &s,
                                 AMQP_SF_NONE, deadline);
    if (res < 0) {
      goto error_res;
    }
  }

  amqp_release_buffers(state);

  {
    amqp_method_number_t expected[] = {AMQP_CONNECTION_TUNE_METHOD,
                                       AMQP_CONNECTION_CLOSE_METHOD, 0};

    res = amqp_simple_wait_method_list(state, 0, expected, deadline, &method);
    if (AMQP_STATUS_OK != res) {
      goto error_res;
    }
  }

  if (AMQP_CONNECTION_CLOSE_METHOD == method.id) {
    result.reply_type = AMQP_RESPONSE_SERVER_EXCEPTION;
    result.reply = method;
    result.library_error = 0;
    goto out;
  }

  {
    amqp_connection_tune_t *s = (amqp_connection_tune_t *)method.decoded;
    server_channel_max = s->channel_max;
    server_frame_max = s->frame_max;
    server_heartbeat = s->heartbeat;
  }

  if (server_channel_max != 0 &&
      (server_channel_max < client_channel_max || client_channel_max == 0)) {
    client_channel_max = server_channel_max;
  } else if (server_channel_max == 0 && client_channel_max == 0) {
    client_channel_max = UINT16_MAX;
  }

  if (server_frame_max != 0 && server_frame_max < client_frame_max) {
    client_frame_max = server_frame_max;
  }

  if (server_heartbeat != 0 && server_heartbeat < client_heartbeat) {
    client_heartbeat = server_heartbeat;
  }

  res = amqp_tune_connection(state, client_channel_max, client_frame_max,
                             client_heartbeat);
  if (res < 0) {
    goto error_res;
  }

  {
    amqp_connection_tune_ok_t s;
    s.frame_max = client_frame_max;
    s.channel_max = client_channel_max;
    s.heartbeat = client_heartbeat;

    res = amqp_send_method_inner(state, 0, AMQP_CONNECTION_TUNE_OK_METHOD, &s,
                                 AMQP_SF_NONE, deadline);
    if (res < 0) {
      goto error_res;
    }
  }

  amqp_release_buffers(state);

  {
    amqp_method_number_t replies[] = {AMQP_CONNECTION_OPEN_OK_METHOD, 0};
    amqp_connection_open_t s;
    s.virtual_host = amqp_cstring_bytes(vhost);
    s.capabilities = amqp_empty_bytes;
    s.insist = 1;

    result = simple_rpc_inner(state, 0, AMQP_CONNECTION_OPEN_METHOD, replies,
                              &s, deadline);
    if (result.reply_type != AMQP_RESPONSE_NORMAL) {
      goto out;
    }
  }

  result.reply_type = AMQP_RESPONSE_NORMAL;
  result.reply.id = 0;
  result.reply.decoded = NULL;
  result.library_error = 0;
  amqp_maybe_release_buffers(state);

out:
  return result;

error_res:
  amqp_socket_close(state->socket, AMQP_SC_FORCE);
  result = amqp_rpc_reply_error(res);

  goto out;
}

amqp_rpc_reply_t amqp_login(amqp_connection_state_t state, char const *vhost,
                            int channel_max, int frame_max, int heartbeat,
                            int sasl_method, ...) {
  va_list vl;
  amqp_rpc_reply_t ret;

  va_start(vl, sasl_method);

  ret = amqp_login_inner(state, vhost, channel_max, frame_max, heartbeat,
                         &amqp_empty_table, state->handshake_timeout,
                         sasl_method, vl);

  va_end(vl);

  return ret;
}

amqp_rpc_reply_t amqp_login_with_properties(
    amqp_connection_state_t state, char const *vhost, int channel_max,
    int frame_max, int heartbeat, const amqp_table_t *client_properties,
    int sasl_method, ...) {
  va_list vl;
  amqp_rpc_reply_t ret;

  va_start(vl, sasl_method);

  ret = amqp_login_inner(state, vhost, channel_max, frame_max, heartbeat,
                         client_properties, state->handshake_timeout,
                         sasl_method, vl);

  va_end(vl);

  return ret;
}
