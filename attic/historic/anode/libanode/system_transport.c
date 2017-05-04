/* libanode: the Anode C reference implementation
 * Copyright (C) 2009-2010 Adam Ierymenko <adam.ierymenko@gmail.com>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <stdio.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "anode.h"
#include "impl/mutex.h"
#include "impl/thread.h"
#include "impl/misc.h"
#include "impl/dns_txt.h"

#ifdef WINDOWS
#include <windows.h>
#include <winsock2.h>
#define AnodeSystemTransport__close_socket(s) closesocket((s))
#define ANODE_USE_SELECT 1
#else
#include <poll.h>
#include <unistd.h>
#define AnodeSystemTransport__close_socket(s) close((s))
#endif

static const char *AnodeSystemTransport_CLASS = "SystemTransport";

/* ======================================================================== */

struct AnodeSystemTransport;

struct AnodeSystemTransport_AnodeSocket
{
  AnodeSocket base; /* must be first */
  unsigned int entry_idx;
};

#define ANODE_SYSTEM_TRANSPORT_DNS_MAX_RESULTS 16
struct AnodeSystemTransport__dns_request
{
  struct AnodeSystemTransport__dns_request *next;

  AnodeThread *thread;
  struct AnodeSystemTransport *owner;

  void (*event_handler)(const AnodeEvent *event);

  char name[256];  
  enum AnodeTransportDnsIncludeMode ipv4_include_mode;
  enum AnodeTransportDnsIncludeMode ipv6_include_mode;
  enum AnodeTransportDnsIncludeMode anode_include_mode;

  AnodeNetworkAddress addresses[ANODE_SYSTEM_TRANSPORT_DNS_MAX_RESULTS];
  unsigned int address_count;

  int error_code;
};

#ifdef ANODE_USE_SELECT
typedef int AnodeSystemTransport__poll_fd; /* for select() */
#else
typedef struct pollfd AnodeSystemTransport__poll_fd; /* for poll() */
#endif

struct AnodeSystemTransport
{
  AnodeTransport interface; /* must be first */

  AnodeTransport *base;

#ifdef ANODE_USE_SELECT
  FD_SET readfds;
  FD_SET writefds;
#endif

  void (*default_event_handler)(const AnodeEvent *event);

  AnodeSystemTransport__poll_fd *fds;
  struct AnodeSystemTransport_AnodeSocket *sockets;
  unsigned int fd_count;
  unsigned int fd_capacity;

  struct AnodeSystemTransport__dns_request *pending_dns_requests;

  int invoke_pipe[2];
  AnodeMutex invoke_pipe_m;
  void *invoke_pipe_buf[2];
  unsigned int invoke_pipe_buf_ptr;
};

/* ======================================================================== */
/* Internal helper methods                                                  */

static unsigned int AnodeSystemTransport__add_entry(struct AnodeSystemTransport *transport)
{
  if ((transport->fd_count + 1) > transport->fd_capacity) {
    transport->fd_capacity += 8;
    transport->fds = realloc(transport->fds,sizeof(AnodeSystemTransport__poll_fd) * transport->fd_capacity);
    transport->sockets = realloc(transport->sockets,sizeof(struct AnodeSystemTransport_AnodeSocket) * transport->fd_capacity);
  }
  return transport->fd_count++;
}

static void AnodeSystemTransport__remove_entry(struct AnodeSystemTransport *transport,const unsigned int idx)
{
  unsigned int i;

  --transport->fd_count;
  for(i=idx;i<transport->fd_count;++i) {
    Anode_memcpy(&transport->fds[i],&transport->fds[i+1],sizeof(AnodeSystemTransport__poll_fd));
    Anode_memcpy(&transport->sockets[i],&transport->sockets[i+1],sizeof(struct AnodeSystemTransport_AnodeSocket));
  }

  if ((transport->fd_capacity - transport->fd_count) > 16) {
    transport->fd_capacity -= 16;
    transport->fds = realloc(transport->fds,sizeof(AnodeSystemTransport__poll_fd) * transport->fd_capacity);
    transport->sockets = realloc(transport->sockets,sizeof(struct AnodeSystemTransport_AnodeSocket) * transport->fd_capacity);
  }
}

static void AnodeSystemTransport__dns_invoke_on_completion(void *_dreq)
{
  struct AnodeSystemTransport__dns_request *dreq = (struct AnodeSystemTransport__dns_request *)_dreq;
  struct AnodeSystemTransport__dns_request *ptr,**lastnext;

  AnodeThread_join(dreq->thread);

  ptr = dreq->owner->pending_dns_requests;
  lastnext = &dreq->owner->pending_dns_requests;
  while (ptr) {
    if (ptr == dreq) {
      *lastnext = ptr->next;
      break;
    } else {
      lastnext = &ptr->next;
      ptr = ptr->next;
    }
  }

  free(dreq);
}

static void AnodeSystemTransport__dns_thread_main(void *_dreq)
{
  struct AnodeSystemTransport__dns_request *dreq = (struct AnodeSystemTransport__dns_request *)_dreq;

  dreq->owner->interface.invoke((AnodeTransport *)dreq->owner,dreq,&AnodeSystemTransport__dns_invoke_on_completion);
}

