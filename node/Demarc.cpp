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

#include <vector>

#include "Constants.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#endif

#include "Demarc.hpp"
#include "RuntimeEnvironment.hpp"
#include "Logger.hpp"
#include "UdpSocket.hpp"
#include "InetAddress.hpp"
#include "Switch.hpp"
#include "Buffer.hpp"

namespace ZeroTier {

const Demarc::Port Demarc::ANY_PORT;
const Demarc::Port Demarc::NULL_PORT;

Demarc::Demarc(const RuntimeEnvironment *renv) :
	_r(renv)
{
}

Demarc::~Demarc()
{
	for(std::map< Port,DemarcPortObj >::iterator pe(_ports.begin());pe!=_ports.end();++pe) {
		switch (pe->second.type) {
			case PORT_TYPE_UDP_SOCKET_V4:
			case PORT_TYPE_UDP_SOCKET_V6:
				delete ((UdpSocket *)pe->second.obj);
				break;
			case PORT_TYPE_LOCAL_ETHERNET:
			case PORT_TYPE_RELAY_TUNNEL:
				break;
		}
	}
}

std::string Demarc::describe(Demarc::Port p)
	throw()
{
	char buf[64];
	switch ((DemarcPortType)(((uint64_t)p) >> 60)) {
		case PORT_TYPE_UDP_SOCKET_V4:
			sprintf(buf,"udp/4/%d",(int)((uint64_t)p & 0xffff));
			return std::string(buf);
		case PORT_TYPE_UDP_SOCKET_V6:
			sprintf(buf,"udp/6/%d",(int)((uint64_t)p & 0xffff));
			return std::string(buf);
		case PORT_TYPE_LOCAL_ETHERNET:
			return std::string("ethernet");
		case PORT_TYPE_RELAY_TUNNEL:
			return std::string("relay");
	}
	return std::string("(null)");
}

bool Demarc::has(Port p) const
	throw()
{
	Mutex::Lock _l(_ports_m);
	return (_ports.count(p) != 0);
}

bool Demarc::bindLocalUdp(unsigned int localPort)
	throw()
{
	Mutex::Lock _l(_ports_m);

	uint64_t v4p = ((uint64_t)PORT_TYPE_UDP_SOCKET_V4 << 60) | (uint64_t)localPort;
	uint64_t v6p = ((uint64_t)PORT_TYPE_UDP_SOCKET_V6 << 60) | (uint64_t)localPort;
	if ((_ports.count((Port)v4p))||(_ports.count((Port)v6p)))
		return true; // port already bound

	UdpSocket *v4 = (UdpSocket *)0;
	try {
		DemarcPortObj *v4r = &(_ports[(Port)v4p]);
		v4r->port = (Port)v4p;
		v4r->parent = this;
		v4r->obj = v4 = new UdpSocket(false,localPort,false,&Demarc::_CBudpSocketPacketHandler,v4r);
		v4r->type = PORT_TYPE_UDP_SOCKET_V4;
	} catch ( ... ) {
		_ports.erase((Port)v4p);
		v4 = (UdpSocket *)0;
	}

	UdpSocket *v6 = (UdpSocket *)0;
	try {
		DemarcPortObj *v6r = &(_ports[(Port)v6p]);
		v6r->port = (Port)v6p;
		v6r->parent = this;
		v6r->obj = v6 = new UdpSocket(false,localPort,true,&Demarc::_CBudpSocketPacketHandler,v6r);
		v6r->type = PORT_TYPE_UDP_SOCKET_V6;
	} catch ( ... ) {
		_ports.erase((Port)v6p);
		v6 = (UdpSocket *)0;
	}

	return ((v4)||(v6));
}

Demarc::Port Demarc::pick(const InetAddress &to) const
	throw()
{
	Mutex::Lock _l(_ports_m);
	try {
		std::vector< std::map< Port,DemarcPortObj >::const_iterator > possibilities;
		for(std::map< Port,DemarcPortObj >::const_iterator pe(_ports.begin());pe!=_ports.end();++pe) {
			switch (pe->second.type) {
				case PORT_TYPE_UDP_SOCKET_V4:
					if (to.isV4())
						possibilities.push_back(pe);
					break;
				case PORT_TYPE_UDP_SOCKET_V6:
					if (to.isV6())
						possibilities.push_back(pe);
					break;
				default:
					break;
			}
		}
		if (possibilities.size())
			return possibilities[_r->prng->next32() % possibilities.size()]->first;
		else return NULL_PORT;
	} catch ( ... ) {
		return NULL_PORT;
	}
}

Demarc::Port Demarc::send(Demarc::Port fromPort,const InetAddress &to,const void *data,unsigned int len,int hopLimit) const
	throw()
{
	_ports_m.lock();

	std::map< Port,DemarcPortObj >::const_iterator pe(_ports.find(fromPort));
	if (pe == _ports.end()) {
		try {
			std::vector< std::map< Port,DemarcPortObj >::const_iterator > possibilities;
			for(pe=_ports.begin();pe!=_ports.end();++pe) {
				switch (pe->second.type) {
					case PORT_TYPE_UDP_SOCKET_V4:
						if (to.isV4())
							possibilities.push_back(pe);
						break;
					case PORT_TYPE_UDP_SOCKET_V6:
						if (to.isV6())
							possibilities.push_back(pe);
						break;
					default:
						break;
				}
			}
			if (possibilities.size())
				pe = possibilities[_r->prng->next32() % possibilities.size()];
			else {
				_ports_m.unlock();
				return NULL_PORT;
			}
		} catch ( ... ) {
			_ports_m.unlock();
			return NULL_PORT;
		}
	}

	switch (pe->second.type) {
		case PORT_TYPE_UDP_SOCKET_V4:
		case PORT_TYPE_UDP_SOCKET_V6:
			_ports_m.unlock();
			if (((UdpSocket *)pe->second.obj)->send(to,data,len,hopLimit))
				return pe->first;
			return NULL_PORT;
		default:
			break;
	}

	_ports_m.unlock();
	return NULL_PORT;
}

void Demarc::_CBudpSocketPacketHandler(UdpSocket *sock,void *arg,const InetAddress &from,const void *data,unsigned int len)
{
	((DemarcPortObj *)arg)->parent->_r->sw->onRemotePacket(((DemarcPortObj *)arg)->port,from,Buffer<4096>(data,len));
}

} // namespace ZeroTier
