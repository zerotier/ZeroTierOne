/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_RUNTIMEENVIRONMENT_HPP
#define ZT_RUNTIMEENVIRONMENT_HPP

#include <string>

#include "Constants.hpp"
#include "Identity.hpp"
#include "Mutex.hpp"

namespace ZeroTier {

class NodeConfig;
class Switch;
class Topology;
class Node;
class Multicaster;
class NetworkController;
class SelfAwareness;
class Cluster;

/**
 * Holds global state for an instance of ZeroTier::Node
 */
class RuntimeEnvironment
{
public:
	RuntimeEnvironment(Node *n) :
		node(n)
		,identity()
		,localNetworkController((NetworkController *)0)
		,sw((Switch *)0)
		,mc((Multicaster *)0)
		,topology((Topology *)0)
		,sa((SelfAwareness *)0)
#ifdef ZT_ENABLE_CLUSTER
		,cluster((Cluster *)0)
#endif
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

	Switch *sw;
	Multicaster *mc;
	Topology *topology;
	SelfAwareness *sa;
#ifdef ZT_ENABLE_CLUSTER
	Cluster *cluster;
#endif
};

} // namespace ZeroTier

#endif
