/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <sys/poll.h>
#include <string>
#include "Intercept.h"
#include "LWIPStack.hpp"

#ifndef _NETCON_SERVICE_H_
#define _NETCON_SERVICE_H_

using namespace std;

enum NetconSocketType { RPC, BUFFER };

class NetconIntercept;
class NetconConnection;
class NetconSocket;

class NetconSocket{
  int fd;
  NetconSocketType type;
};


class NetconConnection
{
public:

  int tid;
  int our_fd;
  int their_fd; /* what the user app sees -- and what they will send us */
  struct tcp_pcb *pcb; /* for handling lwIP calls/data */
  bool unacked;

  unsigned char *write_buf; /* we need this reference so we can grab data from the buffer during a lwip-poll callback */
  long unsigned int write_count;
  long unsigned int write_total;

  unsigned char buf[DEFAULT_READ_BUFFER_SIZE];
  int idx;

  unsigned long data_acked;
  unsigned long data_sent;
  unsigned long data_read;
  unsigned long data_recvd;

  NetconIntercept* owner;

  NetconConnection(NetconIntercept* owner,
    int tid,
    int our_fd,
    int their_fd,
    struct tcp_pcb *pcb)
    :
      write_total(0),
      write_count(0),
      write_buf(NULL),
      owner(owner),
      tid(tid),
      our_fd(our_fd),
      their_fd(their_fd),
      pcb(pcb)
    {}
};

class NetconIntercept
{
public:

  // State (this needs to be evaluated)
  NetconConnection *unmapped_conn;
  bool waiting_for_retval;

  // Connections created for this intercept
  vector<NetconConnection*> owned_connections;

  int tid; /* just for uniqueness */
  int rpc;

  NetconIntercept(int tid, int rpc)
  :
    waiting_for_retval(false),
    unmapped_conn(NULL),
    tid(tid),
    rpc(rpc)
  {}
};


#define POLL_SZ ZT_PHY_MAX_SOCKETS+(ZT_PHY_MAX_INTERCEPTS*2)+2
class NetconService
{
public:

  LWIPStack *ls;

  // TODO: shall replace with map
  vector<NetconIntercept*> intercepts;
  vector<NetconConnection*> connections;

  /* fd_sets for main I/O polling */
  fd_set fdset;
  fd_set exfdset;
  int maxfd;
  size_t sz;
  int tapfd;

  // Sets of file descriptors we will poll() on
  int default_rpc_pipe;
  struct pollfd allfds[POLL_SZ];
  int alltypes[POLL_SZ];

  /* cached fd_sets */
  bool possible_state_change;
  fd_set cached_fdset;
  fd_set cached_exfdset;
  int cached_alltypes[POLL_SZ];
  int cached_maxfd;
  int cached_sz;

  NetconService(LWIPStack *ls, string _handle)
  :
    ls(ls),
    maxfd(0),
    sz(0),
    possible_state_change(true)
  {}

  /* Assemble single poll list */
  void assemble_fd_sets()
  {
    sz = 2 + connections.size() + intercepts.size();
    // initialize
    for(size_t i=0; i<sz; i++){
      allfds[i].fd = 0;
      allfds[i].events = 0;
    }
    int idx = 0;
    // default rpc fd
    allfds[0].fd = default_rpc_pipe;
    allfds[0].events = POLLIN;
    alltypes[0] = 1;
    // netif fd
    allfds[1].fd=tapfd;
    allfds[1].events = POLLIN;
    alltypes[1] = 2;
    // buffers
    for(size_t i=0; i<connections.size(); i++) {
      idx = i + 2;
      allfds[idx].fd = connections[i]->our_fd;
      allfds[idx].events = POLLIN;
      alltypes[idx] = 3;
    }
    // established connections
    for(size_t i=0; i<intercepts.size(); i++) {
      idx = i + connections.size() + 2;
      allfds[idx].fd = intercepts[i]->rpc;
      allfds[idx].events = POLLIN;
      alltypes[idx] = 4;
    }
    FD_ZERO(&fdset);
    FD_ZERO(&exfdset);
    // populate master fd_set
    for(size_t i=0; i<sz; i++) {
      FD_SET(allfds[i].fd, &fdset);
      FD_SET(allfds[i].fd, &exfdset);
    }
    // get maxfd
    for(size_t i=0; i<sz; i++)
    {
      if(allfds[i].fd > maxfd)
        maxfd = allfds[i].fd;
    }
    // cache copy of valid fd_set
    possible_state_change = false;
    memcpy(&cached_fdset, &fdset, sizeof(fdset));
    memcpy(&cached_exfdset, &exfdset, sizeof(exfdset));
    memcpy(&cached_alltypes, &alltypes, sizeof(alltypes));
    cached_maxfd = maxfd;
    cached_sz = sz;
  }


  NetconConnection *get_connection_by_pcb(struct tcp_pcb *pcb) {
    for(size_t i=0; i<connections.size(); i++) {
      if(connections[i]->pcb == pcb) {
        return connections[i];
      }
    }
    return NULL;
  }

  NetconConnection *get_connection_by_buf_sock(int fd) {
    for(size_t i=0; i<connections.size(); i++) {
      if(connections[i]->our_fd==fd) {
        return connections[i];
      }
    }
    return NULL;
  }

  // FIXME: This will be a common operation and thus should be done via
  // some sort of hashing, not iterative lookup.
  NetconIntercept *get_intercept_by_pcb(struct tcp_pcb* pcb) {
    for(size_t i=0; i<connections.size(); i++) {
      if(connections[i]->pcb==pcb) {
        return connections[i]->owner;
      }
    }
    return NULL;
  }


  NetconIntercept *get_intercept_by_rpc(int af) {
    for(size_t i=0; i<intercepts.size(); i++) {
      if(intercepts[i]->rpc==af) {
        return intercepts[i];
      }
    }
    return NULL;
  }

  NetconConnection *get_connection_by_that_fd(NetconIntercept* h, int fd)
  {
    for(size_t i=0; i<h->owned_connections.size(); i++) {
      if(h->owned_connections[i]->their_fd==fd) {
        return h->owned_connections[i];
      }
    }
    return NULL;
  }

  NetconConnection *get_connection_by_this_fd(int fd)
  {
    for(size_t i=0; i<connections.size(); i++) {
      if(connections[i]->our_fd==fd) {
        return connections[i];
      }
    }
    return NULL;
  }

  NetconConnection *get_connection_by_that_fd(int fd)
  {
    for(size_t i=0; i<connections.size(); i++) {
      if(connections[i]->their_fd==fd) {
        return connections[i];
      }
    }
    return NULL;
  }

  NetconConnection *add_connection(NetconIntercept* owner,
      int tid,
      int our_fd,
      int their_fd,
      struct tcp_pcb* pcb)
  {
    possible_state_change = true;
    if(connections.size() >= ZT_PHY_MAX_SOCKETS) {
      return NULL;
    }
    NetconConnection *new_conn = new NetconConnection(owner, tid, our_fd, their_fd, pcb);
    connections.push_back(new_conn);

    NetconIntercept *h;
    for(size_t i=0; i<intercepts.size(); i++) {
      if(intercepts[i]->tid == tid) {
        h = intercepts[i];
      }
    }
    if(h)
      h->owned_connections.push_back(new_conn);
    return new_conn;
  }

  // Removes a Connection from the Service and updates poll lists
  void remove_connection(NetconConnection *c)
  {
    possible_state_change = true;
    for(size_t i=0; i<connections.size(); i++) {
      if(c == connections[i]) {
        close(connections[i]->our_fd);
        ls->tcp_close(c->pcb);
        delete c;
        connections.erase(connections.begin() + i);
      }
    }
  }

  int add_intercept(int listen_sock) {
    possible_state_change = true;
    int accept_socket = accept(listen_sock, NULL, NULL);
    intercepts.push_back(new NetconIntercept(999, accept_socket));
    return accept_socket;
  }


  // Removes an Intercept from the Service
  bool remove_intercept(NetconIntercept *h)
  {
    possible_state_change = true;
    // remove all connections owned by this Intercept
    for(size_t i=0; i<h->owned_connections.size(); i++) {
      remove_connection(h->owned_connections[i]);
    }
    // find and remove Intercept
    for(size_t i=0; i<intercepts.size(); i++)
    {
      if(h == intercepts[i]) {
        delete intercepts[i];
        intercepts.erase(intercepts.begin() + i);
        return true;
      }
    }
    return false;
  }
};

#endif
