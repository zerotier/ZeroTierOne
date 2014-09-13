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

#ifndef ZT_IPCCONNECTION_HPP
#define ZT_IPCCONNECTION_HPP

#include "../node/Constants.hpp"
#include "../node/Thread.hpp"
#include "../node/NonCopyable.hpp"
#include "../node/Mutex.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#endif

namespace ZeroTier {

class IpcListener;

/**
 * Interprocess communication connection
 */
class IpcConnection : NonCopyable
{
	friend class IpcListener;

public:
	enum EventType
	{
		IPC_EVENT_COMMAND,
		IPC_EVENT_NEW_CONNECTION,
		IPC_EVENT_CONNECTION_CLOSED
	};

	/**
	 * Connect to an IPC endpoint
	 *
	 * @param endpoint Endpoint path
	 * @param commandHandler Command handler function
	 * @param arg First argument to command handler
	 * @throws std::runtime_error Unable to connect
	 */
	IpcConnection(const char *endpoint,void (*commandHandler)(void *,IpcConnection *,IpcConnection::EventType,const char *),void *arg);
	~IpcConnection();

	/**
	 * @param format Printf format string
	 * @param ... Printf arguments
	 */
	void printf(const char *format,...);

	void threadMain()
		throw();

private:
	// Used by IpcListener to construct incoming connections
#ifdef __WINDOWS__
	IpcConnection(HANDLE s,void (*commandHandler)(void *,IpcConnection *,IpcConnection::EventType,const char *),void *arg);
#else
	IpcConnection(int s,void (*commandHandler)(void *,IpcConnection *,IpcConnection::EventType,const char *),void *arg);
#endif

	void (*_handler)(void *,IpcConnection *,IpcConnection::EventType,const char *);
	void *_arg;
#ifdef __WINDOWS__
	HANDLE _sock;
	std::string _writeBuf;
	bool _incoming;
#else
	volatile int _sock;
#endif
	Mutex _writeLock;
	Thread _thread;
	volatile bool _run;
	volatile bool _running;
};

} // namespace ZeroTier

#endif
