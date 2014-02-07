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

#pragma region Includes
#include "ZeroTierOneService.h"
#include "../../node/Node.hpp"
#include "../../node/Defaults.hpp"
#include "../../node/Thread.hpp"
#pragma endregion

using namespace ZeroTier;

ZeroTierOneService::ZeroTierOneService() :
	CServiceBase(ZT_SERVICE_NAME,TRUE,TRUE,FALSE),
	_thread(new Thread()),
	_node((Node *)0)
{
}

ZeroTierOneService::~ZeroTierOneService(void)
{
	delete _thread;
	delete _node;
}

void ZeroTierOneService::threadMain()
	throw()
{
	try {
		// Since Windows doesn't auto-restart services, we'll restart the node
		// on normal termination.
		for(;;) {
			switch(_node->run()) {
				case Node::NODE_NORMAL_TERMINATION:
					delete _node;
					_node = new Node(ZT_DEFAULTS.defaultHomePath.c_str(),0,0);
					break; // restart
				case Node::NODE_RESTART_FOR_UPGRADE: {
				}	return; // terminate thread
				case Node::NODE_UNRECOVERABLE_ERROR: {
					std::string err("unrecoverable error: ");
					const char *r = _node->reasonForTermination;
					if (r)
						err.append(r);
					else err.append("(unknown error)");
					WriteEventLogEntry(const_cast <PSTR>(err.c_str()),EVENTLOG_ERROR_TYPE);
				}	return; // terminate thread
				default:
					break;
			}
		}
	} catch ( ... ) {
		WriteEventLogEntry("unexpected exception in Node::run() or during restart",EVENTLOG_ERROR_TYPE);
	}
}

void ZeroTierOneService::OnStart(DWORD dwArgc, LPSTR *lpszArgv)
{
	try {
		_node = new Node(ZT_DEFAULTS.defaultHomePath.c_str(),0,0);
		*_thread = Thread::start(this);
	} catch ( ... ) {
		// shouldn't happen unless something weird occurs like out of memory...
		throw (DWORD)ERROR_EXCEPTION_IN_SERVICE;
	}
}

void ZeroTierOneService::OnStop()
{
	Node *n = _node;
	_node = (Node *)0;
	if (n) {
		n->terminate(Node::NODE_NORMAL_TERMINATION,"Service Shutdown");
		Thread::join(*_thread);
		delete n;
	}
}

void ZeroTierOneService::OnShutdown()
{
	// make sure it's stopped
	OnStop();
}
