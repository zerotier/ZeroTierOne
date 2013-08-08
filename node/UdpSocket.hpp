/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_UDPSOCKET_HPP
#define _ZT_UDPSOCKET_HPP

#include <stdexcept>
#include "Thread.hpp"
#include "InetAddress.hpp"
#include "Mutex.hpp"

namespace ZeroTier {

/**
 * A local UDP socket
 *
 * The socket listens in a background thread and sends packets to Switch.
 */
class UdpSocket
{
public:
	/**
	 * Create and bind a local UDP socket
	 *
	 * @param localOnly If true, bind to loopback address only
	 * @param localPort Local port to listen to
	 * @param ipv6 If true, bind this as an IPv6 socket, otherwise IPv4
	 * @param packetHandler Function to call when packets are read
	 * @param arg First argument (after self) to function
	 * @throws std::runtime_error Unable to bind
	 */
	UdpSocket(
		bool localOnly,
		int localPort,
		bool ipv6,
		void (*packetHandler)(UdpSocket *,void *,const InetAddress &,const void *,unsigned int),
		void *arg)
		throw(std::runtime_error);

	~UdpSocket();

	/**
	 * @return Locally bound port
	 */
	inline int localPort() const throw() { return _localPort; }

	/**
	 * @return True if this is an IPv6 socket
	 */
	inline bool v6() const throw() { return _v6; }

	/**
	 * Send a packet
	 *
	 * Attempt to send V6 on a V4 or V4 on a V6 socket will return false.
	 *
	 * @param to Destination IP/port
	 * @param data Data to send
	 * @param len Length of data in bytes
	 * @param hopLimit IP hop limit for UDP packet or -1 for max (max: 255)
	 * @return True if packet successfully sent to link layer
	 */
	bool send(const InetAddress &to,const void *data,unsigned int len,int hopLimit)
		throw();

	/**
	 * Thread main method; do not call elsewhere
	 */
	void threadMain()
		throw();

private:
	Thread _thread;
	void (*_packetHandler)(UdpSocket *,void *,const InetAddress &,const void *,unsigned int);
	void *_arg;
	int _localPort;
	volatile int _sock;
	bool _v6;
	Mutex _sendLock;
};

} // namespace ZeroTier

#endif
