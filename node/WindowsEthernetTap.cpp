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

#include "Constants.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <WinSock2.h>
#include <Windows.h>
#include <tchar.h>
#include <winreg.h>
#include <wchar.h>
#include <ws2ipdef.h>
#include <WS2tcpip.h>
#include <IPHlpApi.h>
#include <nldef.h>
#include <netioapi.h>

#include "EthernetTap.hpp"
#include "WindowsEthernetTap.hpp"
#include "Logger.hpp"
#include "RuntimeEnvironment.hpp"
#include "Utils.hpp"
#include "Mutex.hpp"

#include "..\windows\TapDriver\tap-windows.h"

// ff:ff:ff:ff:ff:ff with no ADI
static const ZeroTier::MulticastGroup _blindWildcardMulticastGroup(ZeroTier::MAC(0xff),0);

namespace ZeroTier {

// Helper function to get an adapter's LUID and index from its GUID. The LUID is
// constant but the index can change, so go ahead and just look them both up by
// the GUID which is constant. (The GUID is the instance ID in the registry.)
static inline std::pair<NET_LUID,NET_IFINDEX> _findAdapterByGuid(const GUID &guid)
	throw(std::runtime_error)
{
	MIB_IF_TABLE2 *ift = (MIB_IF_TABLE2 *)0;

	if (GetIfTable2Ex(MibIfTableRaw,&ift) != NO_ERROR)
		throw std::runtime_error("GetIfTable2Ex() failed");

	for(ULONG i=0;i<ift->NumEntries;++i) {
		if (ift->Table[i].InterfaceGuid == guid) {
			std::pair<NET_LUID,NET_IFINDEX> tmp(ift->Table[i].InterfaceLuid,ift->Table[i].InterfaceIndex);
			FreeMibTable(ift);
			return tmp;
		}
	}

	FreeMibTable(&ift);

	throw std::runtime_error("interface not found");
}

// Only create or manipulate devices one at a time to avoid weird driver layer demons
static Mutex _systemTapInitLock;

// Compute some basic environment stuff on startup
class _WinSysEnv
{
public:
	_WinSysEnv()
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
	BOOL is64Bit;
	const char *devcon;
	const char *tapDriver;
};
static const _WinSysEnv _winEnv;

static bool _disableTapDevice(const RuntimeEnvironment *_r,const std::string deviceInstanceId)
{
	HANDLE devconLog = CreateFileA((_r->homePath + "\\devcon.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if (devconLog != INVALID_HANDLE_VALUE)
		SetFilePointer(devconLog,0,0,FILE_END);

	STARTUPINFOA startupInfo;
	startupInfo.cb = sizeof(startupInfo);
	if (devconLog != INVALID_HANDLE_VALUE) {
		startupInfo.hStdOutput = devconLog;
		startupInfo.hStdError = devconLog;
	}
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo,0,sizeof(STARTUPINFOA));
	memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
	if (!CreateProcessA(NULL,(LPSTR)(std::string("\"") + _r->homePath + _winEnv.devcon + "\" disable @" + deviceInstanceId).c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
		if (devconLog != INVALID_HANDLE_VALUE)
			CloseHandle(devconLog);
		return false;
	}
	WaitForSingleObject(processInfo.hProcess,INFINITE);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);

	if (devconLog != INVALID_HANDLE_VALUE)
		CloseHandle(devconLog);

	return true;
}
static bool _enableTapDevice(const RuntimeEnvironment *_r,const std::string deviceInstanceId)
{
	HANDLE devconLog = CreateFileA((_r->homePath + "\\devcon.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if (devconLog != INVALID_HANDLE_VALUE)
		SetFilePointer(devconLog,0,0,FILE_END);

	STARTUPINFOA startupInfo;
	startupInfo.cb = sizeof(startupInfo);
	if (devconLog != INVALID_HANDLE_VALUE) {
		startupInfo.hStdOutput = devconLog;
		startupInfo.hStdError = devconLog;
	}
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo,0,sizeof(STARTUPINFOA));
	memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
	if (!CreateProcessA(NULL,(LPSTR)(std::string("\"") + _r->homePath + _winEnv.devcon + "\" enable @" + deviceInstanceId).c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
		if (devconLog != INVALID_HANDLE_VALUE)
			CloseHandle(devconLog);
		return false;
	}
	WaitForSingleObject(processInfo.hProcess,INFINITE);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);

	if (devconLog != INVALID_HANDLE_VALUE)
		CloseHandle(devconLog);

	return true;
}

static void _syncIpsWithRegistry(const std::set<InetAddress> &haveIps,const std::string netCfgInstanceId)
{
	// Update registry to contain all non-link-local IPs for this interface
	std::string regMultiIps,regMultiNetmasks;
	for(std::set<InetAddress>::const_iterator i(haveIps.begin());i!=haveIps.end();++i) {
		if (!i->isLinkLocal()) {
			regMultiIps.append(i->toIpString());
			regMultiIps.push_back((char)0);
			regMultiNetmasks.append(i->netmask().toIpString());
			regMultiNetmasks.push_back((char)0);
		}
	}
	HKEY tcpIpInterfaces;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\services\\Tcpip\\Parameters\\Interfaces",0,KEY_READ|KEY_WRITE,&tcpIpInterfaces) == ERROR_SUCCESS) {
		if (regMultiIps.length()) {
			regMultiIps.push_back((char)0);
			regMultiNetmasks.push_back((char)0);
			RegSetKeyValueA(tcpIpInterfaces,netCfgInstanceId.c_str(),"IPAddress",REG_MULTI_SZ,regMultiIps.data(),(DWORD)regMultiIps.length());
			RegSetKeyValueA(tcpIpInterfaces,netCfgInstanceId.c_str(),"SubnetMask",REG_MULTI_SZ,regMultiNetmasks.data(),(DWORD)regMultiNetmasks.length());
		} else {
			RegDeleteKeyValueA(tcpIpInterfaces,netCfgInstanceId.c_str(),"IPAddress");
			RegDeleteKeyValueA(tcpIpInterfaces,netCfgInstanceId.c_str(),"SubnetMask");
		}
	}
	RegCloseKey(tcpIpInterfaces);
}

WindowsEthernetTap::WindowsEthernetTap(
	const RuntimeEnvironment *renv,
	const char *tag,
	const MAC &mac,
	unsigned int mtu,
	void (*handler)(void *,const MAC &,const MAC &,unsigned int,const Buffer<4096> &),
	void *arg)
	throw(std::runtime_error) :
	EthernetTap("WindowsEthernetTap",mac,mtu),
	_r(renv),
	_handler(handler),
	_arg(arg),
	_tap(INVALID_HANDLE_VALUE),
	_injectSemaphore(INVALID_HANDLE_VALUE),
	_run(true),
	_initialized(false),
	_enabled(false)
{
	char subkeyName[4096];
	char subkeyClass[4096];
	char data[4096];

	if (mtu > ZT_IF_MTU)
		throw std::runtime_error("MTU too large for Windows tap");

	Mutex::Lock _l(_systemTapInitLock); // only one thread may mess with taps at a time, process-wide

	HKEY nwAdapters;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}",0,KEY_READ|KEY_WRITE,&nwAdapters) != ERROR_SUCCESS)
		throw std::runtime_error("unable to open registry key for network adapter enumeration");

	std::set<std::string> existingDeviceInstances;
	std::string mySubkeyName;

	// Look for the tap instance that corresponds with our interface tag (network ID)
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
					std::string instanceId;
					type = 0;
					dataLen = sizeof(data);
					if (RegGetValueA(nwAdapters,subkeyName,"NetCfgInstanceId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
						instanceId.assign(data,dataLen);
						existingDeviceInstances.insert(instanceId);
					}

					std::string instanceIdPath;
					type = 0;
					dataLen = sizeof(data);
					if (RegGetValueA(nwAdapters,subkeyName,"DeviceInstanceID",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS)
						instanceIdPath.assign(data,dataLen);

					if ((_netCfgInstanceId.length() == 0)&&(instanceId.length() != 0)&&(instanceIdPath.length() != 0)) {
						type = 0;
						dataLen = sizeof(data);
						if (RegGetValueA(nwAdapters,subkeyName,"_ZeroTierTapIdentifier",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
							data[dataLen] = '\0';
							if (!strcmp(data,tag)) {
								_netCfgInstanceId = instanceId;
								_deviceInstanceId = instanceIdPath;
								mySubkeyName = subkeyName;
								break; // found it!
							}
						}
					}
				}
			}
		} else break; // no more subkeys or error occurred enumerating them
	}