static void AnodeSystemTransport__do_close(struct AnodeSystemTransport *transport,struct AnodeSystemTransport_AnodeSocket *sock,const int error_code,const int generate_event)
{
  AnodeEvent evbuf;
  int fd;

  if (sock->base.class_name == AnodeSystemTransport_CLASS) {
#ifdef ANODE_USE_SELECT
    fd = (int)(transport->fds[((struct AnodeSystemTransport_AnodeSocket *)sock)->entry_idx]);
#else
    fd = transport->fds[((struct AnodeSystemTransport_AnodeSocket *)sock)->entry_idx].fd;
#endif

    if ((sock->base.type == ANODE_SOCKET_STREAM_CONNECTION)&&(sock->base.state != ANODE_SOCKET_CLOSED)) {
      sock->base.state = ANODE_SOCKET_CLOSED;

      if (generate_event) {
        evbuf.type = ANODE_TRANSPORT_EVENT_STREAM_CLOSED;
        evbuf.transport = (AnodeTransport *)transport;
        evbuf.sock = (AnodeSocket *)sock;
        evbuf.datagram_from = NULL;
        evbuf.dns_name = NULL;
        evbuf.dns_addresses = NULL;
        evbuf.dns_address_count = 0;
        evbuf.error_code = error_code;
        evbuf.data_length = 0;
        evbuf.data = NULL;

        if (sock->base.event_handler)
          sock->base.event_handler(&evbuf);
        else if (transport->default_event_handler)
          transport->default_event_handler(&evbuf);
      }
    }

    AnodeSystemTransport__close_socket(fd);
    AnodeSystemTransport__remove_entry(transport,((struct AnodeSystemTransport_AnodeSocket *)sock)->entry_idx);

#ifdef ANODE_USE_SELECT
    FD_CLR(sock,&THIS->readfds);
    FD_CLR(sock,&THIS->writefds);
#endif
  } else transport->base->close(transport->base,(AnodeSocket *)sock);
}

static int AnodeSystemTransport__populate_network_endpoint(const struct sockaddr_storage *saddr,AnodeNetworkEndpoint *ep)
{
  switch(saddr->ss_family) {
    case AF_INET:
      ep->address.type = ANODE_NETWORK_ADDRESS_IPV4;
      *((uint32_t *)ep->address.bits) = ((struct sockaddr_in *)saddr)->sin_addr.s_addr;
      ep->port = ntohs(((struct sockaddr_in *)saddr)->sin_port);
      return 1;
    case AF_INET6:
      ep->address.type = ANODE_NETWORK_ADDRESS_IPV6;
      Anode_memcpy(ep->address.bits,((struct sockaddr_in6 *)saddr)->sin6_addr.s6_addr,16);
      ep->port = ntohs(((struct sockaddr_in6 *)saddr)->sin6_port);
      return 1;
  }
  return 0;
}

/* ======================================================================== */

#ifdef THIS
#undef THIS
#endif
#define THIS ((struct AnodeSystemTransport *)transport)

static void AnodeSystemTransport_invoke(AnodeTransport *transport,
  void *ptr,
  void (*func)(void *))
{
  void *invoke_msg[2];

  invoke_msg[0] = ptr;
  invoke_msg[1] = (void *)func;

  AnodeMutex_lock(&THIS->invoke_pipe_m);
  write(THIS->invoke_pipe[1],(void *)(&invoke_msg),sizeof(invoke_msg));
  AnodeMutex_unlock(&THIS->invoke_pipe_m);
}

static void AnodeSystemTransport_dns_resolve(AnodeTransport *transport,
  const char *name,
  void (*event_handler)(const AnodeEvent *),
  enum AnodeTransportDnsIncludeMode ipv4_include_mode,
  enum AnodeTransportDnsIncludeMode ipv6_include_mode,
  enum AnodeTransportDnsIncludeMode anode_include_mode)
{
  struct AnodeSystemTransport__dns_request *dreq = malloc(sizeof(struct AnodeSystemTransport__dns_request));

  dreq->owner = THIS;
  dreq->event_handler = event_handler;
  Anode_str_copy(dreq->name,name,sizeof(dreq->name));
  dreq->ipv4_include_mode = ipv4_include_mode;
  dreq->ipv6_include_mode = ipv6_include_mode;
  dreq->anode_include_mode = anode_include_mode;

  dreq->address_count = 0;
  dreq->error_code = 0;

  dreq->next = THIS->pending_dns_requests;
  THIS->pending_dns_requests = dreq;

  dreq->thread = AnodeThread_create(&AnodeSystemTransport__dns_thread_main,dreq,0);
}

