/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_SERVICE_HPP
#define _ZT_SERVICE_HPP

#include <string>
#include <stdexcept>

#include "Constants.hpp"
#include "Dictionary.hpp"
#include "Thread.hpp"
#include "Mutex.hpp"

/**
 * Maximum size of a service message in bytes (sanity limit)
 */
#define ZT_SERVICE_MAX_MESSAGE_SIZE 131072

namespace ZeroTier {

class RuntimeEnvironment;

#ifndef __WINDOWS__
/**
 * A subprocess that communicates with the host via a simple protocol
 *
 * This is currently only supported on *nix systems, and is used to implement
 * special plugins that are used by supernodes and network configuration
 * master nodes. Users will probably have no use for it.
 *
 * The simple binary protocol consists of a bidirectional stream of string-
 * serialized Dictionaries prefixed by a 32-bit message length. Input
 * messages are sent to the subprocess via its stdin, and output is read
 * from its stdout. Messages printed by the subprocess on its stderr are
 * logged via the standard Logger instance. If the subprocess dies, an
 * attempt is made to restart it every second.
 */
class Service : protected Thread
{
public:
	/**
	 * Create and launch a new service
	 *
	 * @param renv Runtime environment
	 * @param name Name of service
	 * @param path Path to service binary
	 * @param handler Handler function to call when service generates output
	 * @param arg First argument to service
	 */
	Service(const RuntimeEnvironment *renv,
	        const char *name,
	        const char *path,
	        void (*handler)(void *,Service &,const Dictionary &),
	        void *arg);

	virtual ~Service();

	/**
	 * Send a message to service subprocess
	 *
	 * @param msg Message in key/value dictionary form
	 * @return True if message was sent
	 */
	bool send(const Dictionary &msg);

	/**
	 * @return Name of service
	 */
	inline const char *name() const
		throw()
	{
		return _name.c_str();
	}

	/**
	 * @return True if subprocess is running
	 */
	inline bool running() const
		throw()
	{
		return (_pid > 0);
	}

protected:
	virtual void main()
		throw();

private:
	const RuntimeEnvironment *_r;
	std::string _path;
	std::string _name;
	void *_arg;
	void (*_handler)(void *,Service &,const Dictionary &);
	long _pid;
	int _childStdin;
	int _childStdout;
	int _childStderr;
	volatile bool _run;
};
#endif // __WINDOWS__

} // namespace ZeroTier

#endif
