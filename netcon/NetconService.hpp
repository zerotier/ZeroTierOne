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

#include "../osdep/Phy.hpp"
#include "NetconEthernetTap.hpp"

#include "Intercept.h"
#include "LWIPStack.hpp"

#ifndef _NETCON_SERVICE_HPP
#define _NETCON_SERVICE_HPP

using namespace std;

namespace ZeroTier {

  class NetconEthernetTap;
  class TcpConnection;

  /*
   * TCP connection administered by service
   */
  class TcpConnection
  {
  public:
    int perceived_fd;
    int their_fd;
    bool pending;
    bool listening;
    int pid;

    unsigned long written;
    unsigned long acked;

    PhySocket *rpcSock;
    PhySocket *dataSock;
    struct tcp_pcb *pcb;

    unsigned char buf[DEFAULT_READ_BUFFER_SIZE];
    int idx;
  };

  /*
   * A helper class for passing a reference to _phy to LWIP callbacks as a "state"
   */
  class Larg
  {
  public:
    NetconEthernetTap *tap;
    TcpConnection *conn;
    Larg(NetconEthernetTap *_tap, TcpConnection *conn) : tap(_tap), conn(conn) {}
  };
} // namespace ZeroTier

#endif
