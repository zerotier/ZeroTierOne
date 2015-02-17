/*
 * ZeroTier One - Network Virtualization Everywhere
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

#ifndef ZT_IPCLISTENER_HPP
#define ZT_IPCLISTENER_HPP

#include "../node/Constants.hpp"
#include "../node/Thread.hpp"
#include "../node/NonCopyable.hpp"
#include "IpcConnection.hpp"

#include <string>
#include <stdexcept>

namespace ZeroTier {

/**
 * IPC incoming connection listener (Unix domain sockets or named pipes on Windows)
 */
class IpcListener : NonCopyable
{
public:
	/**
	 * Listen for IPC connections
	 *
	 * The supplied handler is passed on to incoming instances of IpcConnection. When
	 * a connection is first opened, it is called with IPC_EVENT_NEW_CONNECTION. The
	 * receiver must take ownership of the connection object. When a connection is
	 * closed, IPC_EVENT_CONNECTION_CLOSED is generated. At this point (or after) the
	 * receiver must delete the object. IPC_EVENT_COMMAND is generated when lines of
	 * text are read, and in this cases the last argument is not NULL. No closed event
	 * is generated in the event of manual delete if the connection is still open.
	 *
	 * Yeah, this whole callback model sort of sucks. Might rethink and replace with
	 * some kind of actor model or something if it gets too unweildy. But for now the
	 * use cases are simple enough that it's not too bad.
	 *
	 * @param IPC endpoint name (OS-specific)
	 * @param timeout Endpoint inactivity timeout in seconds
	 * @param commandHandler Function to call for each command
	 * @param arg First argument to pass to handler
	 * @throws std::runtime_error Unable to bind to endpoint
	 */
	IpcListener(const char *ep,unsigned int timeout,void (*commandHandler)(void *,IpcConnection *,IpcConnection::EventType,const char *),void *arg);

	~IpcListener();

	void threadMain()
		throw();

private:
	std::string _endpoint;
	void (*_handler)(void *,IpcConnection *,IpcConnection::EventType,const char *);
	void *_arg;
	unsigned int _timeout;
#ifdef __WINDOWS__
	volatile bool _run;
	volatile bool _running;
#else
	volatile int _sock;
#endif
	Thread _thread;
};

} // namespace ZeroTier

#endif