	// If there is no device, try to create one
	if (_netCfgInstanceId.length() == 0) {
		// Log devcon output to a file
		HANDLE devconLog = CreateFileA((_r->homePath + "\\devcon.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
		if (devconLog == INVALID_HANDLE_VALUE) {
			LOG("WARNING: unable to open devcon.log");
		} else {
			SetFilePointer(devconLog,0,0,FILE_END);
		}

		// Execute devcon to install an instance of the Microsoft Loopback Adapter
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
		if (!CreateProcessA(NULL,(LPSTR)(std::string("\"") + _r->homePath + _winEnv.devcon + "\" install \"" + _r->homePath + _winEnv.tapDriver + "\" zttap200").c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
			RegCloseKey(nwAdapters);
			if (devconLog != INVALID_HANDLE_VALUE)
				CloseHandle(devconLog);
			throw std::runtime_error(std::string("unable to find or execute devcon at ") + _winEnv.devcon);
		}
		WaitForSingleObject(processInfo.hProcess,INFINITE);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);

		if (devconLog != INVALID_HANDLE_VALUE)
			CloseHandle(devconLog);

		// Scan for the new instance by simply looking for taps that weren't
		// there originally. The static mutex we lock ensures this can't step
		// on its own toes.
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
						type = 0;
						dataLen = sizeof(data);
						if (RegGetValueA(nwAdapters,subkeyName,"NetCfgInstanceId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
							if (existingDeviceInstances.count(std::string(data,dataLen)) == 0) {
								RegSetKeyValueA(nwAdapters,subkeyName,"_ZeroTierTapIdentifier",REG_SZ,tag,(DWORD)(strlen(tag)+1));
								_netCfgInstanceId.assign(data,dataLen);
								type = 0;
								dataLen = sizeof(data);
								if (RegGetValueA(nwAdapters,subkeyName,"DeviceInstanceID",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS)
									_deviceInstanceId.assign(data,dataLen);
								mySubkeyName = subkeyName;

								// Disable DHCP by default on newly created devices
								HKEY tcpIpInterfaces;
								if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\services\\Tcpip\\Parameters\\Interfaces",0,KEY_READ|KEY_WRITE,&tcpIpInterfaces) == ERROR_SUCCESS) {
									DWORD enable = 0;
									RegSetKeyValueA(tcpIpInterfaces,_netCfgInstanceId.c_str(),"EnableDHCP",REG_DWORD,&enable,sizeof(enable));
									RegCloseKey(tcpIpInterfaces);
								}

								break; // found it!
							}
						}
					}
				}
			} else break; // no more keys or error occurred
		}
	}

	if (_netCfgInstanceId.length() > 0) {
		char tmps[4096];
		unsigned int tmpsl = Utils::snprintf(tmps,sizeof(tmps),"%.2X-%.2X-%.2X-%.2X-%.2X-%.2X",(unsigned int)mac.data[0],(unsigned int)mac.data[1],(unsigned int)mac.data[2],(unsigned int)mac.data[3],(unsigned int)mac.data[4],(unsigned int)mac.data[5]) + 1;
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"NetworkAddress",REG_SZ,tmps,tmpsl);
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"MAC",REG_SZ,tmps,tmpsl);
		DWORD tmp = mtu;
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"MTU",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));
		tmp = 0;
		RegSetKeyValueA(nwAdapters,mySubkeyName.c_str(),"EnableDHCP",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));
	} else {
		RegCloseKey(nwAdapters);	
		throw std::runtime_error("unable to find or create tap adapter");
	}

	RegCloseKey(nwAdapters);	

	// Convert device GUID junk... blech... is there an easier way to do this?
	{
		char nobraces[128];
		const char *nbtmp1 = _netCfgInstanceId.c_str();
		char *nbtmp2 = nobraces;
		while (*nbtmp1) {
			if ((*nbtmp1 != '{')&&(*nbtmp1 != '}'))
				*nbtmp2++ = *nbtmp1;
			++nbtmp1;
		}
		*nbtmp2 = (char)0;
		if (UuidFromStringA((RPC_CSTR)nobraces,&_deviceGuid) != RPC_S_OK)
			throw std::runtime_error("unable to convert instance ID GUID to native GUID (invalid NetCfgInstanceId in registry?)");
	}

	// Disable and enable interface to ensure registry settings take effect
	_disableTapDevice(_r,_deviceInstanceId);
	if (!_enableTapDevice(_r,_deviceInstanceId))
		throw std::runtime_error("cannot enable tap device driver");

	// Open the tap, which is in this weird Windows analog of /dev
	char tapPath[4096];
	Utils::snprintf(tapPath,sizeof(tapPath),"\\\\.\\Global\\%s.tap",_netCfgInstanceId.c_str());
	for(int openTrials=0;;) {
		// Try multiple times, since there seem to be reports from the field
		// of driver init timing issues. Blech.
		_tap = CreateFileA(tapPath,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_SYSTEM|FILE_FLAG_OVERLAPPED,NULL);
		if (_tap == INVALID_HANDLE_VALUE) {
			if (++openTrials >= 3)
				throw std::runtime_error(std::string("unable to open tap device ")+tapPath);
			else Sleep(500);
		} else break;
	}

	setEnabled(true);
	if (!_enabled) {
		CloseHandle(_tap);
		throw std::runtime_error("cannot enable tap device using IOCTL");
	}

	// Initialized overlapped I/O structures and related events
	memset(&_tapOvlRead,0,sizeof(_tapOvlRead));
	_tapOvlRead.hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	memset(&_tapOvlWrite,0,sizeof(_tapOvlWrite));
	_tapOvlWrite.hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

	// Start background thread that actually performs I/O
	_injectSemaphore = CreateSemaphore(NULL,0,1,NULL);
	_thread = Thread::start(this);

	// Certain functions can now work (e.g. ips())
	_initialized = true;
}

