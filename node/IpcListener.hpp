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

#ifndef ZT_IPCLISTENER_HPP
#define ZT_IPCLISTENER_HPP

#include "Constants.hpp"
#include "Thread.hpp"
#include "NonCopyable.hpp"
#include "SharedPtr.hpp"

#include <string>
#include <stdexcept>

#ifdef __WINDOWS__
#define ZT_IPC_ENDPOINT "\\\\.\\pipe\\ZeroTierOne-control"
#else
#define ZT_IPC_ENDPOINT "/tmp/.ZeroTierOne-control"
#endif

namespace ZeroTier {

class IpcConnection;

/**
 * IPC incoming connection listener (Unix domain sockets or named pipes on Windows)
 */
class IpcListener : NonCopyable
{
public:
	/**
	 * @param commandHandler Function to call for each command
	 * @param arg First argument to pass to handler
	 * @throws std::runtime_error Unable to bind to endpoint
	 */
	IpcListener(cosnt char *ep,void (*commandHandler)(void *,const SharedPtr<IpcConnection> &,const char *),void *arg);

	~IpcListener();

	void threadMain()
		throw();

private:
	std::string _endpoint;
	void (*_handler)(void *,const SharedPtr<IpcConnection> &,const char *);
	void *_arg;
	volatile int _sock;
	Thread _thread;
};

} // namespace ZeroTier

#endif
