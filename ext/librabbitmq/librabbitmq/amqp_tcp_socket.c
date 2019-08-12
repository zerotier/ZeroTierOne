/*
 * Copyright 2012-2013 Michael Steinert
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "amqp_private.h"
#include "amqp_tcp_socket.h"

#include <errno.h>
#if ((defined(_WIN32)) || (defined(__MINGW32__)) || (defined(__MINGW64__)))
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif
#include <stdio.h>
#include <stdlib.h>

struct amqp_tcp_socket_t {
  const struct amqp_socket_class_t *klass;
  int sockfd;
  int internal_error;
  int state;
};

static ssize_t amqp_tcp_socket_send(void *base, const void *buf, size_t len,
                                    int flags) {
  struct amqp_tcp_socket_t *self = (struct amqp_tcp_socket_t *)base;
  ssize_t res;
  int flagz = 0;

  if (-1 == self->sockfd) {
    return AMQP_STATUS_SOCKET_CLOSED;
  }

#ifdef MSG_NOSIGNAL
  flagz |= MSG_NOSIGNAL;
#endif

#if defined(MSG_MORE)
  if (flags & AMQP_SF_MORE) {
    flagz |= MSG_MORE;
  }
/* Cygwin defines TCP_NOPUSH, but trying to use it will return not
 * implemented. Disable it here. */
#elif defined(TCP_NOPUSH) && !defined(__CYGWIN__)
  if (flags & AMQP_SF_MORE && !(self->state & AMQP_SF_MORE)) {
    int one = 1;
    res = setsockopt(self->sockfd, IPPROTO_TCP, TCP_NOPUSH, &one, sizeof(one));
    if (0 != res) {
      self->internal_error = res;
      return AMQP_STATUS_SOCKET_ERROR;
    }
    self->state |= AMQP_SF_MORE;
  } else if (!(flags & AMQP_SF_MORE) && self->state & AMQP_SF_MORE) {
    int zero = 0;
    res =
        setsockopt(self->sockfd, IPPROTO_TCP, TCP_NOPUSH, &zero, sizeof(&zero));
    if (0 != res) {
      self->internal_error = res;
      res = AMQP_STATUS_SOCKET_ERROR;
    } else {
      self->state &= ~AMQP_SF_MORE;
    }
  }
#endif

start:
#ifdef _WIN32
  res = send(self->sockfd, buf, (int)len, flagz);
#else
  res = send(self->sockfd, buf, len, flagz);
#endif

  if (res < 0) {
    self->internal_error = amqp_os_socket_error();
    switch (self->internal_error) {
      case EINTR:
        goto start;
#ifdef _WIN32
      case WSAEWOULDBLOCK:
#else
      case EWOULDBLOCK:
#endif
#if defined(EAGAIN) && EAGAIN != EWOULDBLOCK
      case EAGAIN:
#endif
        res = AMQP_PRIVATE_STATUS_SOCKET_NEEDWRITE;
        break;
      default:
        res = AMQP_STATUS_SOCKET_ERROR;
    }
  } else {
    self->internal_error = 0;
  }

  return res;
}

static ssize_t amqp_tcp_socket_recv(void *base, void *buf, size_t len,
                                    int flags) {
  struct amqp_tcp_socket_t *self = (struct amqp_tcp_socket_t *)base;
  ssize_t ret;
  if (-1 == self->sockfd) {
    return AMQP_STATUS_SOCKET_CLOSED;
  }

start:
#ifdef _WIN32
  ret = recv(self->sockfd, buf, (int)len, flags);
#else
  ret = recv(self->sockfd, buf, len, flags);
#endif

  if (0 > ret) {
    self->internal_error = amqp_os_socket_error();
    switch (self->internal_error) {
      case EINTR:
        goto start;
#ifdef _WIN32
      case WSAEWOULDBLOCK:
#else
      case EWOULDBLOCK:
#endif
#if defined(EAGAIN) && EAGAIN != EWOULDBLOCK
      case EAGAIN:
#endif
        ret = AMQP_PRIVATE_STATUS_SOCKET_NEEDREAD;
        break;
      default:
        ret = AMQP_STATUS_SOCKET_ERROR;
    }
  } else if (0 == ret) {
    ret = AMQP_STATUS_CONNECTION_CLOSED;
  }

  return ret;
}

static int amqp_tcp_socket_open(void *base, const char *host, int port,
                                struct timeval *timeout) {
  struct amqp_tcp_socket_t *self = (struct amqp_tcp_socket_t *)base;
  if (-1 != self->sockfd) {
    return AMQP_STATUS_SOCKET_INUSE;
  }
  self->sockfd = amqp_open_socket_noblock(host, port, timeout);
  if (0 > self->sockfd) {
    int err = self->sockfd;
    self->sockfd = -1;
    return err;
  }
  return AMQP_STATUS_OK;
}

static int amqp_tcp_socket_close(void *base,
                                 AMQP_UNUSED amqp_socket_close_enum force) {
  struct amqp_tcp_socket_t *self = (struct amqp_tcp_socket_t *)base;
  if (-1 == self->sockfd) {
    return AMQP_STATUS_SOCKET_CLOSED;
  }

  if (amqp_os_socket_close(self->sockfd)) {
    return AMQP_STATUS_SOCKET_ERROR;
  }
  self->sockfd = -1;

  return AMQP_STATUS_OK;
}

static int amqp_tcp_socket_get_sockfd(void *base) {
  struct amqp_tcp_socket_t *self = (struct amqp_tcp_socket_t *)base;
  return self->sockfd;
}

static void amqp_tcp_socket_delete(void *base) {
  struct amqp_tcp_socket_t *self = (struct amqp_tcp_socket_t *)base;

  if (self) {
    amqp_tcp_socket_close(self, AMQP_SC_NONE);
    free(self);
  }
}

static const struct amqp_socket_class_t amqp_tcp_socket_class = {
    amqp_tcp_socket_send,       /* send */
    amqp_tcp_socket_recv,       /* recv */
    amqp_tcp_socket_open,       /* open */
    amqp_tcp_socket_close,      /* close */
    amqp_tcp_socket_get_sockfd, /* get_sockfd */
    amqp_tcp_socket_delete      /* delete */
};

amqp_socket_t *amqp_tcp_socket_new(amqp_connection_state_t state) {
  struct amqp_tcp_socket_t *self = calloc(1, sizeof(*self));
  if (!self) {
    return NULL;
  }
  self->klass = &amqp_tcp_socket_class;
  self->sockfd = -1;

  amqp_set_socket(state, (amqp_socket_t *)self);

  return (amqp_socket_t *)self;
}

void amqp_tcp_socket_set_sockfd(amqp_socket_t *base, int sockfd) {
  struct amqp_tcp_socket_t *self;
  if (base->klass != &amqp_tcp_socket_class) {
    amqp_abort("<%p> is not of type amqp_tcp_socket_t", base);
  }
  self = (struct amqp_tcp_socket_t *)base;
  self->sockfd = sockfd;
}