static AnodeSocket *AnodeSystemTransport_datagram_listen(AnodeTransport *transport,
  const AnodeNetworkAddress *local_address,
  int local_port,
  int *error_code)
{
  struct sockaddr_in sin4;
  struct sockaddr_in6 sin6;
  struct AnodeSystemTransport_AnodeSocket *sock;
  unsigned int entry_idx;
  int fd;
  int tmp;

  switch(local_address->type) {
    case ANODE_NETWORK_ADDRESS_IPV4:
      fd = socket(AF_INET,SOCK_DGRAM,0);
      if (fd <= 0) {
        *error_code = ANODE_ERR_UNABLE_TO_BIND;
        return (AnodeSocket *)0;
      }
      tmp = 1;
      setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&tmp,sizeof(tmp));
      fcntl(fd,F_SETFL,O_NONBLOCK);

      Anode_zero(&sin4,sizeof(struct sockaddr_in));
      sin4.sin_family = AF_INET;
      sin4.sin_port = htons(local_port);
      sin4.sin_addr.s_addr = *((uint32_t *)local_address->bits);

      if (bind(fd,(const struct sockaddr *)&sin4,sizeof(sin4))) {
        AnodeSystemTransport__close_socket(fd);
        *error_code = ANODE_ERR_UNABLE_TO_BIND;
        return (AnodeSocket *)0;
      }
      break;
    case ANODE_NETWORK_ADDRESS_IPV6:
      fd = socket(AF_INET6,SOCK_DGRAM,0);
      if (fd <= 0) {
        *error_code = ANODE_ERR_UNABLE_TO_BIND;
        return (AnodeSocket *)0;
      }
      tmp = 1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&tmp,sizeof(tmp));
      fcntl(fd,F_SETFL,O_NONBLOCK);
#ifdef IPV6_V6ONLY
      tmp = 1; setsockopt(fd,IPPROTO_IPV6,IPV6_V6ONLY,&tmp,sizeof(tmp));
#endif

      Anode_zero(&sin6,sizeof(struct sockaddr_in6));
      sin6.sin6_family = AF_INET6;
      sin6.sin6_port = htons(local_port);
      Anode_memcpy(sin6.sin6_addr.s6_addr,local_address->bits,16);

      if (bind(fd,(const struct sockaddr *)&sin6,sizeof(sin6))) {
        AnodeSystemTransport__close_socket(fd);
        *error_code = ANODE_ERR_UNABLE_TO_BIND;
        return (AnodeSocket *)0;
      }
      break;
    default:
      if (THIS->base)
        return THIS->base->datagram_listen(THIS->base,local_address,local_port,error_code);
      else {
        *error_code = ANODE_ERR_ADDRESS_TYPE_NOT_SUPPORTED;
        return (AnodeSocket *)0;
      }
  }

  entry_idx = AnodeSystemTransport__add_entry(THIS);
  sock = &(THIS->sockets[entry_idx]);

  sock->base.type = ANODE_SOCKET_DATAGRAM;
  sock->base.state = ANODE_SOCKET_OPEN;
  Anode_memcpy(&sock->base.endpoint.address,local_address,sizeof(AnodeNetworkAddress));
  sock->base.endpoint.port = local_port;
  sock->base.class_name = AnodeSystemTransport_CLASS;
  sock->base.user_ptr[0] = NULL;
  sock->base.user_ptr[1] = NULL;
  sock->base.event_handler = NULL;
  sock->entry_idx = entry_idx;

  THIS->fds[entry_idx].fd = fd;
  THIS->fds[entry_idx].events = POLLIN;
  THIS->fds[entry_idx].revents = 0;

  *error_code = 0;
  return (AnodeSocket *)sock;
}

