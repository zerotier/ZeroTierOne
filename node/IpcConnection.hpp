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

#include "Constants.hpp"
#include "Thread.hpp"
#include "NonCopyable.hpp"
#include "Mutex.hpp"
#include "SharedPtr.hpp"
#include "AtomicCounter.hpp"

namespace ZeroTier {

class IpcListener;

/**
 * Interprocess communication connection
 */
class IpcConnection : NonCopyable
{
	friend class IpcListener;
	friend class SharedPtr<IpcConnection>;

public:
	IpcConnection(const char *endpoint);
	~IpcConnection();

	void writeln(const char *format,...);

	void close();

	void threadMain()
		throw();

private:
	// Used by IpcListener to construct connections from incoming attempts
	IpcConnection(int s);

	void (*_handler)(void *,const SharedPtr<IpcConnection> &,const char *);
	void *_arg;
	volatile int _sock;
	Thread _thread;
	Mutex _writeLock;

	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif
