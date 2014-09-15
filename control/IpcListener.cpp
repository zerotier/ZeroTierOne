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

#ifndef __WINDOWS__
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ZeroTier {

IpcListener::IpcListener(const char *ep,void (*commandHandler)(void *,IpcConnection *,IpcConnection::EventType,const char *),void *arg) :
	_endpoint(ep),
	_handler(commandHandler),
	_arg(arg),
#ifdef __WINDOWS__
	_run(true),
	_running(true)
#else
	_sock(0)
#endif
{
#ifndef __WINDOWS__
	struct sockaddr_un unaddr;
	unaddr.sun_family = AF_UNIX;
	strncpy(unaddr.sun_path,_endpoint.c_str(),sizeof(unaddr.sun_path));
	unaddr.sun_path[sizeof(unaddr.sun_path) - 1] = (char)0;

	struct stat stattmp;
	if (stat(_endpoint.c_str(),&stattmp)) {
		int testSock = socket(AF_UNIX,SOCK_STREAM,0);
		if (testSock <= 0)
			throw std::runtime_error("unable to create socket of type AF_UNIX");
		if (connect(testSock,(struct sockaddr *)&unaddr,sizeof(unaddr))) {
			// error means nothing is listening, orphaned name
			::close(testSock);
		} else {
			// success means endpoint is being actively listened to by a process
			::close(testSock);
			throw std::runtime_error("IPC endpoint address in use");
		}
	}
	::unlink(_endpoint.c_str());

	_sock = socket(AF_UNIX,SOCK_STREAM,0);
	if (_sock <= 0)
		throw std::runtime_error("unable to create socket of type AF_UNIX");
	if (bind(_sock,(struct sockaddr *)&unaddr,sizeof(unaddr))) {
		::close(_sock);
		throw std::runtime_error("IPC endpoint could not be bound");
	}
	if (listen(_sock,8)) {
		::close(_sock);
		throw std::runtime_error("listen() failed for bound AF_UNIX socket");
	}
	::chmod(_endpoint.c_str(),0777);
#endif

	_thread = Thread::start(this);
}

IpcListener::~IpcListener()
{
#ifdef __WINDOWS__
	_run = false;
	while (_running) {
		Thread::cancelIO(_thread);
		HANDLE tmp = CreateFileA(_endpoint.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
		if (tmp != INVALID_HANDLE_VALUE)
			CloseHandle(tmp);
		Sleep(250);
	}
#else
	int s = _sock;
	_sock = 0;
	if (s > 0) {
		::shutdown(s,SHUT_RDWR);
		::close(s);
	}
	Thread::join(_thread);
	::unlink(_endpoint.c_str());
#endif
}

void IpcListener::threadMain()
	throw()
{
#ifdef __WINDOWS__
	HANDLE s;
	while (_run) {
		s = CreateNamedPipeA(_endpoint.c_str(),PIPE_ACCESS_DUPLEX,PIPE_READMODE_BYTE|PIPE_TYPE_BYTE|PIPE_WAIT,PIPE_UNLIMITED_INSTANCES,1024,1024,0,NULL);
		if (s != INVALID_HANDLE_VALUE) {
			if ((ConnectNamedPipe(s,NULL))||(GetLastError() == ERROR_PIPE_CONNECTED)) {
				if (!_run) {
					DisconnectNamedPipe(s);
					CloseHandle(s);
					break;
				}
				try {
					_handler(_arg,new IpcConnection(s,_handler,_arg),IpcConnection::IPC_EVENT_NEW_CONNECTION,(const char *)0);
				} catch ( ... ) {} // handlers should not throw
			} else {
				CloseHandle(s);
			}
		}
	}
	_running = false;
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
			_handler(_arg,new IpcConnection(s,_handler,_arg),IpcConnection::IPC_EVENT_NEW_CONNECTION,(const char *)0);
		} catch ( ... ) {} // handlers should not throw
	}
#endif
}

} // namespace ZeroTier