static AnodeSocket *AnodeSystemTransport_stream_listen(AnodeTransport *transport,
  const AnodeNetworkAddress *local_address,
  int local_port,
  int *error_code)
{
  struct sockaddr_in sin4;
  struct sockaddr_in6 sin6;
  struct AnodeSystemTransport_AnodeSocket *sock;
  unsigned int entry_idx;
  int fd;
  int tmp;

  switch(local_address->type) {
    case ANODE_NETWORK_ADDRESS_IPV4:
      fd = socket(AF_INET,SOCK_STREAM,0);
      if (fd < 0) {
        *error_code = ANODE_ERR_UNABLE_TO_BIND;
        return (AnodeSocket *)0;
      }
      fcntl(fd,F_SETFL,O_NONBLOCK);

      Anode_zero(&sin4,sizeof(struct sockaddr_in));
      sin4.sin_family = AF_INET;
      sin4.sin_port = htons(local_port);
      sin4.sin_addr.s_addr = *((uint32_t *)local_address->bits);

      if (bind(fd,(const struct sockaddr *)&sin4,sizeof(sin4))) {
        AnodeSystemTransport__close_socket(fd);
        *error_code = ANODE_ERR_UNABLE_TO_BIND;
        return (AnodeSocket *)0;
      }
      if (listen(fd,8)) {
        AnodeSystemTransport__close_socket(fd);
        *error_code = ANODE_ERR_UNABLE_TO_BIND;
        return (AnodeSocket *)0;
      }
      break;
    case ANODE_NETWORK_ADDRESS_IPV6:
      fd = socket(AF_INET6,SOCK_STREAM,0);
      if (fd < 0) {
        *error_code = ANODE_ERR_UNABLE_TO_BIND;
        return (AnodeSocket *)0;
      }
      fcntl(fd,F_SETFL,O_NONBLOCK);
#ifdef IPV6_V6ONLY
      tmp = 1; setsockopt(fd,IPPROTO_IPV6,IPV6_V6ONLY,&tmp,sizeof(tmp));
#endif

      Anode_zero(&sin6,sizeof(struct sockaddr_in6));
      sin6.sin6_family = AF_INET6;
      sin6.sin6_port = htons(local_port);
      Anode_memcpy(sin6.sin6_addr.s6_addr,local_address->bits,16);

      if (bind(fd,(const struct sockaddr *)&sin6,sizeof(sin6))) {
        AnodeSystemTransport__close_socket(fd);
        *error_code = ANODE_ERR_UNABLE_TO_BIND;
        return (AnodeSocket *)0;
      }
      if (listen(fd,8)) {
        AnodeSystemTransport__close_socket(fd);
        *error_code = ANODE_ERR_UNABLE_TO_BIND;
        return (AnodeSocket *)0;
      }
      break;
    default:
      if (THIS->base)
        return THIS->base->stream_listen(THIS->base,local_address,local_port,error_code);
      else {
        *error_code = ANODE_ERR_ADDRESS_TYPE_NOT_SUPPORTED;
        return (AnodeSocket *)0;
      }
  }

  entry_idx = AnodeSystemTransport__add_entry(THIS);
  sock = &(THIS->sockets[entry_idx]);

  sock->base.type = ANODE_SOCKET_STREAM_LISTEN;
  sock->base.state = ANODE_SOCKET_OPEN;
  Anode_memcpy(&sock->base.endpoint.address,local_address,sizeof(AnodeNetworkAddress));
  sock->base.endpoint.port = local_port;
  sock->base.class_name = AnodeSystemTransport_CLASS;
  sock->base.user_ptr[0] = NULL;
  sock->base.user_ptr[1] = NULL;
  sock->base.event_handler = NULL;
  sock->entry_idx = entry_idx;

  THIS->fds[entry_idx].fd = fd;
  THIS->fds[entry_idx].events = POLLIN;
  THIS->fds[entry_idx].revents = 0;

  *error_code = 0;
  return (AnodeSocket *)sock;
}

static int AnodeSystemTransport_datagram_send(AnodeTransport *transport,
  AnodeSocket *sock,
  const void *data,
  int data_len,
  const AnodeNetworkEndpoint *to_endpoint)
{
  struct sockaddr_in sin4;
  struct sockaddr_in6 sin6;

#ifdef ANODE_USE_SELECT
  const int fd = (int)(THIS->fds[((struct AnodeSystemTransport_AnodeSocket *)sock)->entry_idx]);
#else
  const int fd = THIS->fds[((struct AnodeSystemTransport_AnodeSocket *)sock)->entry_idx].fd;
#endif

  switch(to_endpoint->address.type) {
    case ANODE_NETWORK_ADDRESS_IPV4:
      Anode_zero(&sin4,sizeof(struct sockaddr_in));
      sin4.sin_family = AF_INET;
      sin4.sin_port = htons((uint16_t)to_endpoint->port);
      sin4.sin_addr.s_addr = *((uint32_t *)to_endpoint->address.bits);
      sendto(fd,data,data_len,0,(struct sockaddr *)&sin4,sizeof(sin4));
      return 0;
    case ANODE_NETWORK_ADDRESS_IPV6:
      Anode_zero(&sin6,sizeof(struct sockaddr_in6));
      sin6.sin6_family = AF_INET6;
      sin6.sin6_port = htons((uint16_t)to_endpoint->port);
      Anode_memcpy(sin6.sin6_addr.s6_addr,to_endpoint->address.bits,16);
      sendto(fd,data,data_len,0,(struct sockaddr *)&sin6,sizeof(sin6));
      return 0;
    default:
      if (THIS->base)
        return THIS->base->datagram_send(THIS->base,sock,data,data_len,to_endpoint);
      else return ANODE_ERR_ADDRESS_TYPE_NOT_SUPPORTED;
  }
}

