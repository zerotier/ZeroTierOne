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

#pragma once

#include <stdio.h>

#include "ServiceBase.h"

#include <string>

#include "../../node/Mutex.hpp"
#include "../../osdep/Thread.hpp"
#include "../../service/OneService.hpp"

// Uncomment to make debugging Windows services suck slightly less hard.
//#define ZT_DEBUG_SERVICE "C:\\ZeroTierOneServiceDebugLog.txt"

#ifdef ZT_DEBUG_SERVICE
extern FILE *SVCDBGfile;
extern ZeroTier::Mutex SVCDBGfile_m;
#define ZT_SVCDBG(f,...) { SVCDBGfile_m.lock(); fprintf(SVCDBGfile,f,##__VA_ARGS__); fflush(SVCDBGfile); SVCDBGfile_m.unlock(); }
#else
#define ZT_SVCDBG(f,...) {}
#endif

#define ZT_SERVICE_NAME "ZeroTierOneService"
#define ZT_SERVICE_DISPLAY_NAME "ZeroTier One"
#define ZT_SERVICE_START_TYPE SERVICE_AUTO_START
#define ZT_SERVICE_DEPENDENCIES ""
//#define ZT_SERVICE_ACCOUNT "NT AUTHORITY\\LocalService"
#define ZT_SERVICE_ACCOUNT NULL
#define ZT_SERVICE_PASSWORD NULL

class ZeroTierOneService : public CServiceBase
{
public:
    ZeroTierOneService();
    virtual ~ZeroTierOneService(void);

	/**
	 * Thread main method; do not call elsewhere
	 */
	void threadMain()
		throw();

protected:
    virtual void OnStart(DWORD dwArgc, PSTR *pszArgv);
    virtual void OnStop();
	virtual void OnShutdown();

private:
	ZeroTier::OneService *volatile _service;
	ZeroTier::Mutex _lock;
	ZeroTier::Thread _thread;
};
