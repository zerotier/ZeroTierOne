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

#include "WindowsEthernetTapFactory.hpp"
#include "WindowsEthernetTap.hpp"

namespace ZeroTier {

WindowsEthernetTapFactory::Env::Env()
{
#ifdef _WIN64
	is64Bit = TRUE;
	devcon = "\\devcon_x64.exe";
	tapDriver = "\\tap-windows\\x64\\zttap200.inf";
#else
	is64Bit = FALSE;
	IsWow64Process(GetCurrentProcess(),&is64Bit);
	devcon = ((is64Bit == TRUE) ? "\\devcon_x64.exe" : "\\devcon_x86.exe");
	tapDriver = ((is64Bit == TRUE) ? "\\tap-windows\\x64\\zttap200.inf" : "\\tap-windows\\x86\\zttap200.inf");
#endif
}
const WindowsEthernetTapFactory::Env WindowsEthernetTapFactory::WINENV;

WindowsEthernetTapFactory::WindowsEthernetTapFactory(const char *pathToHelpers) :
	_pathToHelpers(pathToHelpers)
{
}

WindowsEthernetTapFactory::~WindowsEthernetTapFactory()
{
	Mutex::Lock _l(_devices_m);
	for(std::vector<EthernetTap *>::iterator d(_devices.begin());d!=_devices.end();++d)
		delete *d;
}

EthernetTap *WindowsEthernetTapFactory::open(
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *desiredDevice,
	const char *friendlyName,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg)
{
	Mutex::Lock _l(_devices_m);
	EthernetTap *t = new WindowsEthernetTap(_pathToHelpers.c_str(),mac,mtu,metric,nwid,desiredDevice,friendlyName,handler,arg);
	_devices.push_back(t);
	return t;
}

void WindowsEthernetTapFactory::close(EthernetTap *tap,bool destroyPersistentDevices)
{
	if (!tap)
		return;

	std::string instanceId(((WindowsEthernetTap *)tap)->instanceId());
	Mutex::Lock _l(_devices_m);

	for(std::vector<EthernetTap *>::iterator d(_devices.begin());d!=_devices.end();++d) {
		if (*d == tap) {
			_devices.erase(d);
			break;
		}
	}

	delete tap;

	if (destroyPersistentDevices)
		_deletePersistentTapDevice(_pathToHelpers.c_str(),instanceId.c_str());
}

void WindowsEthernetTapFactory::destroyAllPersistentTapDevices(const char *pathToHelpers)
{
	char subkeyName[4096];
	char subkeyClass[4096];
	char data[4096];

	std::set<std::string> instanceIdPathsToRemove;
	{
		HKEY nwAdapters;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}",0,KEY_READ|KEY_WRITE,&nwAdapters) != ERROR_SUCCESS)
			return;

		for(DWORD subkeyIndex=0;;++subkeyIndex) {
			DWORD type;
			DWORD dataLen;
			DWORD subkeyNameLen = sizeof(subkeyName);
			DWORD subkeyClassLen = sizeof(subkeyClass);
			FILETIME lastWriteTime;
			if (RegEnumKeyExA(nwAdapters,subkeyIndex,subkeyName,&subkeyNameLen,(DWORD *)0,subkeyClass,&subkeyClassLen,&lastWriteTime) == ERROR_SUCCESS) {
				type = 0;
				dataLen = sizeof(data);
				if (RegGetValueA(nwAdapters,subkeyName,"ComponentId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
					data[dataLen] = '\0';
					if (!strnicmp(data,"zttap",5)) {
						std::string instanceIdPath;
						type = 0;
						dataLen = sizeof(data);
						if (RegGetValueA(nwAdapters,subkeyName,"DeviceInstanceID",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS)
							instanceIdPath.assign(data,dataLen);
						if (instanceIdPath.length() != 0)
							instanceIdPathsToRemove.insert(instanceIdPath);
					}
				}
			} else break; // end of list or failure
		}

		RegCloseKey(nwAdapters);
	}

	for(std::set<std::string>::iterator iidp(instanceIdPathsToRemove.begin());iidp!=instanceIdPathsToRemove.end();++iidp)
		_deletePersistentTapDevice(pathToHelpers,iidp->c_str());
}

void WindowsEthernetTapFactory::_deletePersistentTapDevice(const char *pathToHelpers,const char *instanceId)
{
	HANDLE devconLog = CreateFileA((std::string(pathToHelpers) + "\\devcon.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	STARTUPINFOA startupInfo;
	startupInfo.cb = sizeof(startupInfo);
	if (devconLog != INVALID_HANDLE_VALUE) {
		SetFilePointer(devconLog,0,0,FILE_END);
		startupInfo.hStdOutput = devconLog;
		startupInfo.hStdError = devconLog;
	}
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo,0,sizeof(STARTUPINFOA));
	memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
	if (CreateProcessA(NULL,(LPSTR)(std::string("\"") + pathToHelpers + WINENV.devcon + "\" remove @" + instanceId).c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
		WaitForSingleObject(processInfo.hProcess,INFINITE);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
	}
	if (devconLog != INVALID_HANDLE_VALUE)
		CloseHandle(devconLog);
}

} // namespace ZeroTier