static AnodeSocket *AnodeSystemTransport_stream_connect(AnodeTransport *transport,
  const AnodeNetworkEndpoint *to_endpoint,
  int *error_code)
{
  struct sockaddr_in sin4;
  struct sockaddr_in6 sin6;
  struct AnodeSystemTransport_AnodeSocket *sock;
  unsigned int entry_idx;
  int fd;

  switch(to_endpoint->address.type) {
    case ANODE_NETWORK_ADDRESS_IPV4:
      Anode_zero(&sin4,sizeof(struct sockaddr_in));
      sin4.sin_family = AF_INET;
      sin4.sin_port = htons(to_endpoint->port);
      sin4.sin_addr.s_addr = *((uint32_t *)to_endpoint->address.bits);

      fd = socket(AF_INET,SOCK_STREAM,0);
      if (fd < 0) {
        *error_code = ANODE_ERR_ADDRESS_TYPE_NOT_SUPPORTED;
        return (AnodeSocket *)0;
      }
      fcntl(fd,F_SETFL,O_NONBLOCK);

      if (connect(fd,(struct sockaddr *)&sin4,sizeof(sin4))) {
        if (errno != EINPROGRESS) {
          *error_code = ANODE_ERR_CONNECT_FAILED;
          AnodeSystemTransport__close_socket(fd);
          return (AnodeSocket *)0;
        }
      }
      break;
    case ANODE_NETWORK_ADDRESS_IPV6:
      Anode_zero(&sin6,sizeof(struct sockaddr_in6));
      sin6.sin6_family = AF_INET6;
      sin6.sin6_port = htons(to_endpoint->port);
      Anode_memcpy(sin6.sin6_addr.s6_addr,to_endpoint->address.bits,16);

      fd = socket(AF_INET6,SOCK_STREAM,0);
      if (fd < 0) {
        *error_code = ANODE_ERR_ADDRESS_TYPE_NOT_SUPPORTED;
        return (AnodeSocket *)0;
      }
      fcntl(fd,F_SETFL,O_NONBLOCK);

      if (connect(fd,(struct sockaddr *)&sin6,sizeof(sin6))) {
        if (errno == EINPROGRESS) {
          *error_code = ANODE_ERR_CONNECT_FAILED;
          AnodeSystemTransport__close_socket(fd);
          return (AnodeSocket *)0;
        }
      }
      break;
    default:
      if (THIS->base)
        return THIS->base->stream_connect(THIS->base,to_endpoint,error_code);
      else {
        *error_code = ANODE_ERR_ADDRESS_TYPE_NOT_SUPPORTED;
        return (AnodeSocket *)0;
      }
  }

  entry_idx = AnodeSystemTransport__add_entry(THIS);
  sock = &(THIS->sockets[entry_idx]);

  sock->base.type = ANODE_SOCKET_STREAM_CONNECTION;
  sock->base.state = ANODE_SOCKET_CONNECTING;
  Anode_memcpy(&sock->base.endpoint,to_endpoint,sizeof(AnodeNetworkEndpoint));
  sock->base.class_name = AnodeSystemTransport_CLASS;
  sock->base.user_ptr[0] = NULL;
  sock->base.user_ptr[1] = NULL;
  sock->base.event_handler = NULL;
  sock->entry_idx = entry_idx;

  THIS->fds[entry_idx].fd = fd;
  THIS->fds[entry_idx].events = POLLIN|POLLOUT;
  THIS->fds[entry_idx].revents = 0;

  return (AnodeSocket *)sock;
}

static void AnodeSystemTransport_stream_start_writing(AnodeTransport *transport,
  AnodeSocket *sock)
{
  if ((sock->type == ANODE_SOCKET_STREAM_CONNECTION)&&(((struct AnodeSystemTransport_AnodeSocket *)sock)->base.state == ANODE_SOCKET_OPEN)) {
    if (sock->class_name == AnodeSystemTransport_CLASS) {
#ifdef ANODE_USE_SELECT
      FD_SET((int)(THIS->fds[((struct AnodeSystemTransport_AnodeSocket *)sock)->entry_idx]),&THIS->writefds);
#else
      THIS->fds[((struct AnodeSystemTransport_AnodeSocket *)sock)->entry_idx].events = (POLLIN|POLLOUT);
#endif
    } else THIS->base->stream_start_writing(THIS->base,sock);
  }
}

static void AnodeSystemTransport_stream_stop_writing(AnodeTransport *transport,
  AnodeSocket *sock)
{
  if ((sock->type == ANODE_SOCKET_STREAM_CONNECTION)&&(((struct AnodeSystemTransport_AnodeSocket *)sock)->base.state == ANODE_SOCKET_OPEN)) {
    if (sock->class_name == AnodeSystemTransport_CLASS) {
#ifdef ANODE_USE_SELECT
      FD_CLR((int)(THIS->fds[((struct AnodeSystemTransport_AnodeSocket *)sock)->entry_idx]),&THIS->writefds);
#else
      THIS->fds[((struct AnodeSystemTransport_AnodeSocket *)sock)->entry_idx].events = POLLIN;
#endif
    } else THIS->base->stream_stop_writing(THIS->base,sock);
  }
}

static int AnodeSystemTransport_stream_send(AnodeTransport *transport,
  AnodeSocket *sock,
  const void *data,
  int data_len)
{
  int result;

  if (sock->type == ANODE_SOCKET_STREAM_CONNECTION) {
    if (sock->class_name == AnodeSystemTransport_CLASS) {
      if (((struct AnodeSystemTransport_AnodeSocket *)sock)->base.state != ANODE_SOCKET_OPEN)
        return ANODE_ERR_CONNECTION_CLOSED;

#ifdef ANODE_USE_SELECT
      result = send((int)(THIS->fds[((struct AnodeSystemTransport_AnodeSocket *)sock)->entry_idx]),data,data_len,0);
#else
      result = send(THIS->fds[((struct AnodeSystemTransport_AnodeSocket *)sock)->entry_idx].fd,data,data_len,0);
#endif

      if (result >= 0)
        return result;
      else {
        AnodeSystemTransport__do_close(THIS,(struct AnodeSystemTransport_AnodeSocket *)sock,ANODE_ERR_CONNECTION_CLOSED_BY_REMOTE,1);
        return ANODE_ERR_CONNECTION_CLOSED;
      }
    } else return THIS->base->stream_send(THIS->base,sock,data,data_len);
  } else return ANODE_ERR_INVALID_ARGUMENT;
}

