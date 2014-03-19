/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <stdexcept>

#include "IpcConnection.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace ZeroTier {

IpcConnection::IpcConnection(const char *endpoint,void (*commandHandler)(void *,const SharedPtr<IpcConnection> &,const char *),void *arg) :
	_handler(commandHandler),
	_arg(arg),
	_sock(0)
{
	struct sockaddr_un unaddr;
	unaddr.sun_family = AF_UNIX;
	strncpy(unaddr.sun_path,endpoint,sizeof(unaddr.sun_path));
	unaddr.sun_path[sizeof(unaddr.sun_path) - 1] = (char)0;

	_sock = socket(AF_UNIX,SOCK_STREAM,0);
	if (_sock <= 0)
		throw std::runtime_error("unable to create socket of type AF_UNIX");

	if (connect(_sock,(struct sockaddr *)&unaddr,sizeof(unaddr))) {
		::close(_sock);
		throw std::runtime_error("IPC endpoint unreachable");
	}

	Thread::start(this);
}

IpcConnection::IpcConnection(int s,void (*commandHandler)(void *,const SharedPtr<IpcConnection> &,const char *),void *arg) :
	_handler(commandHandler),
	_arg(arg),
	_sock(s)
{
	Thread::start(this);
}

IpcConnection::~IpcConnection()
{
	this->close();
}

void IpcConnection::printf(const char *format,...)
{
	va_list ap;
	int n;
	char tmp[65536];

	Mutex::Lock _l(_writeLock);

	if (_sock <= 0)
		return;

	va_start(ap,format);
	n = (int)::vsnprintf(tmp,sizeof(tmp),format,ap);
	va_end(ap);

	::write(_sock,tmp,n);
}

void IpcConnection::close()
{
	Mutex::Lock _l(_writeLock);
	int s = _sock;
	_sock = 0;
	if (s > 0) {
		::shutdown(s,SHUT_RDWR);
		::close(s);
	}
	Thread::join(_thread);
}

void IpcConnection::threadMain()
	throw()
{
	char tmp[65536];
	char linebuf[65536];
	unsigned int lineptr = 0;

	while (_sock) {
		int n = (int)::read(_sock,tmp,sizeof(tmp));
		if (n <= 0)
			break;
		for(int i=0;i<n;++i) {
			char c = (linebuf[lineptr] = tmp[i]);
			if ((c == '\r')||(c == '\n')||(lineptr == (sizeof(linebuf) - 1))) {
				if (lineptr) {
					linebuf[lineptr] = (char)0;
					_handler(_arg,SharedPtr<IpcConnection>(this),linebuf);
					lineptr = 0;
				}
			} else ++lineptr;
		}
	}
}

} // namespace ZeroTier