WindowsEthernetTap::~WindowsEthernetTap()
{
	_run = false;

	ReleaseSemaphore(_injectSemaphore,1,NULL);
	Thread::join(_thread);

	CloseHandle(_tap);
	CloseHandle(_tapOvlRead.hEvent);
	CloseHandle(_tapOvlWrite.hEvent);
	CloseHandle(_injectSemaphore);

	_disableTapDevice(_r,_deviceInstanceId);
}

void WindowsEthernetTap::setEnabled(bool en)
{
	if (_tap == INVALID_HANDLE_VALUE) {
		_enabled = false;
	} else {
		uint32_t tmpi = (en ? 1 : 0);
		DWORD bytesReturned = 0;
		if (DeviceIoControl(_tap,TAP_WIN_IOCTL_SET_MEDIA_STATUS,&tmpi,sizeof(tmpi),&tmpi,sizeof(tmpi),&bytesReturned,NULL))
			_enabled = en;
		else _enabled = false;
	}
}

bool WindowsEthernetTap::enabled() const
{
	return _enabled;
}

void WindowsEthernetTap::setDisplayName(const char *dn)
{
	if (!_initialized)
		return;
	HKEY ifp;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,(std::string("SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\") + _netCfgInstanceId).c_str(),0,KEY_READ|KEY_WRITE,&ifp) == ERROR_SUCCESS) {
		RegSetKeyValueA(ifp,"Connection","Name",REG_SZ,(LPCVOID)dn,(DWORD)(strlen(dn)+1));
		RegCloseKey(ifp);
	}
}

bool WindowsEthernetTap::addIP(const InetAddress &ip)
{
	if (!_initialized)
		return false;
	if (!ip.netmaskBits()) // sanity check... netmask of 0.0.0.0 is WUT?
		return false;

	std::set<InetAddress> haveIps(ips());

	try {
		// Add IP to interface at the netlink level if not already assigned.
		if (!haveIps.count(ip)) {
			std::pair<NET_LUID,NET_IFINDEX> ifidx = _findAdapterByGuid(_deviceGuid);
			MIB_UNICASTIPADDRESS_ROW ipr;

			InitializeUnicastIpAddressEntry(&ipr);
			if (ip.isV4()) {
				ipr.Address.Ipv4.sin_family = AF_INET;
				ipr.Address.Ipv4.sin_addr.S_un.S_addr = *((const uint32_t *)ip.rawIpData());
				ipr.OnLinkPrefixLength = ip.port();
			} else if (ip.isV6()) {
				ipr.Address.Ipv6.sin6_family = AF_INET6;
				memcpy(ipr.Address.Ipv6.sin6_addr.u.Byte,ip.rawIpData(),16);
				ipr.OnLinkPrefixLength = ip.port();
			} else return false;

			ipr.PrefixOrigin = IpPrefixOriginManual;
			ipr.SuffixOrigin = IpSuffixOriginManual;
			ipr.ValidLifetime = 0xffffffff;
			ipr.PreferredLifetime = 0xffffffff;

			ipr.InterfaceLuid = ifidx.first;
			ipr.InterfaceIndex = ifidx.second;

			if (CreateUnicastIpAddressEntry(&ipr) == NO_ERROR) {
				haveIps.insert(ip);
			} else {
				LOG("unable to add IP address %s to interface %s: %d",ip.toString().c_str(),deviceName().c_str(),(int)GetLastError());
				return false;
			}
		}

		_syncIpsWithRegistry(haveIps,_netCfgInstanceId);
	} catch (std::exception &exc) {
		LOG("unexpected exception adding IP address %s to %s: %s",ip.toString().c_str(),deviceName().c_str(),exc.what());
	} catch ( ... ) {
		LOG("unexpected exception adding IP address %s to %s: unknown exception",ip.toString().c_str(),deviceName().c_str());
	}
	return false;
}

bool WindowsEthernetTap::removeIP(const InetAddress &ip)
{
	if (!_initialized)
		return false;
	try {
		MIB_UNICASTIPADDRESS_TABLE *ipt = (MIB_UNICASTIPADDRESS_TABLE *)0;
		std::pair<NET_LUID,NET_IFINDEX> ifidx = _findAdapterByGuid(_deviceGuid);
		if (GetUnicastIpAddressTable(AF_UNSPEC,&ipt) == NO_ERROR) {
			for(DWORD i=0;i<ipt->NumEntries;++i) {
				if ((ipt->Table[i].InterfaceLuid.Value == ifidx.first.Value)&&(ipt->Table[i].InterfaceIndex == ifidx.second)) {
					InetAddress addr;
					switch(ipt->Table[i].Address.si_family) {
						case AF_INET:
							addr.set(&(ipt->Table[i].Address.Ipv4.sin_addr.S_un.S_addr),4,ipt->Table[i].OnLinkPrefixLength);
							break;
						case AF_INET6:
							addr.set(ipt->Table[i].Address.Ipv6.sin6_addr.u.Byte,16,ipt->Table[i].OnLinkPrefixLength);
							if (addr.isLinkLocal())
								continue; // can't remove link-local IPv6 addresses
							break;
					}
					if (addr == ip) {
						DeleteUnicastIpAddressEntry(&(ipt->Table[i]));
						FreeMibTable(ipt);
						_syncIpsWithRegistry(ips(),_netCfgInstanceId);
						return true;
					}
				}
			}
			FreeMibTable((PVOID)ipt);
		}
	} catch (std::exception &exc) {
		LOG("unexpected exception removing IP address %s from %s: %s",ip.toString().c_str(),deviceName().c_str(),exc.what());
	} catch ( ... ) {
		LOG("unexpected exception removing IP address %s from %s: unknown exception",ip.toString().c_str(),deviceName().c_str());
	}
	return false;
}

std::set<InetAddress> WindowsEthernetTap::ips() const
{
	static const InetAddress linkLocalLoopback("fe80::1",64); // what is this and why does Windows assign it?
	std::set<InetAddress> addrs;

	if (!_initialized)
		return addrs;

	try {
		MIB_UNICASTIPADDRESS_TABLE *ipt = (MIB_UNICASTIPADDRESS_TABLE *)0;
		std::pair<NET_LUID,NET_IFINDEX> ifidx = _findAdapterByGuid(_deviceGuid);

		if (GetUnicastIpAddressTable(AF_UNSPEC,&ipt) == NO_ERROR) {
			for(DWORD i=0;i<ipt->NumEntries;++i) {
				if ((ipt->Table[i].InterfaceLuid.Value == ifidx.first.Value)&&(ipt->Table[i].InterfaceIndex == ifidx.second)) {
					switch(ipt->Table[i].Address.si_family) {
						case AF_INET: {
							InetAddress ip(&(ipt->Table[i].Address.Ipv4.sin_addr.S_un.S_addr),4,ipt->Table[i].OnLinkPrefixLength);
							if (ip != InetAddress::LO4)
								addrs.insert(ip);
						}	break;
						case AF_INET6: {
							InetAddress ip(ipt->Table[i].Address.Ipv6.sin6_addr.u.Byte,16,ipt->Table[i].OnLinkPrefixLength);
							if ((ip != linkLocalLoopback)&&(ip != InetAddress::LO6))
								addrs.insert(ip);
						}	break;
					}
				}
			}
			FreeMibTable(ipt);
		}
	} catch ( ... ) {} // sanity check, shouldn't happen unless out of memory

	return addrs;
}

void WindowsEthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	if ((!_initialized)||(!_enabled))
		return;
	if (len > (ZT_IF_MTU))
		return; // sanity check

	{
		Mutex::Lock _l(_injectPending_m);
		_injectPending.push( std::pair<Array<char,ZT_IF_MTU + 32>,unsigned int>(Array<char,ZT_IF_MTU + 32>(),len + 14) );
		char *d = _injectPending.back().first.data;
		memcpy(d,to.data,6);
		memcpy(d + 6,from.data,6);
		d[12] = (char)((etherType >> 8) & 0xff);
		d[13] = (char)(etherType & 0xff);
		memcpy(d + 14,data,len);
	}

	ReleaseSemaphore(_injectSemaphore,1,NULL);
}

