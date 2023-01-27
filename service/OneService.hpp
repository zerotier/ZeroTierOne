/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_ONESERVICE_HPP
#define ZT_ONESERVICE_HPP

#include <string>
#include <vector>

namespace ZeroTier {

#ifdef ZT_SDK
class VirtualTap;
// Use the virtual libzt endpoint instead of a tun/tap port driver
namespace ZeroTier { typedef VirtualTap EthernetTap; }
#endif

// Forward declaration so we can avoid dragging everything in
struct InetAddress;
class Node;

/**
 * Local service for ZeroTier One as system VPN/NFV provider
 */
class OneService
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
	 * Local settings for each network
	 */
	struct NetworkSettings
	{
		/**
		 * Allow this network to configure IP addresses and routes?
		 */
		bool allowManaged;

		/**
		 * Whitelist of addresses that can be configured by this network.
		 * If empty and allowManaged is true, allow all private/pseudoprivate addresses.
		 */
		std::vector<InetAddress> allowManagedWhitelist;

		/**
		 * Allow configuration of IPs and routes within global (Internet) IP space?
		 */
		bool allowGlobal;

		/**
		 * Allow overriding of system default routes for "full tunnel" operation?
		 */
		bool allowDefault;

		/**
		 * Allow configuration of DNS for the network
		 */
		bool allowDNS;
	};

	/**
	 * @return Platform default home path or empty string if this platform doesn't have one
	 */
	static std::string platformDefaultHomePath();

	/**
	 * Create a new instance of the service
	 *
	 * Once created, you must call the run() method to actually start
	 * processing.
	 *
	 * The port is saved to a file in the home path called zerotier-one.port,
	 * which is used by the CLI and can be used to see which port was chosen if
	 * 0 (random port) is picked.
	 *
	 * @param hp Home path
	 * @param port TCP and UDP port for packets and HTTP control (if 0, pick random port)
	 */
	static OneService *newInstance(const char *hp,unsigned int port, const char *sp);

	virtual ~OneService();

	/**
	 * Execute the service main I/O loop until terminated
	 *
	 * The terminate() method may be called from a signal handler or another
	 * thread to terminate execution. Otherwise this will not return unless
	 * another condition terminates execution such as a fatal error.
	 */
	virtual ReasonForTermination run() = 0;

	/**
	 * @return Reason for terminating or ONE_STILL_RUNNING if running
	 */
	virtual ReasonForTermination reasonForTermination() const = 0;

	/**
	 * @return Fatal error message or empty string if none
	 */
	virtual std::string fatalErrorMessage() const = 0;

	/**
	 * @return System device name corresponding with a given ZeroTier network ID or empty string if not opened yet or network ID not found
	 */
	virtual std::string portDeviceName(uint64_t nwid) const = 0;

#ifdef ZT_SDK
	/**
	 * Whether we allow access to the service via local HTTP requests (disabled by default in libzt)
	 */
	bool allowHttpBackplaneManagement = false;
	/**
	 * @return Reference to the Node
	 */
	virtual Node * getNode() = 0;
	/**
	 * Fills out a structure with network-specific route information
	 */
	virtual void getRoutes(uint64_t nwid, void *routeArray, unsigned int *numRoutes) = 0;
#endif

	/**
	 * Terminate background service (can be called from other threads)
	 */
	virtual void terminate() = 0;

	/**
	 * Get local settings for a network
	 *
	 * @param nwid Network ID
	 * @param settings Buffer to fill with local network settings
	 * @return True if network was found and settings is filled
	 */
	virtual bool getNetworkSettings(const uint64_t nwid,NetworkSettings &settings) const = 0;

	/**
	 * Set local settings for a network
	 *
	 * @param nwid Network ID
	 * @param settings New network local settings
	 * @return True if network was found and setting modified
	 */
	virtual bool setNetworkSettings(const uint64_t nwid,const NetworkSettings &settings) = 0;

	/**
	 * @return True if service is still running
	 */
	inline bool isRunning() const { return (this->reasonForTermination() == ONE_STILL_RUNNING); }

protected:
	OneService() {}

private:
	OneService(const OneService &one) {}
	inline OneService &operator=(const OneService &one) { return *this; }
};

} // namespace ZeroTier

#endif
