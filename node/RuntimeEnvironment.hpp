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

#ifndef _ZT_RUNTIMEENVIRONMENT_HPP
#define _ZT_RUNTIMEENVIRONMENT_HPP

#include <string>
#include "Identity.hpp"
#include "Condition.hpp"

namespace ZeroTier {

class NodeConfig;
class Logger;
class Demarc;
class Switch;
class Topology;
class SysEnv;
class Multicaster;

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
		identity(),
		log((Logger *)0),
		nc((NodeConfig *)0),
		demarc((Demarc *)0),
		multicaster((Multicaster *)0),
		sw((Switch *)0),
		topology((Topology *)0)
	{
	}

	std::string homePath;
	std::string autoconfUrlPrefix;
	std::string configAuthorityIdentityStr;
	std::string ownershipVerificationSecret;
	std::string ownershipVerificationSecretHash; // base64 of SHA-256 X16 rounds

	// signal() to prematurely interrupt main loop wait
	Condition mainLoopWaitCondition;

	Identity configAuthority;
	Identity identity;

	Logger *log; // may be null
	NodeConfig *nc;
	Demarc *demarc;
	Multicaster *multicaster;
	Switch *sw;
	Topology *topology;
	SysEnv *sysEnv;
};

} // namespace ZeroTier

#endif
