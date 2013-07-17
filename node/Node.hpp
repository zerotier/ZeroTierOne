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

#ifndef _ZT_NODE_HPP
#define _ZT_NODE_HPP

namespace ZeroTier {

/**
 * A ZeroTier One node
 *
 * This class hides all its implementation details and all other classes in
 * preparation for ZeroTier One being made available in library form for
 * embedding in mobile apps.
 */
class Node
{
public:
	/**
	 * Returned by node main if/when it terminates
	 */
	enum ReasonForTermination
	{
		NODE_RUNNING = 0,
		NODE_NORMAL_TERMINATION = 1,
		NODE_RESTART_FOR_RECONFIGURATION = 2,
		NODE_UNRECOVERABLE_ERROR = 3,
		NODE_NEW_VERSION_AVAILABLE = 4
	};

	/**
	 * Create a new node
	 *
	 * The node is not executed until run() is called.
	 *
	 * @param hp Home directory path
	 */
	Node(const char *hp)
		throw();

	~Node();

	/**
	 * Execute node in current thread
	 *
	 * This does not return until the node shuts down. Shutdown may be caused
	 * by an internally detected condition such as a new upgrade being
	 * available or a fatal error, or it may be signaled externally using
	 * the terminate() method.
	 *
	 * @return Reason for termination
	 */
	ReasonForTermination run()
		throw();

	/**
	 * Obtain a human-readable reason for node termination
	 *
	 * @return Reason for node termination or NULL if run() has not returned
	 */
	const char *reasonForTermination() const
		throw();

	/**
	 * Cause run() to return with NODE_NORMAL_TERMINATION
	 *
	 * This can be called from a signal handler or another thread to signal a
	 * running node to shut down. Shutdown may take a few seconds, so run()
	 * may not return instantly. Multiple calls are ignored.
	 */
	void terminate()
		throw();

	/**
	 * Get the ZeroTier version in major.minor.revision string format
	 * 
	 * @return Version in string form
	 */
	static const char *versionString()
		throw();

	static unsigned int versionMajor() throw();
	static unsigned int versionMinor() throw();
	static unsigned int versionRevision() throw();

private:
	void *const _impl; // private implementation
};

/**
 * An embedded version code that can be searched for in the binary
 *
 * This shouldn't be used by users, but is exported to make certain that
 * the linker actually includes it in the image.
 */
extern const unsigned char EMBEDDED_VERSION_STAMP[20];

} // namespace ZeroTier

#endif

