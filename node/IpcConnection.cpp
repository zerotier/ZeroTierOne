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

#ifndef __WINDOWS__
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace ZeroTier {

IpcConnection::IpcConnection(const char *endpoint,void (*commandHandler)(void *,IpcConnection *,IpcConnection::EventType,const char *),void *arg) :
	_handler(commandHandler),
	_arg(arg),
#ifdef __WINDOWS__
	_sock(INVALID_HANDLE_VALUE)
#else
	_sock(0)
#endif
{
#ifdef __WINDOWS__
	_sock = CreateFileA(endpoint,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
	if (_sock == INVALID_HANDLE_VALUE)
		throw std::runtime_error("IPC endpoint unreachable");
	DWORD pipeMode = PIPE_READMODE_BYTE;
	SetNamedPipeHandleState(_sock,&pipeMode,NULL,NULL);
#else
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
#endif

	Thread::start(this);
}

#ifdef __WINDOWS__
IpcConnection::IpcConnection(HANDLE s,void (*commandHandler)(void *,IpcConnection *,IpcConnection::EventType,const char *),void *arg) :
#else
IpcConnection::IpcConnection(int s,void (*commandHandler)(void *,IpcConnection *,IpcConnection::EventType,const char *),void *arg) :
#endif
	_handler(commandHandler),
	_arg(arg),
	_sock(s)
{
	Thread::start(this);
}

IpcConnection::~IpcConnection()
{
	_writeLock.lock();
#ifdef __WINDOWS__
	HANDLE s = _sock;
	_sock = INVALID_HANDLE_VALUE;
	if (s != INVALID_HANDLE_VALUE) {
		CloseHandle(s);
	}
#else
	int s = _sock;
	_sock = 0;
	if (s > 0) {
		::shutdown(s,SHUT_RDWR);
		::close(s);
	}
#endif
	_writeLock.unlock();
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
	if (n <= 0)
		return;

#ifdef __WINDOWS__
	DWORD bsent = 0;
	WriteFile(_sock,tmp,n,&bsent,NULL);
#else
	::write(_sock,tmp,n);
#endif
}

void IpcConnection::threadMain()
	throw()
{
	char tmp[65536];
	char linebuf[65536];
	unsigned int lineptr = 0;
#ifdef __WINDOWS__
	HANDLE s;
	DWORD n,i;
#else
	int s,n,i;
#endif
	char c;

	for(;;) {
#ifdef __WINDOWS__
		s = _sock;
		if (s == INVALID_HANDLE_VALUE)
			break;
		if (!ReadFile(s,tmp,sizeof(tmp),&n,NULL))
			break;
		if (n < 0)
			break;
#else
		s = _sock;
		if (s <= 0)
			break;
		n = (int)::read(s,tmp,sizeof(tmp));
		if (n <= 0)
			break;
#endif
		for(i=0;i<n;++i) {
			c = (linebuf[lineptr] = tmp[i]);
			if ((c == '\r')||(c == '\n')||(lineptr == (sizeof(linebuf) - 1))) {
				if (lineptr) {
					linebuf[lineptr] = (char)0;
					_handler(_arg,this,IPC_EVENT_COMMAND,linebuf);
					lineptr = 0;
				}
			} else ++lineptr;
		}
	}

	{
		_writeLock.lock();
		s = _sock;
#ifdef __WINDOWS__
		_sock = INVALID_HANDLE_VALUE;
		if (s != INVALID_HANDLE_VALUE)
			CloseHandle(s);
#else
		_sock = 0;
		if (s > 0)
			::close(s);
#endif
		_writeLock.unlock();
	}

	_handler(_arg,this,IPC_EVENT_CONNECTION_CLOSED,(const char *)0);
}

} // namespace ZeroTier