std::string WindowsEthernetTap::deviceName() const
{
	return _netCfgInstanceId;
}

std::string WindowsEthernetTap::persistentId() const
{
	return _deviceInstanceId;
}

bool WindowsEthernetTap::updateMulticastGroups(std::set<MulticastGroup> &groups)
{
	if (!_initialized)
		return false;

	std::set<MulticastGroup> newGroups;

	// Ensure that groups are added for each IP... this handles the MAC:ADI
	// groups that are created from IPv4 addresses. Some of these may end
	// up being duplicates of what the IOCTL returns but that's okay since
	// the set<> will filter that.
	std::set<InetAddress> ipaddrs(ips());
	for(std::set<InetAddress>::const_iterator i(ipaddrs.begin());i!=ipaddrs.end();++i)
		newGroups.insert(MulticastGroup::deriveMulticastGroupForAddressResolution(*i));

	// The ZT1 tap driver supports an IOCTL to get multicast memberships at the L2
	// level... something Windows does not seem to expose ordinarily. This lets
	// pretty much anything work... IPv4, IPv6, IPX, oldskool Netbios, who knows...
	unsigned char mcastbuf[TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS_OUTPUT_BUF_SIZE];
	DWORD bytesReturned = 0;
	if (DeviceIoControl(_tap,TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS,(LPVOID)0,0,(LPVOID)mcastbuf,sizeof(mcastbuf),&bytesReturned,NULL)) {
		MAC mac;
		DWORD i = 0;
		while ((i + 6) <= bytesReturned) {
			mac.data[0] = mcastbuf[i++];
			mac.data[1] = mcastbuf[i++];
			mac.data[2] = mcastbuf[i++];
			mac.data[3] = mcastbuf[i++];
			mac.data[4] = mcastbuf[i++];
			mac.data[5] = mcastbuf[i++];
			if (mac.isMulticast()) {
				// exclude the nulls that may be returned or any other junk Windows puts in there
				newGroups.insert(MulticastGroup(mac,0));
			}
		}
	}

	newGroups.insert(_blindWildcardMulticastGroup); // always join this

	bool changed = false;

	for(std::set<MulticastGroup>::iterator mg(newGroups.begin());mg!=newGroups.end();++mg) {
		if (!groups.count(*mg)) {
			groups.insert(*mg);
			changed = true;
		}
	}
	for(std::set<MulticastGroup>::iterator mg(groups.begin());mg!=groups.end();) {
		if (!newGroups.count(*mg)) {
			groups.erase(mg++);
			changed = true;
		} else ++mg;
	}

	return changed;
}