static void AnodeSystemTransport_close(AnodeTransport *transport,
  AnodeSocket *sock)
{
  AnodeSystemTransport__do_close(THIS,(struct AnodeSystemTransport_AnodeSocket *)sock,0,1);
}

static void AnodeSystemTransport__poll_do_read_datagram(struct AnodeSystemTransport *transport,int fd,struct AnodeSystemTransport_AnodeSocket *sock)
{
  char buf[16384];
  struct sockaddr_storage fromaddr;
  AnodeNetworkEndpoint tmp_ep;
  AnodeEvent evbuf;
  socklen_t addrlen;
  int n;

  addrlen = sizeof(struct sockaddr_storage);
  n = recvfrom(fd,buf,sizeof(buf),0,(struct sockaddr *)&fromaddr,&addrlen);
  if ((n >= 0)&&(AnodeSystemTransport__populate_network_endpoint(&fromaddr,&tmp_ep))) {
    evbuf.type = ANODE_TRANSPORT_EVENT_DATAGRAM_RECEIVED;
    evbuf.transport = (AnodeTransport *)transport;
    evbuf.sock = (AnodeSocket *)sock;
    evbuf.datagram_from = &tmp_ep;
    evbuf.dns_name = NULL;
    evbuf.dns_addresses = NULL;
    evbuf.dns_address_count = 0;
    evbuf.error_code = 0;
    evbuf.data_length = n;
    evbuf.data = buf;

    if (sock->base.event_handler)
      sock->base.event_handler(&evbuf);
    else if (transport->default_event_handler)
      transport->default_event_handler(&evbuf);
  }
}

static void AnodeSystemTransport__poll_do_accept_incoming_connection(struct AnodeSystemTransport *transport,int fd,struct AnodeSystemTransport_AnodeSocket *sock)
{
  struct sockaddr_storage fromaddr;
  AnodeNetworkEndpoint tmp_ep;
  AnodeEvent evbuf;
  struct AnodeSystemTransport_AnodeSocket *newsock;
  socklen_t addrlen;
  int n;
  unsigned int entry_idx;

  addrlen = sizeof(struct sockaddr_storage);
  n = accept(fd,(struct sockaddr *)&fromaddr,&addrlen);
  if ((n >= 0)&&(AnodeSystemTransport__populate_network_endpoint(&fromaddr,&tmp_ep))) {
    entry_idx = AnodeSystemTransport__add_entry(transport);
    newsock = &(transport->sockets[entry_idx]);

    newsock->base.type = ANODE_SOCKET_STREAM_CONNECTION;
    newsock->base.state = ANODE_SOCKET_OPEN;
    Anode_memcpy(&newsock->base.endpoint,&tmp_ep,sizeof(AnodeNetworkEndpoint));
    newsock->base.class_name = AnodeSystemTransport_CLASS;
    newsock->base.user_ptr[0] = NULL;
    newsock->base.user_ptr[1] = NULL;
    newsock->base.event_handler = NULL;
    newsock->entry_idx = entry_idx;

    THIS->fds[entry_idx].fd = n;
    THIS->fds[entry_idx].events = POLLIN;
    THIS->fds[entry_idx].revents = 0;

    evbuf.type = ANODE_TRANSPORT_EVENT_STREAM_INCOMING_CONNECT;
    evbuf.transport = (AnodeTransport *)transport;
    evbuf.sock = (AnodeSocket *)newsock;
    evbuf.datagram_from = NULL;
    evbuf.dns_name = NULL;
    evbuf.dns_addresses = NULL;
    evbuf.dns_address_count = 0;
    evbuf.error_code = 0;
    evbuf.data_length = 0;
    evbuf.data = NULL;

    if (sock->base.event_handler)
      sock->base.event_handler(&evbuf);
    else if (transport->default_event_handler)
      transport->default_event_handler(&evbuf);
  }
}

static void AnodeSystemTransport__poll_do_read_stream(struct AnodeSystemTransport *transport,int fd,struct AnodeSystemTransport_AnodeSocket *sock)
{
  char buf[65536];
  AnodeEvent evbuf;
  int n;

  n = recv(fd,buf,sizeof(buf),0);
  if (n > 0) {
    evbuf.type = ANODE_TRANSPORT_EVENT_STREAM_DATA_RECEIVED;
    evbuf.transport = (AnodeTransport *)transport;
    evbuf.sock = (AnodeSocket *)sock;
    evbuf.datagram_from = NULL;
    evbuf.dns_name = NULL;
    evbuf.dns_addresses = NULL;
    evbuf.dns_address_count = 0;
    evbuf.error_code = 0;
    evbuf.data_length = n;
    evbuf.data = buf;

    if (sock->base.event_handler)
      sock->base.event_handler(&evbuf);
    else if (transport->default_event_handler)
      transport->default_event_handler(&evbuf);
  } else AnodeSystemTransport__do_close(transport,sock,ANODE_ERR_CONNECTION_CLOSED_BY_REMOTE,1);
}

