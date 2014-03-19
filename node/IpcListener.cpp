/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014	ZeroTier Networks LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.	If not, see <http://www.gnu.org/licenses/>.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "IpcListener.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace ZeroTier {

IpcListener::IpcListener(const char *ep,void (*commandHandler)(void *,IpcConnection *,IpcConnection::EventType,const char *),void *arg) :
	_endpoint(ep),
	_handler(commandHandler),
	_arg(arg),
	_sock(0)
{
#ifdef __WINDOWS__
#else
	struct sockaddr_un unaddr;
	unaddr.sun_family = AF_UNIX;
	strncpy(unaddr.sun_path,_endpoint.c_str(),sizeof(unaddr.sun_path));
	unaddr.sun_path[sizeof(unaddr.sun_path) - 1] = (char)0;

	for(int tries=0;tries<3;++tries) {
		_sock = socket(AF_UNIX,SOCK_STREAM,0);
		if (_sock <= 0)
			throw std::runtime_error("unable to create socket of type AF_UNIX");
		if (bind(_sock,(struct sockaddr *)&unaddr,sizeof(unaddr))) {
			::close(_sock);
			if (errno == EADDRINUSE) {
				int testSock = socket(AF_UNIX,SOCK_STREAM,0);
				if (testSock <= 0)
					throw std::runtime_error("unable to create socket of type AF_UNIX");
				if (connect(testSock,(struct sockaddr *)&unaddr,sizeof(unaddr))) {
					// error indicates nothing is listening on other end, so unlink and try again
					::close(testSock);
					unlink(_endpoint.c_str());
				} else {
					// success means endpoint is being actively listened to by a process
					::close(testSock);
					throw std::runtime_error("IPC endpoint address in use");
				}
			} else throw std::runtime_error("IPC endpoint could not be bound");
		}
	}

	if (listen(_sock,8)) {
		::close(_sock);
		throw std::runtime_error("listen() failed for bound AF_UNIX socket");
	}
#endif

	_thread = Thread::start(this);
}

IpcListener::~IpcListener()
{
#ifdef __WINDOWS__
#else
	int s = _sock;
	_sock = 0;
	if (s > 0) {
		::shutdown(s,SHUT_RDWR);
		::close(s);
	}
	Thread::join(_thread);
	unlink(_endpoint.c_str());
#endif
}

void IpcListener::threadMain()
	throw()
{
#ifdef __WINDOWS__
#else
	struct sockaddr_un unaddr;
	socklen_t socklen;
	int s;
	while (_sock > 0) {
		unaddr.sun_family = AF_UNIX;
		strncpy(unaddr.sun_path,_endpoint.c_str(),sizeof(unaddr.sun_path));
		unaddr.sun_path[sizeof(unaddr.sun_path) - 1] = (char)0;
		socklen = sizeof(unaddr);
		s = accept(_sock,(struct sockaddr *)&unaddr,&socklen);
		if (s <= 0)
			break;
		if (!_sock) {
			::close(s);
			break;
		}
		try {
			_handler(_arg,new IpcConnection(s,_handler,_arg),Ipcconnection::IPC_EVENT_NEW_CONNECTION,(const char *)0);
		} catch ( ... ) {} // handlers should not throw
	}
#endif
}

} // namespace ZeroTier
