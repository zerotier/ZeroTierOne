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

#pragma region Includes
#include "ZeroTierOneService.h"
#pragma endregion

ZeroTierOneService::ZeroTierOneService() :
	CServiceBase(ZT_SERVICE_NAME,TRUE,TRUE,FALSE),
	_node((ZeroTier::Node *)0)
{
}

ZeroTierOneService::~ZeroTierOneService(void)
{
}

void ZeroTierOneService::threadMain()
	throw()
{
restart_node:
	try {
		{
			// start or restart
			ZeroTier::Mutex::Lock _l(_lock);
			delete _node;
			_node = new ZeroTier::Node(ZeroTier::ZT_DEFAULTS.defaultHomePath.c_str(),0,0);
		}
		switch(_node->run()) {
			case ZeroTier::Node::NODE_RESTART_FOR_UPGRADE: {
			}	break;
			case ZeroTier::Node::NODE_UNRECOVERABLE_ERROR: {
				std::string err("ZeroTier node encountered an unrecoverable error: ");
				const char *r = _node->reasonForTermination();
				if (r)
					err.append(r);
				else err.append("(unknown error)");
				err.append(" (restarting in 5 seconds)");
				WriteEventLogEntry(const_cast <PSTR>(err.c_str()),EVENTLOG_ERROR_TYPE);
				Sleep(5000);
				goto restart_node;
			}	break;
			default: // includes normal termination, which will terminate thread
				break;
		}
	} catch ( ... ) {
		// sanity check, shouldn't happen since Node::run() should catch all its own errors
		// could also happen if we're out of memory though!
		WriteEventLogEntry("unexpected exception (out of memory?) (trying again in 5 seconds)",EVENTLOG_ERROR_TYPE);
		Sleep(5000);
		goto restart_node;
	}

	_lock.lock();
	delete _node;
	_node = (ZeroTier::Node *)0;
	_lock.unlock();
}

void ZeroTierOneService::OnStart(DWORD dwArgc, LPSTR *lpszArgv)
{
	if (_node)
		return; // sanity check
	try {
		_thread = ZeroTier::Thread::start(this);
	} catch ( ... ) {
		throw (DWORD)ERROR_EXCEPTION_IN_SERVICE;
	}
}

void ZeroTierOneService::OnStop()
{
	_lock.lock();
	ZeroTier::Node *n = _node;
	_lock.unlock();
	if (n) {
		n->terminate(ZeroTier::Node::NODE_NORMAL_TERMINATION,"Windows service stopped");
		ZeroTier::Thread::join(_thread);
	}
}

void ZeroTierOneService::OnShutdown()
{
	// stop thread on system shutdown (if it hasn't happened already)
	OnStop();
}
