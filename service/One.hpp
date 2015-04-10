/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#ifndef ZT_ONE_HPP
#define ZT_ONE_HPP

#include <string>

namespace ZeroTier {

class NetworkConfigMaster;

/**
 * ZeroTier One -- local VPN/NVF service built around ZeroTier core
 *
 * Actual implementation is under the fold, hence the pure virtual
 * interface.
 */
class One
{
public:
	/**
	 * Returned by node main if/when it terminates
	 */
	enum ReasonForTermination
	{
		/**
		 * Instance is still running
		 */
		ONE_STILL_RUNNING = 0,

		/**
		 * Normal shutdown
		 */
		ONE_NORMAL_TERMINATION = 1,

		/**
		 * A serious unrecoverable error has occurred
		 */
		ONE_UNRECOVERABLE_ERROR = 2,

		/**
		 * Your identity has collided with another
		 */
		ONE_IDENTITY_COLLISION = 3
	};

	/**
	 * @return Platform default home path or empty string if this platform doesn't have one
	 */
	static std::string platformDefaultHomePath();

	/**
	 * Create and start a new instance of the service
	 *
	 * @param hp Home path
	 * @param port TCP and UDP port for packets and HTTP control
	 * @param master Instance of network config master if this instance is to act as one (default: NULL)
	 * @param overrideRootTopology String-serialized root topology (for testing, default: NULL)
	 */
	static One *newInstance(
		const char *hp,
		unsigned int port,
		NetworkConfigMaster *master = (NetworkConfigMaster *)0,
		const char *overrideRootTopology = (const char *)0);

	/**
	 * Deletion will block until service stops if it's still running
	 */
	virtual ~One();

	/**
	 * @return Reason for terminating or ONE_STILL_RUNNING if running
	 */
	virtual ReasonForTermination reasonForTermination() const = 0;

	/**
	 * @return Fatal error message or empty string if none
	 */
	virtual std::string fatalErrorMessage() const = 0;

	/**
	 * Block until service terminates
	 */
	virtual void waitForTermination() = 0;

	/**
	 * Terminate background service
	 *
	 * Actual shutdown might take a few seconds.
	 */
	virtual void terminate() = 0;

	/**
	 * @return True if service is still running
	 */
	inline bool isRunning() const { return (this->reasonForTermination() == ONE_STILL_RUNNING); }

protected:
	One() {}

private:
	One(const One &one) {}
	inline One &operator=(const One &one) { return *this; }
};

} // namespace ZeroTier

#endif
