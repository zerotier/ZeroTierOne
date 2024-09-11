/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#pragma region Includes

#if defined(_WIN32) || defined(_WIN64)

#include <winsock2.h>
#include <windows.h>
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
			_service = ZeroTier::OneService::newInstance(_path.c_str(), ZT_DEFAULT_PORT);
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

void ZeroTierOneService::OnStart(DWORD dwArgc, PSTR *lpszArgv)
{
	ZT_SVCDBG("ZeroTierOneService::OnStart()\r\n");

	if ((dwArgc > 1)&&(lpszArgv[1])&&(strlen(lpszArgv[1]) > 0)) {
		this->_path = lpszArgv[1];
	} else {
		this->_path = ZeroTier::OneService::platformDefaultHomePath();
	}

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

#endif
