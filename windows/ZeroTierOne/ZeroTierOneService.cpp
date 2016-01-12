/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#pragma region Includes

#include <WinSock2.h>
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "ZeroTierOneService.h"

#include "../../version.h"
#include "../../include/ZeroTierOne.h"

#include "../../node/Constants.hpp"
#include "../../node/Utils.hpp"
#include "../../osdep/OSUtils.hpp"
#include "../../service/OneService.hpp"

#pragma endregion // Includes

#ifdef ZT_DEBUG_SERVICE
FILE *SVCDBGfile = (FILE *)0;
ZeroTier::Mutex SVCDBGfile_m;
#endif

ZeroTierOneService::ZeroTierOneService() :
	CServiceBase(ZT_SERVICE_NAME,TRUE,TRUE,FALSE),
	_service((ZeroTier::OneService *)0)
{
#ifdef ZT_DEBUG_SERVICE
	SVCDBGfile_m.lock();
	if (!SVCDBGfile)
		SVCDBGfile = fopen(ZT_DEBUG_SERVICE,"a");
	SVCDBGfile_m.unlock();
#endif

	ZT_SVCDBG("ZeroTierOneService::ZeroTierOneService()\r\n");
}

ZeroTierOneService::~ZeroTierOneService(void)
{
	ZT_SVCDBG("ZeroTierOneService::~ZeroTierOneService()\r\n");

#ifdef ZT_DEBUG_SERVICE
	SVCDBGfile_m.lock();
	if (SVCDBGfile) {
		fclose(SVCDBGfile);
		SVCDBGfile = (FILE *)0;
	}
	SVCDBGfile_m.unlock();
#endif
}

void ZeroTierOneService::threadMain()
	throw()
{
	ZT_SVCDBG("ZeroTierOneService::threadMain()\r\n");

restart_node:
	try {
		{
			ZeroTier::Mutex::Lock _l(_lock);
			delete _service;
			_service = (ZeroTier::OneService *)0; // in case newInstance() fails
			_service = ZeroTier::OneService::newInstance(
				ZeroTier::OneService::platformDefaultHomePath().c_str(),
				ZT_DEFAULT_PORT);
		}
		switch(_service->run()) {
			case ZeroTier::OneService::ONE_UNRECOVERABLE_ERROR: {
				std::string err("ZeroTier One encountered an unrecoverable error: ");
				err.append(_service->fatalErrorMessage());
				err.append(" (restarting in 5 seconds)");
				WriteEventLogEntry(const_cast <PSTR>(err.c_str()),EVENTLOG_ERROR_TYPE);
				Sleep(5000);
			}	goto restart_node;

			case ZeroTier::OneService::ONE_IDENTITY_COLLISION: {
				std::string homeDir(ZeroTier::OneService::platformDefaultHomePath());
				delete _service;
				_service = (ZeroTier::OneService *)0;
				std::string oldid;
				ZeroTier::OSUtils::readFile((homeDir + ZT_PATH_SEPARATOR_S + "identity.secret").c_str(),oldid);
				if (oldid.length()) {
					ZeroTier::OSUtils::writeFile((homeDir + ZT_PATH_SEPARATOR_S + "identity.secret.saved_after_collision").c_str(),oldid);
					ZeroTier::OSUtils::rm((homeDir + ZT_PATH_SEPARATOR_S + "identity.secret").c_str());
					ZeroTier::OSUtils::rm((homeDir + ZT_PATH_SEPARATOR_S + "identity.public").c_str());
				}
			}	goto restart_node;

			default: // normal termination
				break;
		}
	} catch ( ... ) {
		// sanity check, shouldn't happen since Node::run() should catch all its own errors
		// could also happen if we're out of memory though!
		WriteEventLogEntry("unexpected exception (out of memory?) (trying again in 5 seconds)",EVENTLOG_ERROR_TYPE);
		Sleep(5000);
		goto restart_node;
	}

	{
		ZeroTier::Mutex::Lock _l(_lock);
		delete _service;
		_service = (ZeroTier::OneService *)0;
	}
}

void ZeroTierOneService::OnStart(DWORD dwArgc, LPSTR *lpszArgv)
{
	ZT_SVCDBG("ZeroTierOneService::OnStart()\r\n");

	try {
		_thread = ZeroTier::Thread::start(this);
	} catch ( ... ) {
		throw (DWORD)ERROR_EXCEPTION_IN_SERVICE;
	}
}

void ZeroTierOneService::OnStop()
{
	ZT_SVCDBG("ZeroTierOneService::OnStop()\r\n");

	_lock.lock();
	ZeroTier::OneService *s = _service;
	_lock.unlock();

	if (s) {
		s->terminate();
		ZeroTier::Thread::join(_thread);
	}
}

void ZeroTierOneService::OnShutdown()
{
	ZT_SVCDBG("ZeroTierOneService::OnShutdown()\r\n");

	// stop thread on system shutdown (if it hasn't happened already)
	OnStop();
}