void WindowsEthernetTap::threadMain()
	throw()
{
	HANDLE wait4[3];
	wait4[0] = _injectSemaphore;
	wait4[1] = _tapOvlRead.hEvent;
	wait4[2] = _tapOvlWrite.hEvent; // only included if writeInProgress is true

	ReadFile(_tap,_tapReadBuf,sizeof(_tapReadBuf),NULL,&_tapOvlRead);
	bool writeInProgress = false;

	for(;;) {
		// Windows can DIAF
		if (_enabled)
			setEnabled(true);

		if (!_run) break;
		DWORD r = WaitForMultipleObjectsEx(writeInProgress ? 3 : 2,wait4,FALSE,INFINITE,TRUE);
		if (!_run) break;

		if (HasOverlappedIoCompleted(&_tapOvlRead)) {
			DWORD bytesRead = 0;
			if (GetOverlappedResult(_tap,&_tapOvlRead,&bytesRead,FALSE)) {
				if ((bytesRead > 14)&&(_enabled)) {
					MAC to(_tapReadBuf);
					MAC from(_tapReadBuf + 6);
					unsigned int etherType = ((((unsigned int)_tapReadBuf[12]) & 0xff) << 8) | (((unsigned int)_tapReadBuf[13]) & 0xff);
					try {
						Buffer<4096> tmp(_tapReadBuf + 14,bytesRead - 14);
						_handler(_arg,from,to,etherType,tmp);
					} catch ( ... ) {} // handlers should not throw
				}
			}
			ReadFile(_tap,_tapReadBuf,sizeof(_tapReadBuf),NULL,&_tapOvlRead);
		}

		if (writeInProgress) {
			if (HasOverlappedIoCompleted(&_tapOvlWrite)) {
				writeInProgress = false;
				_injectPending_m.lock();
				_injectPending.pop();
			} else continue; // still writing, so skip code below and wait
		} else _injectPending_m.lock();

		if (!_injectPending.empty()) {
			WriteFile(_tap,_injectPending.front().first.data,_injectPending.front().second,NULL,&_tapOvlWrite);
			writeInProgress = true;
		}

		_injectPending_m.unlock();
	}

	CancelIo(_tap);
}

