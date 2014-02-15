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

#ifndef ZT_RUNTIMEENVIRONMENT_HPP
#define ZT_RUNTIMEENVIRONMENT_HPP

#include <string>

#include "Constants.hpp"
#include "Identity.hpp"
#include "Condition.hpp"

namespace ZeroTier {

class NodeConfig;
class Logger;
class Demarc;
class Switch;
class Topology;
class SysEnv;
class CMWC4096;
class Service;
class Node;
class Multicaster;
class SoftwareUpdater;

/**
 * Holds global state for an instance of ZeroTier::Node
 * 
 * I do not believe in mutable static variables, period, or in global static
 * instances of objects that don't basically represent constants. It makes
 * unit testing, embedding, threading, and other things hard and is poor
 * practice.
 * 
 * So we put everything that we would want to be global, like Logger, here
 * and we give everybody this as _r. The Node creates and initializes this
 * on startup and deletes things on shutdown.
 */
class RuntimeEnvironment
{
public:
	RuntimeEnvironment() :
		shutdownInProgress(false),
		timeOfLastNetworkEnvironmentChange(0),
		log((Logger *)0),
		prng((CMWC4096 *)0),
		mc((Multicaster *)0),
		sw((Switch *)0),
		demarc((Demarc *)0),
		topology((Topology *)0),
		sysEnv((SysEnv *)0),
		nc((NodeConfig *)0),
		updater((SoftwareUpdater *)0)
#ifndef __WINDOWS__
		,netconfService((Service *)0)
#endif
	{
	}

	// Full path to home folder
	std::string homePath;

	// Main loop waits on this condition when it delays between runs, so
	// signaling this will prematurely wake it.
	Condition mainLoopWaitCondition;

	// This node's identity
	Identity identity;

	// Indicates that we are shutting down -- this is hacky, want to factor out
	volatile bool shutdownInProgress;

	// Time network environment (e.g. fingerprint) last changed -- used to determine online-ness
	volatile uint64_t timeOfLastNetworkEnvironmentChange;

	/*
	 * Order matters a bit here. These are constructed in this order
	 * and then deleted in the opposite order on Node exit. The order ensures
	 * that things that are needed are there before they're needed.
	 *
	 * These are constant and never null after startup unless indicated.
	 */

	Logger *log; // null if logging is disabled
	CMWC4096 *prng;
	Multicaster *mc;
	Switch *sw;
	Demarc *demarc;
	Topology *topology;
	SysEnv *sysEnv;
	NodeConfig *nc;
	Node *node;
	SoftwareUpdater *updater; // null if software updates are not enabled
#ifndef __WINDOWS__
	Service *netconfService; // null if no netconf service running
#endif
};

} // namespace ZeroTier

#endif