static void AnodeSystemTransport__poll_do_stream_available_for_write(struct AnodeSystemTransport *transport,int fd,struct AnodeSystemTransport_AnodeSocket *sock)
{
  AnodeEvent evbuf;

  evbuf.type = ANODE_TRANSPORT_EVENT_STREAM_DATA_RECEIVED;
  evbuf.transport = (AnodeTransport *)transport;
  evbuf.sock = (AnodeSocket *)sock;
  evbuf.datagram_from = NULL;
  evbuf.dns_name = NULL;
  evbuf.dns_addresses = NULL;
  evbuf.dns_address_count = 0;
  evbuf.error_code = 0;
  evbuf.data_length = 0;
  evbuf.data = NULL;

  if (sock->base.event_handler)
    sock->base.event_handler(&evbuf);
  else if (transport->default_event_handler)
    transport->default_event_handler(&evbuf);
}

static void AnodeSystemTransport__poll_do_outgoing_connect(struct AnodeSystemTransport *transport,int fd,struct AnodeSystemTransport_AnodeSocket *sock)
{
  AnodeEvent evbuf;
  int err_code;
  socklen_t optlen;

  optlen = sizeof(err_code);
  if (getsockopt(fd,SOL_SOCKET,SO_ERROR,(void *)&err_code,&optlen)) {
    /* Error getting result, so we assume a failure */
    evbuf.type = ANODE_TRANSPORT_EVENT_STREAM_OUTGOING_CONNECT_FAILED;
    evbuf.transport = (AnodeTransport *)transport;
    evbuf.sock = (AnodeSocket *)sock;
    evbuf.datagram_from = NULL;
    evbuf.dns_name = NULL;
    evbuf.dns_addresses = NULL;
    evbuf.dns_address_count = 0;
    evbuf.error_code = ANODE_ERR_CONNECT_FAILED;
    evbuf.data_length = 0;
    evbuf.data = NULL;

    AnodeSystemTransport__do_close(transport,sock,0,0);
  } else if (err_code) {
    /* Error code is nonzero, so connect failed */
    evbuf.type = ANODE_TRANSPORT_EVENT_STREAM_OUTGOING_CONNECT_FAILED;
    evbuf.transport = (AnodeTransport *)transport;
    evbuf.sock = (AnodeSocket *)sock;
    evbuf.datagram_from = NULL;
    evbuf.dns_name = NULL;
    evbuf.dns_addresses = NULL;
    evbuf.dns_address_count = 0;
    evbuf.error_code = ANODE_ERR_CONNECT_FAILED;
    evbuf.data_length = 0;
    evbuf.data = NULL;

    AnodeSystemTransport__do_close(transport,sock,0,0);
  } else {
    /* Connect succeeded */
    evbuf.type = ANODE_TRANSPORT_EVENT_STREAM_OUTGOING_CONNECT_ESTABLISHED;
    evbuf.transport = (AnodeTransport *)transport;
    evbuf.sock = (AnodeSocket *)sock;
    evbuf.datagram_from = NULL;
    evbuf.dns_name = NULL;
    evbuf.dns_addresses = NULL;
    evbuf.dns_address_count = 0;
    evbuf.error_code = 0;
    evbuf.data_length = 0;
    evbuf.data = NULL;
  }

  if (sock->base.event_handler)
    sock->base.event_handler(&evbuf);
  else if (transport->default_event_handler)
    transport->default_event_handler(&evbuf);
}