bool WindowsEthernetTap::deletePersistentTapDevice(const RuntimeEnvironment *_r,const char *pid)
{
	Mutex::Lock _l(_systemTapInitLock); // only one thread may mess with taps at a time, process-wide

	HANDLE devconLog = CreateFileA((_r->homePath + "\\devcon.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
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
	if (CreateProcessA(NULL,(LPSTR)(std::string("\"") + _r->homePath + _winEnv.devcon + "\" remove @" + pid).c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
		WaitForSingleObject(processInfo.hProcess,INFINITE);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
		if (devconLog != INVALID_HANDLE_VALUE)
			CloseHandle(devconLog);
		return true;
	}
	if (devconLog != INVALID_HANDLE_VALUE)
		CloseHandle(devconLog);

	return false;
}

int WindowsEthernetTap::cleanPersistentTapDevices(const RuntimeEnvironment *_r,const std::set<std::string> &exceptThese,bool alsoRemoveUnassociatedDevices)
{
	char subkeyName[4096];
	char subkeyClass[4096];
	char data[4096];

	std::set<std::string> instanceIdPathsToRemove;
	{
		Mutex::Lock _l(_systemTapInitLock); // only one thread may mess with taps at a time, process-wide

		HKEY nwAdapters;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}",0,KEY_READ|KEY_WRITE,&nwAdapters) != ERROR_SUCCESS)
			return -1;

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
						if (instanceIdPath.length() != 0) {
							type = 0;
							dataLen = sizeof(data);
							if (RegGetValueA(nwAdapters,subkeyName,"_ZeroTierTapIdentifier",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
								if (dataLen <= 0) {
									if (alsoRemoveUnassociatedDevices)
										instanceIdPathsToRemove.insert(instanceIdPath);
								} else {
									if (!exceptThese.count(std::string(data,dataLen)))
										instanceIdPathsToRemove.insert(instanceIdPath);
								}
							} else if (alsoRemoveUnassociatedDevices)
								instanceIdPathsToRemove.insert(instanceIdPath);
						}
					}
				}
			} else break; // end of list or failure
		}

		RegCloseKey(nwAdapters);
	}

	int removed = 0;
	for(std::set<std::string>::iterator iidp(instanceIdPathsToRemove.begin());iidp!=instanceIdPathsToRemove.end();++iidp) {
		if (deletePersistentTapDevice(_r,iidp->c_str()))
			++removed;
	}
	return removed;
}

} // namespace ZeroTier
