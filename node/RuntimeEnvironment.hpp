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

#ifndef ZT_RUNTIMEENVIRONMENT_HPP
#define ZT_RUNTIMEENVIRONMENT_HPP

#include <string>

#include "Constants.hpp"
#include "Identity.hpp"

namespace ZeroTier {

class NodeConfig;
class Switch;
class Topology;
class CMWC4096;
class Node;
class Multicaster;
class AntiRecursion;
class NetworkController;
class SelfAwareness;

/**
 * Holds global state for an instance of ZeroTier::Node
 */
class RuntimeEnvironment
{
public:
	RuntimeEnvironment(Node *n) :
		node(n),
		identity(),
		localNetworkController((NetworkController *)0),
		prng((CMWC4096 *)0),
		sw((Switch *)0),
		mc((Multicaster *)0),
		antiRec((AntiRecursion *)0),
		topology((Topology *)0),
		sa((SelfAwareness *)0)
	{
	}

	// Node instance that owns this RuntimeEnvironment
	Node *const node;

	// This node's identity
	Identity identity;
	std::string publicIdentityStr;
	std::string secretIdentityStr;

	// This is set externally to an instance of this base class
	NetworkController *localNetworkController;

	/*
	 * Order matters a bit here. These are constructed in this order
	 * and then deleted in the opposite order on Node exit. The order ensures
	 * that things that are needed are there before they're needed.
	 *
	 * These are constant and never null after startup unless indicated.
	 */

	CMWC4096 *prng;
	Switch *sw;
	Multicaster *mc;
	AntiRecursion *antiRec;
	Topology *topology;
	SelfAwareness *sa;
};

} // namespace ZeroTier

#endif