static int AnodeSystemTransport_poll(AnodeTransport *transport)
{
  int timeout = -1;
  unsigned int fd_idx;
  int event_count = 0;
  int n;

  if (poll((struct pollfd *)THIS->fds,THIS->fd_count,timeout) > 0) {
    for(fd_idx=0;fd_idx<THIS->fd_count;++fd_idx) {
      if ((THIS->fds[fd_idx].revents & (POLLERR|POLLHUP|POLLNVAL))) {
        if (THIS->sockets[fd_idx].base.type == ANODE_SOCKET_STREAM_CONNECTION) {
          if (THIS->sockets[fd_idx].base.state == ANODE_SOCKET_CONNECTING)
            AnodeSystemTransport__poll_do_outgoing_connect(THIS,THIS->fds[fd_idx].fd,&THIS->sockets[fd_idx]);
          else AnodeSystemTransport__do_close(THIS,&THIS->sockets[fd_idx],ANODE_ERR_CONNECTION_CLOSED_BY_REMOTE,1);
          ++event_count;
        }
      } else {
        if ((THIS->fds[fd_idx].revents & POLLIN)) {
          if (THIS->fds[fd_idx].fd == THIS->invoke_pipe[0]) {
            n = read(THIS->invoke_pipe[0],&(((unsigned char *)(&(THIS->invoke_pipe_buf)))[THIS->invoke_pipe_buf_ptr]),sizeof(THIS->invoke_pipe_buf) - THIS->invoke_pipe_buf_ptr);
            if (n > 0) {
              THIS->invoke_pipe_buf_ptr += (unsigned int)n;
              if (THIS->invoke_pipe_buf_ptr >= sizeof(THIS->invoke_pipe_buf)) {
                THIS->invoke_pipe_buf_ptr -= sizeof(THIS->invoke_pipe_buf);
                ((void (*)(void *))(THIS->invoke_pipe_buf[1]))(THIS->invoke_pipe_buf[0]);
              }
            }
          } else {
            switch(THIS->sockets[fd_idx].base.type) {
              case ANODE_SOCKET_DATAGRAM:
                AnodeSystemTransport__poll_do_read_datagram(THIS,THIS->fds[fd_idx].fd,&THIS->sockets[fd_idx]);
                break;
              case ANODE_SOCKET_STREAM_LISTEN:
                AnodeSystemTransport__poll_do_accept_incoming_connection(THIS,THIS->fds[fd_idx].fd,&THIS->sockets[fd_idx]);
                break;
              case ANODE_SOCKET_STREAM_CONNECTION:
                if (THIS->sockets[fd_idx].base.state == ANODE_SOCKET_CONNECTING)
                  AnodeSystemTransport__poll_do_outgoing_connect(THIS,THIS->fds[fd_idx].fd,&THIS->sockets[fd_idx]);
                else AnodeSystemTransport__poll_do_read_stream(THIS,THIS->fds[fd_idx].fd,&THIS->sockets[fd_idx]);
                break;
            }
            ++event_count;
          }
        }

        if ((THIS->fds[fd_idx].revents & POLLOUT)) {
          if (THIS->sockets[fd_idx].base.state == ANODE_SOCKET_CONNECTING)
            AnodeSystemTransport__poll_do_outgoing_connect(THIS,THIS->fds[fd_idx].fd,&THIS->sockets[fd_idx]);
          else AnodeSystemTransport__poll_do_stream_available_for_write(THIS,THIS->fds[fd_idx].fd,&THIS->sockets[fd_idx]);
          ++event_count;
        }
      }
    }
  }

  return event_count;
}

static int AnodeSystemTransport_supports_address_type(const AnodeTransport *transport,
  enum AnodeNetworkAddressType at)
{
  switch(at) {
    case ANODE_NETWORK_ADDRESS_IPV4:
      return 1;
    case ANODE_NETWORK_ADDRESS_IPV6:
      return 1;
    default:
      if (THIS->base)
        return THIS->base->supports_address_type(THIS->base,at);
      return 0;
  }
}

static AnodeTransport *AnodeSystemTransport_base_instance(const AnodeTransport *transport)
{
  return THIS->base;
}

static const char *AnodeSystemTransport_class_name(AnodeTransport *transport)
{
  return AnodeSystemTransport_CLASS;
}

static void AnodeSystemTransport_delete(AnodeTransport *transport)
{
  close(THIS->invoke_pipe[0]);
  close(THIS->invoke_pipe[1]);

  AnodeMutex_destroy(&THIS->invoke_pipe_m);

  if (THIS->fds) free(THIS->fds);
  if (THIS->sockets) free(THIS->sockets);

  if (THIS->base) THIS->base->delete(THIS->base);

  free(transport);
}

/* ======================================================================== */

AnodeTransport *AnodeSystemTransport_new(AnodeTransport *base)
{
  struct AnodeSystemTransport *t;
  unsigned int entry_idx;

  t = malloc(sizeof(struct AnodeSystemTransport));
  if (!t) return (AnodeTransport *)0;
  Anode_zero(t,sizeof(struct AnodeSystemTransport));

  t->interface.invoke = &AnodeSystemTransport_invoke;
  t->interface.dns_resolve = &AnodeSystemTransport_dns_resolve;
  t->interface.datagram_listen = &AnodeSystemTransport_datagram_listen;
  t->interface.stream_listen = &AnodeSystemTransport_stream_listen;
  t->interface.datagram_send = &AnodeSystemTransport_datagram_send;
  t->interface.stream_connect = &AnodeSystemTransport_stream_connect;
  t->interface.stream_start_writing = &AnodeSystemTransport_stream_start_writing;
  t->interface.stream_stop_writing = &AnodeSystemTransport_stream_stop_writing;
  t->interface.stream_send = &AnodeSystemTransport_stream_send;
  t->interface.close = &AnodeSystemTransport_close;
  t->interface.poll = &AnodeSystemTransport_poll;
  t->interface.supports_address_type = &AnodeSystemTransport_supports_address_type;
  t->interface.base_instance = &AnodeSystemTransport_base_instance;
  t->interface.class_name = &AnodeSystemTransport_class_name;
  t->interface.delete = &AnodeSystemTransport_delete;

  t->base = base;

  pipe(t->invoke_pipe);
  fcntl(t->invoke_pipe[0],F_SETFL,O_NONBLOCK);
  entry_idx = AnodeSystemTransport__add_entry(t);
  t->fds[entry_idx].fd = t->invoke_pipe[0];
  t->fds[entry_idx].events = POLLIN;
  t->fds[entry_idx].revents = 0;
  AnodeMutex_init(&t->invoke_pipe_m);

  return (AnodeTransport *)t;
}
