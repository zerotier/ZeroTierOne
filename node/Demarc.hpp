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

#ifndef _ZT_DEMARC_HPP
#define _ZT_DEMARC_HPP

#include <stdlib.h>
#include <stdint.h>
#include <map>
#include <string>
#include "Mutex.hpp"
#include "InetAddress.hpp"

namespace ZeroTier {

class RuntimeEnvironment;
class UdpSocket;

/**
 * Local demarcation point
 *
 * This holds and provides unique identifiers for all local communication
 * endpoints, such as UDP sockets, raw Ethernet sockets, tunnels to a relay
 * server, etc. It permits other code to refer to these via Port and forget
 * about what they actually are.
 *
 * All ports are closed when this class is destroyed.
 *
 * Its name "demarcation point" comes from the telco/cable terminology for
 * the box where wires terminate at a customer's property.
 */
class Demarc
{
public:
	/**
	 * Local demarcation port
	 */
	typedef uint64_t Port;

	/**
	 * Port identifier used to refer to any port
	 */
	static const Port ANY_PORT;

	/**
	 * Port identifier used to refer to null port / port not found
	 */
	static const Port NULL_PORT;

	Demarc(const RuntimeEnvironment *renv);
	~Demarc();

	/**
	 * Describe a port
	 *
	 * This can describe even ports that are not bound, e.g. from serialized
	 * data.
	 *
	 * @param p Port
	 * @return Human-readable description of port
	 */
	static std::string describe(Port p);

	/**
	 * @param p Port to check
	 * @return True if this port is bound/connected/etc.
	 */
	bool has(Port p) const
		throw();

	/**
	 * Bind local UDP port for both IPv4 and IPv6 traffic
	 *
	 * @param localPort Local IP port
	 * @return True if successfully bound, or if already bound
	 */
	bool bindLocalUdp(unsigned int localPort)
		throw();

	/**
	 * Pick a port to send to an address of a given type
	 *
	 * @param to Destination address
	 * @return Port or NULL_PORT if none
	 */
	Port pick(const InetAddress &to) const
		throw();

	/**
	 * Send a packet
	 *
	 * If fromPort is ANY_PORT or if the port is not found, a random port is
	 * chosen from those available matching the characteristics of the address
	 * in 'to'.
	 *
	 * @param fromPort Port to send from
	 * @param to Destination IP/port
	 * @param data Data to send
	 * @param len Length of data in bytes
	 * @param hopLimit IP hop limit for UDP packets or -1 for max/unlimited
	 * @return Port actually sent from or NULL_PORT on failure
	 */
	Port send(Port fromPort,const InetAddress &to,const void *data,unsigned int len,int hopLimit) const
		throw();

	/**
	 * @param p Port
	 * @return 64-bit integer suitable for serialization
	 */
	static inline uint64_t portToInt(const Port p) throw() { return (uint64_t)p; }

	/**
	 * @param p 64-bit integer from serialized representation
	 * @return Port suitable for use in code
	 */
	static inline Port intToPort(const uint64_t p) throw() { return (Port)p; }

private:
	const RuntimeEnvironment *_r;

	static void _CBudpSocketPacketHandler(UdpSocket *sock,void *arg,const InetAddress &from,const void *data,unsigned int len);

	enum DemarcPortType
	{
		PORT_TYPE_UDP_SOCKET_V4 = 1,
		PORT_TYPE_UDP_SOCKET_V6 = 2,
		PORT_TYPE_LOCAL_ETHERNET = 3,
		PORT_TYPE_RELAY_TUNNEL = 4
	};

	// Variant holding instances of UdpSocket, etc.
	struct DemarcPortObj
	{
		Demarc::Port port;
		Demarc *parent;
		void *obj;
		DemarcPortType type;
	};

	std::map< Port,DemarcPortObj > _ports;
	Mutex _ports_m;
};

} // namespace ZeroTier

#endif
