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
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>

#include "Constants.hpp"
#include "TcpSocket.hpp"
#include "SocketManager.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#endif

namespace ZeroTier {

TcpSocket::~TcpSocket()
{
#ifdef __WINDOWS__
	::closesocket(_sock);
#else
	::close(_sock);
#endif
	//printf("!!! TCP SOCKET DESTROYED @%.16llx to %s\r\n",(unsigned long long)this,_remote.toString().c_str());
}

bool TcpSocket::send(const InetAddress &to,const void *msg,unsigned int msglen)
{
	if (msglen > ZT_SOCKET_MAX_MESSAGE_LEN)
		return false; // message too big
	if (!msglen)
		return true; // sanity check

	Mutex::Lock _l(_writeLock);

	bool writeInProgress = ((_outptr != 0)||(_connecting));

	if ((_outptr + 5 + msglen) > (unsigned int)sizeof(_outbuf))
		return false;

	_outbuf[_outptr++] = 0x17; // look like TLS data
	_outbuf[_outptr++] = 0x03;
	_outbuf[_outptr++] = 0x03; // look like TLS 1.2
	_outbuf[_outptr++] = (unsigned char)((msglen >> 8) & 0xff);
	_outbuf[_outptr++] = (unsigned char)(msglen & 0xff);
	for(unsigned int i=0;i<msglen;++i)
		_outbuf[_outptr++] = ((const unsigned char *)msg)[i];

	if (!writeInProgress) {
		// If no output was enqueued before this, try to send() it and then
		// start a queued write if any remains after that.

		int n = (int)::send(_sock,(const char *)_outbuf,_outptr,0);
		if (n > 0)
			memmove(_outbuf,_outbuf + (unsigned int)n,_outptr -= (unsigned int)n);

		if (_outptr) {
			_sm->startNotifyWrite(this);
			_sm->whack();
		}
	} // else just leave in _outbuf[] to get written when stream is available for write

	return true;
}

bool TcpSocket::notifyAvailableForRead(const SharedPtr<Socket> &self,SocketManager *sm)
{
	unsigned char buf[65536];

	// will not be called concurrently since only SocketManager::poll() calls this

	int n = (int)::recv(_sock,(char *)buf,sizeof(buf),0);
	if (n <= 0)
		return false; // read error, stream probably closed

	unsigned int p = _inptr,pl = 0;
	for(int k=0;k<n;++k) {
		_inbuf[p++] = buf[k];
		if (p >= (int)sizeof(_inbuf))
			return false; // read overrun, packet too large or invalid

		if ((!pl)&&(p >= 5)) {
			if (_inbuf[0] == 0x17) {
				// fake TLS data frame, next two bytes are TLS version and are ignored
				pl = (((unsigned int)_inbuf[3] << 8) | (unsigned int)_inbuf[4]) + 5;
			} else return false; // in the future we may support fake TLS handshakes
		}

		if ((pl)&&(p >= pl)) {
			Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> data(_inbuf + 5,pl - 5);
			memmove(_inbuf,_inbuf + pl,p -= pl);
			try {
				sm->handleReceivedPacket(self,_remote,data);
			} catch ( ... ) {} // handlers should not throw
			pl = 0;
		}
	}
	_inptr = p;

	return true;
}

bool TcpSocket::notifyAvailableForWrite(const SharedPtr<Socket> &self,SocketManager *sm)
{
	Mutex::Lock _l(_writeLock);

	if (_connecting)
		_connecting = false;

	if (_outptr) {
		int n = (int)::send(_sock,(const char *)_outbuf,_outptr,0);
#ifdef __WINDOWS__
		if (n == SOCKET_ERROR) {
			switch(WSAGetLastError()) {
				case WSAEINTR:
				case WSAEWOULDBLOCK:
					break;
				default:
					return false;
			}
#else
		if (n <= 0) {
			switch(errno) {
#ifdef EAGAIN
				case EAGAIN:
#endif
#if defined(EWOULDBLOCK) && ( !defined(EAGAIN) || (EWOULDBLOCK != EAGAIN) )
				case EWOULDBLOCK:
#endif
#ifdef EINTR
				case EINTR:
#endif
					break;
				default:
					return false;
			}
#endif
		} else memmove(_outbuf,_outbuf + (unsigned int)n,_outptr -= (unsigned int)n);
	}

	if (!_outptr)
		sm->stopNotifyWrite(this);

	return true;
}

} // namespace ZeroTier
