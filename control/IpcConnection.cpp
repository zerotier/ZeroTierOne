/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2015  ZeroTier Networks
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
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/select.h>
#endif

namespace ZeroTier {

IpcConnection::IpcConnection(const char *endpoint,unsigned int timeout,void (*commandHandler)(void *,IpcConnection *,IpcConnection::EventType,const char *),void *arg) :
	_handler(commandHandler),
	_arg(arg),
	_timeout(timeout),
#ifdef __WINDOWS__
	_sock(INVALID_HANDLE_VALUE),
	_incoming(false),
#else
	_sock(-1),
#endif
	_run(true),
	_running(true)
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

	_thread = Thread::start(this);
}

#ifdef __WINDOWS__
IpcConnection::IpcConnection(HANDLE s,unsigned int timeout,void (*commandHandler)(void *,IpcConnection *,IpcConnection::EventType,const char *),void *arg) :
#else
IpcConnection::IpcConnection(int s,unsigned int timeout,void (*commandHandler)(void *,IpcConnection *,IpcConnection::EventType,const char *),void *arg) :
#endif
	_handler(commandHandler),
	_arg(arg),
	_timeout(timeout),
	_sock(s),
#ifdef __WINDOWS__
	_incoming(true),
#endif
	_run(true),
	_running(true)
{
	_thread = Thread::start(this);
}

IpcConnection::~IpcConnection()
{
	_writeLock.lock();
	_run = false;
	_writeLock.unlock();

#ifdef __WINDOWS__

	while (_running) {
		Thread::cancelIO(_thread); // cause Windows to break from blocking read and detect shutdown
		Sleep(100);
	}

#else // !__WINDOWS__

	int s = _sock;
	_sock = 0;
	if (s > 0) {
		::shutdown(s,SHUT_RDWR);
		::close(s);
	}
	Thread::join(_thread);

#endif // __WINDOWS__ / !__WINDOWS__
}

void IpcConnection::printf(const char *format,...)
{
	va_list ap;
	int n;
	char tmp[65536];

	va_start(ap,format);
	n = (int)::vsnprintf(tmp,sizeof(tmp),format,ap);
	va_end(ap);
	if (n <= 0)
		return;

	Mutex::Lock _l(_writeLock);

#ifdef __WINDOWS__
	_writeBuf.append(tmp,n);
	Thread::cancelIO(_thread); // cause Windows to break from blocking read and service write buffer
#else
	if (_sock > 0)
		::write(_sock,tmp,n);
#endif
}

void IpcConnection::threadMain()
	throw()
{
	char tmp[16384];
	char linebuf[16384];
	unsigned int lineptr = 0;
	char c;

#ifdef __WINDOWS__

	DWORD n,i;
	std::string wbuf;

#else // !__WINDOWS__

	int s,n,i;
	fd_set readfds,writefds,errorfds;
	struct timeval tout;

#ifdef SO_NOSIGPIPE
	if (_sock > 0) {
		i = 1;
		::setsockopt(_sock,SOL_SOCKET,SO_NOSIGPIPE,(char *)&i,sizeof(i));
	}
#endif // SO_NOSIGPIPE

#endif // __WINDOWS__ / !__WINDOWS__

	while (_run) {

#ifdef __WINDOWS__

		/* Note that we do not use fucking timeouts in Windows, since it does seem
		 * to properly detect named pipe endpoint close. But we do use a write buffer
		 * because Windows won't let you divorce reading and writing threads without
		 * all that OVERLAPPED cruft. */
		{
			Mutex::Lock _l(_writeLock);
			if (!_run)
				break;
			if (_writeBuf.length() > 0) {
				wbuf.append(_writeBuf);
				_writeBuf.clear();
			}
		}
		if (wbuf.length() > 0) {
			n = 0;
			if ((WriteFile(_sock,wbuf.data(),(DWORD)(wbuf.length()),&n,NULL))&&(n > 0)) {
				if (n < (DWORD)wbuf.length())
					wbuf.erase(0,n);
				else wbuf.clear();
			} else if (GetLastError() != ERROR_OPERATION_ABORTED)
				break;
			FlushFileBuffers(_sock);
		}
		if (!_run)
			break;
		n = 0;
		if ((!ReadFile(_sock,tmp,sizeof(tmp),&n,NULL))||(n <= 0)) {
			if (GetLastError() == ERROR_OPERATION_ABORTED)
				n = 0;
			else break;
		}
		if (!_run)
			break;

#else // !__WINDOWS__

		/* So today I learned that there is no reliable way to detect a half-closed
		 * Unix domain socket. So to make sure we don't leave orphaned sockets around
		 * we just use fucking timeouts. If a socket fucking times out, we break from
		 * the I/O loop and terminate the thread. But this IpcConnection code is ugly
		 * so maybe the OS is simply offended by it and refuses to reveal its mysteries
		 * to me. Oh well... this IPC code will probably get canned when we go to
		 * local HTTP RESTful interfaces or soemthing like that. */
		if ((s = _sock) <= 0)
			break;
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&errorfds);
		FD_SET(s,&readfds);
		FD_SET(s,&errorfds);
		tout.tv_sec = _timeout; // use a fucking timeout
		tout.tv_usec = 0;
		if (select(s+1,&readfds,&writefds,&errorfds,&tout) <= 0) {
			break; // socket has fucking timed out
		} else {
			if (FD_ISSET(s,&errorfds))
				break; // socket has an exception... sometimes works
			else {
				n = (int)::read(s,tmp,sizeof(tmp));
				if ((n <= 0)||(_sock <= 0))
					break; // read returned error... sometimes works
			}
		}

#endif // __WINDOWS__ / !__WINDOWS__

		for(i=0;i<n;++i) {
			c = (linebuf[lineptr] = tmp[i]);
			if ((c == '\r')||(c == '\n')||(c == (char)0)||(lineptr == (sizeof(linebuf) - 1))) {
				if (lineptr) {
					linebuf[lineptr] = (char)0;
					_handler(_arg,this,IPC_EVENT_COMMAND,linebuf);
					lineptr = 0;
				}
			} else ++lineptr;
		}
	}

	_writeLock.lock();
	bool r = _run;
	_writeLock.unlock();

#ifdef __WINDOWS__

	if (_incoming)
		DisconnectNamedPipe(_sock);
	CloseHandle(_sock);
	_running = false;

#endif // __WINDOWS__

	if (r)
		_handler(_arg,this,IPC_EVENT_CONNECTION_CLOSED,(const char *)0);
}

} // namespace ZeroTier
