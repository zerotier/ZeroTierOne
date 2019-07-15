/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_MTUDPSOCKET_HPP
#define ZT_MTUDPSOCKET_HPP

#ifndef __WINDOWS__

#include "../node/Constants.hpp"
#include "../include/ZeroTierOne.h"
#include "../osdep/OSUtils.hpp"
#include "../osdep/Thread.hpp"

#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ZeroTier {

/**
 * MTUDPSocket is a multithreaded UDP socket using multiple binds and SO_REUSEPORT
 * 
 * On Mac and Linux this is the most efficient way to implement a multithreaded UDP
 * I/O path. On Windows it's probably not necessary to optimize this much. If it ever
 * is, we will have to implement a version of this the Windows way.
 */
class MTUDPSocket
{
public:
	inline MTUDPSocket(ZT_Node *n,volatile int64_t *dptr,const struct sockaddr *bindAddr)
	{
		const int ncores = std::max(1,(int)sysconf(_SC_NPROCESSORS_CONF));
		for(int t=0;t<ncores;t++) {
			int s = socket(bindAddr->sa_family,SOCK_DGRAM,0);
			if (s < 0) {
				for(auto i=_sockets.begin();i!=_sockets.end();++i)
					close(*i);
				throw std::runtime_error("unable to allocate socket");
			}

			int f = 131072;
			setsockopt(s,SOL_SOCKET,SO_RCVBUF,(const char *)&f,sizeof(f));
			f = 131072;
			setsockopt(s,SOL_SOCKET,SO_SNDBUF,(const char *)&f,sizeof(f));

			if (bindAddr->sa_family == AF_INET6) {
				f = 1; setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&f,sizeof(f));
#ifdef IPV6_MTU_DISCOVER
				f = 0; setsockopt(s,IPPROTO_IPV6,IPV6_MTU_DISCOVER,&f,sizeof(f));
#endif
#ifdef IPV6_DONTFRAG
				f = 0; setsockopt(s,IPPROTO_IPV6,IPV6_DONTFRAG,&f,sizeof(f));
#endif
			}
			f = 1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
			f = 1; setsockopt(s,SOL_SOCKET,SO_REUSEPORT,(void *)&f,sizeof(f));
			f = 1; setsockopt(s,SOL_SOCKET,SO_BROADCAST,(void *)&f,sizeof(f));
#ifdef IP_DONTFRAG
			f = 0; setsockopt(s,IPPROTO_IP,IP_DONTFRAG,&f,sizeof(f));
#endif
#ifdef IP_MTU_DISCOVER
			f = 0; setsockopt(s,IPPROTO_IP,IP_MTU_DISCOVER,&f,sizeof(f));
#endif
#ifdef SO_NO_CHECK
			if (bindAddr->sa_family == AF_INET) {
				f = 1; setsockopt(s,SOL_SOCKET,SO_NO_CHECK,(void *)&f,sizeof(f));
			}
#endif

			if (bind(s,bindAddr,(bindAddr->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))) {
				for(auto i=_sockets.begin();i!=_sockets.end();++i)
					close(*i);
				throw std::runtime_error("unable to bind to address");
			}

			_sockets.push_back(s);
		}

		for(auto s=_sockets.begin();s!=_sockets.end();++s) {
			try {
				new MTUDPThread(n,dptr,*s);
			} catch ( ... ) {
				for(auto i=_sockets.begin();i!=_sockets.end();++i)
					close(*i);
				throw;
			}
		}
	}

	inline ~MTUDPSocket()
	{
		for(auto i=_sockets.begin();i!=_sockets.end();++i)
			close(*i);
	}

private:
	class MTUDPThread
	{
	public:
		inline MTUDPThread(ZT_Node *n,volatile int64_t *dptr,int s) :
			node(n),
			deadlinePtr(dptr),
			sock(s),
			thread(Thread::start(this))
		{
		}

		inline void threadMain()
		{
			struct sockaddr_storage from;
			for(;;) {
				socklen_t fromLen = sizeof(from);
				const int nr = recvfrom(this->sock,this->buf,sizeof(this->buf),0,(struct sockaddr *)&from,&fromLen);
				if (nr > 0) {
					ZT_Node_processWirePacket(this->node,nullptr,OSUtils::now(),(int64_t)this->sock,&from,this->buf,(unsigned int)nr,this->deadlinePtr);
				} else {
					close(this->sock);
					break;
				}
			}
			delete this; // closing the socket causes this to exit and delete itself
		}

		ZT_Node *const node;
		volatile int64_t *const deadlinePtr;
		const int sock;
		Thread thread;
		char buf[10000];
	};

	std::vector<int> _sockets;
};

} // namespace ZeroTier

#endif // !__WINDOWS__

#endif
