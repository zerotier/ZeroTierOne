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

#ifndef ZT_NODECONTROLSERVICE_HPP
#define ZT_NODECONTROLSERVICE_HPP

#include <string>
#include <map>

#include "IpcConnection.hpp"
#include "IpcListener.hpp"

#include "../node/Constants.hpp"
#include "../node/NonCopyable.hpp"
#include "../node/Thread.hpp"

namespace ZeroTier {

class Node;

/**
 * Background controller service that controls and configures a node
 *
 * This is used with system-installed instances of ZeroTier One to
 * provide the IPC-based control bus service for node configuration.
 */
class NodeControlService : NonCopyable
{
public:
	/**
	 * @param node Node to control and configure
	 * @param authToken Authorization token for clients
	 */
	NodeControlService(Node *node,const char *authToken);

	~NodeControlService();

	// Background thread waits for node to initialize, then creates IpcListener and
	// terminates. It also terminates on delete if it hasn't bootstrapped yet.
	void threadMain()
		throw();

private:
	static void _CBcommandHandler(void *arg,IpcConnection *ipcc,IpcConnection::EventType event,const char *commandLine);
	void _doCommand(IpcConnection *ipcc,const char *commandLine);

	Node *_node;
	IpcListener *_listener;
	std::string _authToken;

	std::map< IpcConnection *,bool > _connections;
	Mutex _connections_m;

	volatile bool _running;
	Thread _thread;
};

} // namespace ZeroTier

#endif
