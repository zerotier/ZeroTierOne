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

#pragma region Includes

#include <WinSock2.h>
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "ZeroTierOneService.h"

#include "../../node/Defaults.hpp"
#include "../../node/Utils.hpp"

#include "../../control/NodeControlClient.hpp"
#include "../../control/NodeControlService.hpp"

#include "../../osnet/WindowsEthernetTapFactory.hpp"
#include "../../osnet/WindowsRoutingTable.hpp"
#include "../../osnet/NativeSocketManager.hpp"

#pragma endregion // Includes

#ifdef ZT_DEBUG_SERVICE
FILE *SVCDBGfile = (FILE *)0;
ZeroTier::Mutex SVCDBGfile_m;
#endif

ZeroTierOneService::ZeroTierOneService() :
	CServiceBase(ZT_SERVICE_NAME,TRUE,TRUE,FALSE),
	_node((ZeroTier::Node *)0)
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
		std::string authToken(ZeroTier::NodeControlClient::getAuthToken((ZeroTier::ZT_DEFAULTS.defaultHomePath + ZT_PATH_SEPARATOR_S + "authtoken.secret").c_str(),true));

		ZeroTier::WindowsEthernetTapFactory tapFactory(ZeroTier::ZT_DEFAULTS.defaultHomePath.c_str());
		ZeroTier::WindowsRoutingTable routingTable;
		ZeroTier::NativeSocketManager socketManager(ZT_DEFAULT_UDP_PORT,0);

		{
			// start or restart
			ZeroTier::Mutex::Lock _l(_lock);
			delete _node;
			_node = new ZeroTier::Node(ZeroTier::ZT_DEFAULTS.defaultHomePath.c_str(),&tapFactory,&routingTable,&socketManager,false,(const char *)0);
		}

		ZeroTier::NodeControlService controlService(_node,authToken.c_str());

		switch(_node->run()) {

			case ZeroTier::Node::NODE_RESTART_FOR_UPGRADE: {
				// Shut down node
				ZeroTier::Node *n;
				{
					ZeroTier::Mutex::Lock _l(_lock);
					n = _node;
					_node = (ZeroTier::Node *)0;
				}

				// Get upgrade path, which will be its reason for termination
				std::string msiPath;
				if (n) {
					const char *msiPathTmp = n->terminationMessage();
					if (msiPathTmp)
						msiPath = msiPathTmp;
				}

				delete n;

				if ((!msiPath.length())||(!ZeroTier::Utils::fileExists(msiPath.c_str()))) {
					WriteEventLogEntry("auto-update failed: no msi path provided by Node",EVENTLOG_ERROR_TYPE);
					Sleep(5000);
					goto restart_node;
				}

				if (!doStartUpgrade(msiPath)) {
					WriteEventLogEntry("auto-update failed: unable to create InstallAndRestartService.bat",EVENTLOG_ERROR_TYPE);
					Sleep(5000);
					goto restart_node;
				}

				// Terminate service to allow updater to update
				Stop();
			}	return;

			case ZeroTier::Node::NODE_UNRECOVERABLE_ERROR: {
				std::string err("ZeroTier node encountered an unrecoverable error: ");
				const char *r = _node->terminationMessage();
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

	{
		ZeroTier::Mutex::Lock _l(_lock);
		delete _node;
		_node = (ZeroTier::Node *)0;
	}
}

bool ZeroTierOneService::doStartUpgrade(const std::string &msiPath)
{
	std::string msiLog(ZeroTier::ZT_DEFAULTS.defaultHomePath + "\\LastUpdateLog.txt");
	ZeroTier::Utils::rm(msiLog);

	std::string bat(ZeroTier::ZT_DEFAULTS.defaultHomePath + "\\InstallAndRestartService.bat");
	FILE *batf = fopen(bat.c_str(),"wb");
	if (!batf)
		return false;
	fprintf(batf,"TIMEOUT.EXE /T 1 /NOBREAK\r\n");
	fprintf(batf,"NET.EXE STOP \"ZeroTier One\"\r\n");
	fprintf(batf,"MSIEXEC.EXE /i \"%s\" /l* \"%s\" /qn\r\n",msiPath.c_str(),msiLog.c_str());
	fprintf(batf,"NET.EXE START \"ZeroTier One\"\r\n");
	fclose(batf);

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	memset(&si,0,sizeof(si));
	memset(&pi,0,sizeof(pi));
	if (!CreateProcessA(NULL,const_cast <LPSTR>((std::string("CMD.EXE /c \"") + bat + "\"").c_str()),NULL,NULL,FALSE,CREATE_NO_WINDOW|CREATE_NEW_PROCESS_GROUP,NULL,NULL,&si,&pi))
		return false;

	return true;
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
	ZeroTier::Node *n = _node;
	_lock.unlock();
	if (n) {
		n->terminate(ZeroTier::Node::NODE_NORMAL_TERMINATION,"Windows service stopped");
		ZeroTier::Thread::join(_thread);
	}
}

void ZeroTierOneService::OnShutdown()
{
	ZT_SVCDBG("ZeroTierOneService::OnShutdown()\r\n");

	// stop thread on system shutdown (if it hasn't happened already)
	OnStop();
}
