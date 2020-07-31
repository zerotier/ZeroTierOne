/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#define _WIN32_WINNT 0x06010000

#include "../core/Constants.hpp"

#ifdef __WINDOWS__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <WinSock2.h>
#include <Windows.h>
#include <tchar.h>
#include <malloc.h>
#include <winreg.h>
#include <wchar.h>
#include <ws2ipdef.h>
#include <WS2tcpip.h>
#include <IPHlpApi.h>
#include <nldef.h>
#include <netioapi.h>
#include <ipmib.h>
//#include <atlbase.h>
#include <netlistmgr.h>
#include <nldef.h>
#include <SetupAPI.h>
#include <newdev.h>
#include <cfgmgr32.h>

#include <iostream>
#include <set>

#include "../core/Utils.hpp"
#include "../core/Mutex.hpp"

#include "WindowsEthernetTap.hpp"
#include "OSUtils.hpp"

#include "../installsupport/windows/tap-windows-ndis6/src/tap-windows-ndis6/tap-windows.h"

#include <netcon.h>

// Create a fake unused default route to force detection of network type on networks without gateways
#define ZT_WINDOWS_CREATE_FAKE_DEFAULT_ROUTE

// Function signatures of dynamically loaded functions, from newdev.h, setupapi.h, and cfgmgr32.h
typedef BOOL (WINAPI *UpdateDriverForPlugAndPlayDevicesA_t)(_In_opt_ HWND hwndParent,_In_ LPCSTR HardwareId,_In_ LPCSTR FullInfPath,_In_ DWORD InstallFlags,_Out_opt_ PBOOL bRebootRequired);
typedef BOOL (WINAPI *SetupDiGetINFClassA_t)(_In_ PCSTR InfName,_Out_ LPGUID ClassGuid,_Out_writes_(ClassNameSize) PSTR ClassName,_In_ DWORD ClassNameSize,_Out_opt_ PDWORD RequiredSize);
typedef HDEVINFO (WINAPI *SetupDiCreateDeviceInfoList_t)(_In_opt_ CONST GUID *ClassGuid,_In_opt_ HWND hwndParent);
typedef BOOL (WINAPI *SetupDiCreateDeviceInfoA_t)(_In_ HDEVINFO DeviceInfoSet,_In_ PCSTR DeviceName,_In_ CONST GUID *ClassGuid,_In_opt_ PCSTR DeviceDescription,_In_opt_ HWND hwndParent,_In_ DWORD CreationFlags,_Out_opt_ PSP_DEVINFO_DATA DeviceInfoData);
typedef BOOL (WINAPI *SetupDiSetDeviceRegistryPropertyA_t)(_In_ HDEVINFO DeviceInfoSet,_Inout_ PSP_DEVINFO_DATA DeviceInfoData,_In_ DWORD Property,_In_reads_bytes_opt_(PropertyBufferSize) CONST BYTE *PropertyBuffer,_In_ DWORD PropertyBufferSize);
typedef BOOL (WINAPI *SetupDiCallClassInstaller_t)(_In_ DI_FUNCTION InstallFunction,_In_ HDEVINFO DeviceInfoSet,_In_opt_ PSP_DEVINFO_DATA DeviceInfoData);
typedef BOOL (WINAPI *SetupDiDestroyDeviceInfoList_t)(_In_ HDEVINFO DeviceInfoSet);
typedef HDEVINFO (WINAPI *SetupDiGetClassDevsExA_t)(_In_opt_ CONST GUID *ClassGuid,_In_opt_ PCSTR Enumerator,_In_opt_ HWND hwndParent,_In_ DWORD Flags,_In_opt_ HDEVINFO DeviceInfoSet,_In_opt_ PCSTR MachineName,_Reserved_ PVOID Reserved);
typedef BOOL (WINAPI *SetupDiOpenDeviceInfoA_t)(_In_ HDEVINFO DeviceInfoSet,_In_ PCSTR DeviceInstanceId,_In_opt_ HWND hwndParent,_In_ DWORD OpenFlags,_Out_opt_ PSP_DEVINFO_DATA DeviceInfoData);
typedef BOOL (WINAPI *SetupDiEnumDeviceInfo_t)(_In_ HDEVINFO DeviceInfoSet,_In_ DWORD MemberIndex,_Out_ PSP_DEVINFO_DATA DeviceInfoData);
typedef BOOL (WINAPI *SetupDiSetClassInstallParamsA_t)(_In_ HDEVINFO DeviceInfoSet,_In_opt_ PSP_DEVINFO_DATA DeviceInfoData,_In_reads_bytes_opt_(ClassInstallParamsSize) PSP_CLASSINSTALL_HEADER ClassInstallParams,_In_ DWORD ClassInstallParamsSize);
typedef CONFIGRET (WINAPI *CM_Get_Device_ID_ExA_t)(_In_ DEVINST dnDevInst,_Out_writes_(BufferLen) PSTR Buffer,_In_ ULONG BufferLen,_In_ ULONG ulFlags,_In_opt_ HMACHINE hMachine);
typedef BOOL (WINAPI *SetupDiGetDeviceInstanceIdA_t)(_In_ HDEVINFO DeviceInfoSet,_In_ PSP_DEVINFO_DATA DeviceInfoData,_Out_writes_opt_(DeviceInstanceIdSize) PSTR DeviceInstanceId,_In_ DWORD DeviceInstanceIdSize,_Out_opt_ PDWORD RequiredSize);

namespace ZeroTier {

namespace {

// Static/singleton class that when initialized loads a bunch of environment information and a few dynamically loaded DLLs
class WindowsEthernetTapEnv
{
public:
	WindowsEthernetTapEnv()
	{
#ifdef _WIN64
		is64Bit = TRUE;
		tapDriverPath = "\\tap-windows\\x64\\zttap300.inf";
#else
		is64Bit = FALSE;
		IsWow64Process(GetCurrentProcess(),&is64Bit);
		if (is64Bit) {
			fprintf(stderr,"FATAL: you must use the 64-bit ZeroTier One service on 64-bit Windows systems\r\n");
			_exit(1);
		}
		tapDriverPath = "\\tap-windows\\x86\\zttap300.inf";
#endif
		tapDriverName = "zttap300";

		setupApiMod = LoadLibraryA("setupapi.dll");
		if (!setupApiMod) {
			fprintf(stderr,"FATAL: unable to dynamically load setupapi.dll\r\n");
			_exit(1);
		}
		if (!(this->SetupDiGetINFClassA = (SetupDiGetINFClassA_t)GetProcAddress(setupApiMod,"SetupDiGetINFClassA"))) {
			fprintf(stderr,"FATAL: SetupDiGetINFClassA not found in setupapi.dll\r\n");
			_exit(1);
		}
		if (!(this->SetupDiCreateDeviceInfoList = (SetupDiCreateDeviceInfoList_t)GetProcAddress(setupApiMod,"SetupDiCreateDeviceInfoList"))) {
			fprintf(stderr,"FATAL: SetupDiCreateDeviceInfoList not found in setupapi.dll\r\n");
			_exit(1);
		}
		if (!(this->SetupDiCreateDeviceInfoA = (SetupDiCreateDeviceInfoA_t)GetProcAddress(setupApiMod,"SetupDiCreateDeviceInfoA"))) {
			fprintf(stderr,"FATAL: SetupDiCreateDeviceInfoA not found in setupapi.dll\r\n");
			_exit(1);
		}
		if (!(this->SetupDiSetDeviceRegistryPropertyA = (SetupDiSetDeviceRegistryPropertyA_t)GetProcAddress(setupApiMod,"SetupDiSetDeviceRegistryPropertyA"))) {
			fprintf(stderr,"FATAL: SetupDiSetDeviceRegistryPropertyA not found in setupapi.dll\r\n");
			_exit(1);
		}
		if (!(this->SetupDiCallClassInstaller = (SetupDiCallClassInstaller_t)GetProcAddress(setupApiMod,"SetupDiCallClassInstaller"))) {
			fprintf(stderr,"FATAL: SetupDiCallClassInstaller not found in setupapi.dll\r\n");
			_exit(1);
		}
		if (!(this->SetupDiDestroyDeviceInfoList = (SetupDiDestroyDeviceInfoList_t)GetProcAddress(setupApiMod,"SetupDiDestroyDeviceInfoList"))) {
			fprintf(stderr,"FATAL: SetupDiDestroyDeviceInfoList not found in setupapi.dll\r\n");
			_exit(1);
		}
		if (!(this->SetupDiGetClassDevsExA = (SetupDiGetClassDevsExA_t)GetProcAddress(setupApiMod,"SetupDiGetClassDevsExA"))) {
			fprintf(stderr,"FATAL: SetupDiGetClassDevsExA not found in setupapi.dll\r\n");
			_exit(1);
		}
		if (!(this->SetupDiOpenDeviceInfoA = (SetupDiOpenDeviceInfoA_t)GetProcAddress(setupApiMod,"SetupDiOpenDeviceInfoA"))) {
			fprintf(stderr,"FATAL: SetupDiOpenDeviceInfoA not found in setupapi.dll\r\n");
			_exit(1);
		}
		if (!(this->SetupDiEnumDeviceInfo = (SetupDiEnumDeviceInfo_t)GetProcAddress(setupApiMod,"SetupDiEnumDeviceInfo"))) {
			fprintf(stderr,"FATAL: SetupDiEnumDeviceInfo not found in setupapi.dll\r\n");
			_exit(1);
		}
		if (!(this->SetupDiSetClassInstallParamsA = (SetupDiSetClassInstallParamsA_t)GetProcAddress(setupApiMod,"SetupDiSetClassInstallParamsA"))) {
			fprintf(stderr,"FATAL: SetupDiSetClassInstallParamsA not found in setupapi.dll\r\n");
			_exit(1);
		}
		if (!(this->SetupDiGetDeviceInstanceIdA = (SetupDiGetDeviceInstanceIdA_t)GetProcAddress(setupApiMod,"SetupDiGetDeviceInstanceIdA"))) {
			fprintf(stderr,"FATAL: SetupDiGetDeviceInstanceIdA not found in setupapi.dll\r\n");
			_exit(1);
		}

		newDevMod = LoadLibraryA("newdev.dll");
		if (!newDevMod) {
			fprintf(stderr,"FATAL: unable to dynamically load newdev.dll\r\n");
			_exit(1);
		}
		if (!(this->UpdateDriverForPlugAndPlayDevicesA = (UpdateDriverForPlugAndPlayDevicesA_t)GetProcAddress(newDevMod,"UpdateDriverForPlugAndPlayDevicesA"))) {
			fprintf(stderr,"FATAL: UpdateDriverForPlugAndPlayDevicesA not found in newdev.dll\r\n");
			_exit(1);
		}

		cfgMgrMod = LoadLibraryA("cfgmgr32.dll");
		if (!cfgMgrMod) {
			fprintf(stderr,"FATAL: unable to dynamically load cfgmgr32.dll\r\n");
			_exit(1);
		}
		if (!(this->CM_Get_Device_ID_ExA = (CM_Get_Device_ID_ExA_t)GetProcAddress(cfgMgrMod,"CM_Get_Device_ID_ExA"))) {
			fprintf(stderr,"FATAL: CM_Get_Device_ID_ExA not found in cfgmgr32.dll\r\n");
			_exit(1);
		}
	}

	BOOL is64Bit; // is the system 64-bit, regardless of whether this binary is or not
	std::string tapDriverPath;
	std::string tapDriverName;

	UpdateDriverForPlugAndPlayDevicesA_t UpdateDriverForPlugAndPlayDevicesA;

	SetupDiGetINFClassA_t SetupDiGetINFClassA;
	SetupDiCreateDeviceInfoList_t SetupDiCreateDeviceInfoList;
	SetupDiCreateDeviceInfoA_t SetupDiCreateDeviceInfoA;
	SetupDiSetDeviceRegistryPropertyA_t SetupDiSetDeviceRegistryPropertyA;
	SetupDiCallClassInstaller_t SetupDiCallClassInstaller;
	SetupDiDestroyDeviceInfoList_t SetupDiDestroyDeviceInfoList;
	SetupDiGetClassDevsExA_t SetupDiGetClassDevsExA;
	SetupDiOpenDeviceInfoA_t SetupDiOpenDeviceInfoA;
	SetupDiEnumDeviceInfo_t SetupDiEnumDeviceInfo;
	SetupDiSetClassInstallParamsA_t SetupDiSetClassInstallParamsA;
	SetupDiGetDeviceInstanceIdA_t SetupDiGetDeviceInstanceIdA;

	CM_Get_Device_ID_ExA_t CM_Get_Device_ID_ExA;

private:
	HMODULE setupApiMod;
	HMODULE newDevMod;
	HMODULE cfgMgrMod;
};
static const WindowsEthernetTapEnv WINENV;

// Only create or delete devices one at a time
static Mutex _systemTapInitLock;

// Only perform installation or uninstallation options one at a time
static Mutex _systemDeviceManagementLock;

} // anonymous namespace

std::string WindowsEthernetTap::addNewPersistentTapDevice(const char *pathToInf,std::string &deviceInstanceId)
{
	Mutex::Lock _l(_systemDeviceManagementLock);

	GUID classGuid;
	char className[1024];
	if (!WINENV.SetupDiGetINFClassA(pathToInf,&classGuid,className,sizeof(className),(PDWORD)0)) {
		return std::string("SetupDiGetINFClassA() failed -- unable to read zttap driver INF file");
	}

	HDEVINFO deviceInfoSet = WINENV.SetupDiCreateDeviceInfoList(&classGuid,(HWND)0);
	if (deviceInfoSet == INVALID_HANDLE_VALUE) {
		return std::string("SetupDiCreateDeviceInfoList() failed");
	}

	SP_DEVINFO_DATA deviceInfoData;
	memset(&deviceInfoData,0,sizeof(deviceInfoData));
	deviceInfoData.cbSize = sizeof(deviceInfoData);
	if (!WINENV.SetupDiCreateDeviceInfoA(deviceInfoSet,className,&classGuid,(PCSTR)0,(HWND)0,DICD_GENERATE_ID,&deviceInfoData)) {
		WINENV.SetupDiDestroyDeviceInfoList(deviceInfoSet);
		return std::string("SetupDiCreateDeviceInfoA() failed");
	}

	if (!WINENV.SetupDiSetDeviceRegistryPropertyA(deviceInfoSet,&deviceInfoData,SPDRP_HARDWAREID,(const BYTE *)WINENV.tapDriverName.c_str(),(DWORD)(WINENV.tapDriverName.length() + 1))) {
		WINENV.SetupDiDestroyDeviceInfoList(deviceInfoSet);
		return std::string("SetupDiSetDeviceRegistryPropertyA() failed");
	}

	if (!WINENV.SetupDiCallClassInstaller(DIF_REGISTERDEVICE,deviceInfoSet,&deviceInfoData)) {
		WINENV.SetupDiDestroyDeviceInfoList(deviceInfoSet);
		return std::string("SetupDiCallClassInstaller(DIF_REGISTERDEVICE) failed");
	}

	// HACK: During upgrades, this can fail while the installer is still running. So make 60 attempts
	// with a 1s delay between each attempt.
	bool driverInstalled = false;
	for(int retryCounter=0;retryCounter<60;++retryCounter) {
		BOOL rebootRequired = FALSE;
		if (WINENV.UpdateDriverForPlugAndPlayDevicesA((HWND)0,WINENV.tapDriverName.c_str(),pathToInf,INSTALLFLAG_FORCE|INSTALLFLAG_NONINTERACTIVE,&rebootRequired)) {
			driverInstalled = true;
			break;
		} else Sleep(1000);
	}
	if (!driverInstalled) {
		WINENV.SetupDiDestroyDeviceInfoList(deviceInfoSet);
		return std::string("UpdateDriverForPlugAndPlayDevices() failed (made 60 attempts)");
	}

	char iidbuf[1024];
	DWORD iidReqSize = sizeof(iidbuf);
	if (WINENV.SetupDiGetDeviceInstanceIdA(deviceInfoSet,&deviceInfoData,iidbuf,sizeof(iidbuf),&iidReqSize)) {
		deviceInstanceId = iidbuf;
	} // failure here is not fatal since we only need this on Vista and 2008 -- other versions fill it into the registry automatically

	WINENV.SetupDiDestroyDeviceInfoList(deviceInfoSet);

	return std::string();
}

std::string WindowsEthernetTap::destroyAllLegacyPersistentTapDevices()
{
	char subkeyName[1024];
	char subkeyClass[1024];
	char data[1024];

	std::set<std::string> instanceIdPathsToRemove;
	{
		HKEY nwAdapters;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}",0,KEY_READ|KEY_WRITE,&nwAdapters) != ERROR_SUCCESS)
			return std::string("Could not open registry key");

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

					if ((!strnicmp(data,"zttap",5))&&(WINENV.tapDriverName != data)) {
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

	std::string errlist;
	for(std::set<std::string>::iterator iidp(instanceIdPathsToRemove.begin());iidp!=instanceIdPathsToRemove.end();++iidp) {
		std::string err = deletePersistentTapDevice(iidp->c_str());
		if (err.length() > 0) {
			if (errlist.length() > 0)
				errlist.push_back(',');
			errlist.append(err);
		}
	}
	return errlist;
}

std::string WindowsEthernetTap::destroyAllPersistentTapDevices()
{
	char subkeyName[1024];
	char subkeyClass[1024];
	char data[1024];

	std::set<std::string> instanceIdPathsToRemove;
	{
		HKEY nwAdapters;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}",0,KEY_READ|KEY_WRITE,&nwAdapters) != ERROR_SUCCESS)
			return std::string("Could not open registry key");

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

	std::string errlist;
	for(std::set<std::string>::iterator iidp(instanceIdPathsToRemove.begin());iidp!=instanceIdPathsToRemove.end();++iidp) {
		std::string err = deletePersistentTapDevice(iidp->c_str());
		if (err.length() > 0) {
			if (errlist.length() > 0)
				errlist.push_back(',');
			errlist.append(err);
		}
	}
	return errlist;
}

std::string WindowsEthernetTap::deletePersistentTapDevice(const char *instanceId)
{
	char iid[256];
	SP_REMOVEDEVICE_PARAMS rmdParams;

	memset(&rmdParams,0,sizeof(rmdParams));
	rmdParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
	rmdParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
	rmdParams.Scope = DI_REMOVEDEVICE_GLOBAL;
	rmdParams.HwProfile = 0;

	Mutex::Lock _l(_systemDeviceManagementLock);

	HDEVINFO devInfo = WINENV.SetupDiGetClassDevsExA((const GUID *)0,(PCSTR)0,(HWND)0,DIGCF_ALLCLASSES,(HDEVINFO)0,(PCSTR)0,(PVOID)0);
	if (devInfo == INVALID_HANDLE_VALUE)
		return std::string("SetupDiGetClassDevsExA() failed");
	WINENV.SetupDiOpenDeviceInfoA(devInfo,instanceId,(HWND)0,0,(PSP_DEVINFO_DATA)0);

	SP_DEVINFO_DATA devInfoData;
	memset(&devInfoData,0,sizeof(devInfoData));
	devInfoData.cbSize = sizeof(devInfoData);
	for(DWORD devIndex=0;WINENV.SetupDiEnumDeviceInfo(devInfo,devIndex,&devInfoData);devIndex++) {
		if ((WINENV.CM_Get_Device_ID_ExA(devInfoData.DevInst,iid,sizeof(iid),0,(HMACHINE)0) == CR_SUCCESS)&&(!strcmp(iid,instanceId))) {
			if (!WINENV.SetupDiSetClassInstallParamsA(devInfo,&devInfoData,&rmdParams.ClassInstallHeader,sizeof(rmdParams))) {
				WINENV.SetupDiDestroyDeviceInfoList(devInfo);
				return std::string("SetupDiSetClassInstallParams() failed");
			}

			if (!WINENV.SetupDiCallClassInstaller(DIF_REMOVE,devInfo,&devInfoData)) {
				WINENV.SetupDiDestroyDeviceInfoList(devInfo);
				return std::string("SetupDiCallClassInstaller(DIF_REMOVE) failed");
			}

			WINENV.SetupDiDestroyDeviceInfoList(devInfo);
			return std::string();
		}
	}

	WINENV.SetupDiDestroyDeviceInfoList(devInfo);
	return std::string("instance ID not found");
}

bool WindowsEthernetTap::setPersistentTapDeviceState(const char *instanceId,bool enabled)
{
	char iid[256];
	SP_PROPCHANGE_PARAMS params;

	Mutex::Lock _l(_systemDeviceManagementLock);

	HDEVINFO devInfo = WINENV.SetupDiGetClassDevsExA((const GUID *)0,(PCSTR)0,(HWND)0,DIGCF_ALLCLASSES,(HDEVINFO)0,(PCSTR)0,(PVOID)0);
	if (devInfo == INVALID_HANDLE_VALUE)
		return false;
	WINENV.SetupDiOpenDeviceInfoA(devInfo,instanceId,(HWND)0,0,(PSP_DEVINFO_DATA)0);

	SP_DEVINFO_DATA devInfoData;
	memset(&devInfoData,0,sizeof(devInfoData));
	devInfoData.cbSize = sizeof(devInfoData);
	for(DWORD devIndex=0;WINENV.SetupDiEnumDeviceInfo(devInfo,devIndex,&devInfoData);devIndex++) {
		if ((WINENV.CM_Get_Device_ID_ExA(devInfoData.DevInst,iid,sizeof(iid),0,(HMACHINE)0) == CR_SUCCESS)&&(!strcmp(iid,instanceId))) {
			memset(&params,0,sizeof(params));
			params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
			params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
			params.StateChange = enabled ? DICS_ENABLE : DICS_DISABLE;
			params.Scope = DICS_FLAG_GLOBAL;
			params.HwProfile = 0;

			WINENV.SetupDiSetClassInstallParamsA(devInfo,&devInfoData,&params.ClassInstallHeader,sizeof(params));
			WINENV.SetupDiCallClassInstaller(DIF_PROPERTYCHANGE,devInfo,&devInfoData);

			memset(&params,0,sizeof(params));
			params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
			params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
			params.StateChange = enabled ? DICS_ENABLE : DICS_DISABLE;
			params.Scope = DICS_FLAG_CONFIGSPECIFIC;
			params.HwProfile = 0;

			WINENV.SetupDiSetClassInstallParamsA(devInfo,&devInfoData,&params.ClassInstallHeader,sizeof(params));
			WINENV.SetupDiCallClassInstaller(DIF_PROPERTYCHANGE,devInfo,&devInfoData);

			WINENV.SetupDiDestroyDeviceInfoList(devInfo);
			return true;
		}
	}

	WINENV.SetupDiDestroyDeviceInfoList(devInfo);
	return false;
}

WindowsEthernetTap::WindowsEthernetTap(
	const char *hp,
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *friendlyName,
	void (*handler)(void *,void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
	void *arg) :
	_handler(handler),
	_arg(arg),
	_mac(mac),
	_nwid(nwid),
	_mtu(mtu),
	_tap(INVALID_HANDLE_VALUE),
	_friendlyName(friendlyName),
	_injectSemaphore(INVALID_HANDLE_VALUE),
	_pathToHelpers(hp),
	_run(true),
	_initialized(false),
	_enabled(true)
{
	char subkeyName[1024];
	char subkeyClass[1024];
	char data[1024];
	char tag[24];

	// We "tag" registry entries with the network ID to identify persistent devices
	OSUtils::ztsnprintf(tag,sizeof(tag),"%.16llx",(unsigned long long)nwid);

	Mutex::Lock _l(_systemTapInitLock);

	HKEY nwAdapters;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}",0,KEY_READ|KEY_WRITE,&nwAdapters) != ERROR_SUCCESS)
		throw std::runtime_error("unable to open registry key for network adapter enumeration");

	// Look for the tap instance that corresponds with this network
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
				data[dataLen] = (char)0;

				if (WINENV.tapDriverName == data) {
					std::string instanceId;
					type = 0;
					dataLen = sizeof(data);
					if (RegGetValueA(nwAdapters,subkeyName,"NetCfgInstanceId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS)
						instanceId.assign(data,dataLen);

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

								_mySubkeyName = subkeyName;
								break; // found it!
							}
						}
					}
				}
			}
		} else break; // no more subkeys or error occurred enumerating them
	}

	// If there is no device, try to create one
	bool creatingNewDevice = (_netCfgInstanceId.length() == 0);
	std::string newDeviceInstanceId;
	if (creatingNewDevice) {
		for(int getNewAttemptCounter=0;getNewAttemptCounter<2;++getNewAttemptCounter) {
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

						if (WINENV.tapDriverName == data) {
							type = 0;
							dataLen = sizeof(data);
							if ((RegGetValueA(nwAdapters,subkeyName,"_ZeroTierTapIdentifier",RRF_RT_ANY,&type,(PVOID)data,&dataLen) != ERROR_SUCCESS)||(dataLen <= 0)) {
								type = 0;
								dataLen = sizeof(data);
								if (RegGetValueA(nwAdapters,subkeyName,"NetCfgInstanceId",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS) {
									RegSetKeyValueA(nwAdapters,subkeyName,"_ZeroTierTapIdentifier",REG_SZ,tag,(DWORD)(strlen(tag)+1));

									_netCfgInstanceId.assign(data,dataLen);

									type = 0;
									dataLen = sizeof(data);
									if (RegGetValueA(nwAdapters,subkeyName,"DeviceInstanceID",RRF_RT_ANY,&type,(PVOID)data,&dataLen) == ERROR_SUCCESS)
										_deviceInstanceId.assign(data,dataLen);

									_mySubkeyName = subkeyName;

									// Disable DHCP by default on new devices
									HKEY tcpIpInterfaces;
									if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\services\\Tcpip\\Parameters\\Interfaces",0,KEY_READ|KEY_WRITE,&tcpIpInterfaces) == ERROR_SUCCESS) {
										DWORD enable = 0;
										RegSetKeyValueA(tcpIpInterfaces,_netCfgInstanceId.c_str(),"EnableDHCP",REG_DWORD,&enable,sizeof(enable));
										RegCloseKey(tcpIpInterfaces);
									}

									break; // found an unused zttap device
								}
							}
						}
					}
				} else break; // no more keys or error occurred
			}

			if (_netCfgInstanceId.length() > 0) {
				break; // found an unused zttap device
			} else {
				// no unused zttap devices, so create one
				std::string errm = addNewPersistentTapDevice((std::string(_pathToHelpers) + WINENV.tapDriverPath).c_str(),newDeviceInstanceId);
				if (errm.length() > 0)
					throw std::runtime_error(std::string("unable to create new device instance: ")+errm);
			}
		}
	}

	if (_netCfgInstanceId.length() > 0) {
		char tmps[64];
		unsigned int tmpsl = OSUtils::ztsnprintf(tmps,sizeof(tmps),"%.2X-%.2X-%.2X-%.2X-%.2X-%.2X",(unsigned int)mac[0],(unsigned int)mac[1],(unsigned int)mac[2],(unsigned int)mac[3],(unsigned int)mac[4],(unsigned int)mac[5]) + 1;
		RegSetKeyValueA(nwAdapters,_mySubkeyName.c_str(),"NetworkAddress",REG_SZ,tmps,tmpsl);
		RegSetKeyValueA(nwAdapters,_mySubkeyName.c_str(),"MAC",REG_SZ,tmps,tmpsl);
		tmpsl = OSUtils::ztsnprintf(tmps, sizeof(tmps), "%d", mtu);
		RegSetKeyValueA(nwAdapters,_mySubkeyName.c_str(),"MTU",REG_SZ,tmps,tmpsl);

		DWORD tmp = 0;
		RegSetKeyValueA(nwAdapters,_mySubkeyName.c_str(),"*NdisDeviceType",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));
		tmp = IF_TYPE_ETHERNET_CSMACD;
		RegSetKeyValueA(nwAdapters,_mySubkeyName.c_str(),"*IfType",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));

		if (creatingNewDevice) {
			// Vista/2008 does not set this
			if (newDeviceInstanceId.length() > 0)
				RegSetKeyValueA(nwAdapters,_mySubkeyName.c_str(),"DeviceInstanceID",REG_SZ,newDeviceInstanceId.c_str(),(DWORD)newDeviceInstanceId.length());

			// Set EnableDHCP to 0 by default on new devices
			tmp = 0;
			RegSetKeyValueA(nwAdapters,_mySubkeyName.c_str(),"EnableDHCP",REG_DWORD,(LPCVOID)&tmp,sizeof(tmp));
		}
		RegCloseKey(nwAdapters);
	} else {
		RegCloseKey(nwAdapters);
		throw std::runtime_error("unable to find or create tap adapter");
	}

	{
		char nobraces[128]; // strip braces from GUID before converting it, because Windows
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

	// Get the LUID, which is one of like four fucking ways to refer to a network device in Windows
	if (ConvertInterfaceGuidToLuid(&_deviceGuid,&_deviceLuid) != NO_ERROR)
		throw std::runtime_error("unable to convert device interface GUID to LUID");

	//_initialized = true;

	if (friendlyName)
		setFriendlyName(friendlyName);

	_injectSemaphore = CreateSemaphore(NULL,0,1,NULL);
	_thread = Thread::start(this);
}

WindowsEthernetTap::~WindowsEthernetTap()
{
	_run = false;
	ReleaseSemaphore(_injectSemaphore,1,NULL);
	Thread::join(_thread);
	CloseHandle(_injectSemaphore);
	setPersistentTapDeviceState(_deviceInstanceId.c_str(),false);
}

void WindowsEthernetTap::setEnabled(bool en)
{
	_enabled = en;
}

bool WindowsEthernetTap::enabled() const
{
	return _enabled;
}

bool WindowsEthernetTap::addIp(const InetAddress &ip)
{
	if (!ip.netmaskBits()) // sanity check... netmask of 0.0.0.0 is WUT?
		return false;

	Mutex::Lock _l(_assignedIps_m);
	if (std::find(_assignedIps.begin(),_assignedIps.end(),ip) != _assignedIps.end())
		return true;
	_assignedIps.push_back(ip);
	_syncIps();
	return true;
}

bool WindowsEthernetTap::removeIp(const InetAddress &ip)
{
    if (ip.isV6())
        return true;

	{
		Mutex::Lock _l(_assignedIps_m);
		std::vector<InetAddress>::iterator aip(std::find(_assignedIps.begin(),_assignedIps.end(),ip));
		if (aip != _assignedIps.end())
			_assignedIps.erase(aip);
	}

	if (!_initialized)
		return false;

	try {
		MIB_UNICASTIPADDRESS_TABLE *ipt = (MIB_UNICASTIPADDRESS_TABLE *)0;
		if (GetUnicastIpAddressTable(AF_UNSPEC,&ipt) == NO_ERROR) {
			if ((ipt)&&(ipt->NumEntries > 0)) {
				for(DWORD i=0;i<(DWORD)ipt->NumEntries;++i) {
					if (ipt->Table[i].InterfaceLuid.Value == _deviceLuid.Value) {
						InetAddress addr;
						switch(ipt->Table[i].Address.si_family) {
							case AF_INET:
								addr.set(&(ipt->Table[i].Address.Ipv4.sin_addr.S_un.S_addr),4,ipt->Table[i].OnLinkPrefixLength);
								break;
							case AF_INET6:
								addr.set(ipt->Table[i].Address.Ipv6.sin6_addr.u.Byte,16,ipt->Table[i].OnLinkPrefixLength);
								if (addr.ipScope() == InetAddress::IP_SCOPE_LINK_LOCAL)
									continue; // can't remove link-local IPv6 addresses
								break;
						}
						if (addr == ip) {
							DeleteUnicastIpAddressEntry(&(ipt->Table[i]));
							FreeMibTable(ipt);

							if (ip.isV4()) {
								std::vector<std::string> regIps(_getRegistryIPv4Value("IPAddress"));
								std::vector<std::string> regSubnetMasks(_getRegistryIPv4Value("SubnetMask"));
								char ipbuf[64];
								std::string ipstr(ip.toIpString(ipbuf));
								for (std::vector<std::string>::iterator rip(regIps.begin()), rm(regSubnetMasks.begin()); ((rip != regIps.end()) && (rm != regSubnetMasks.end())); ++rip, ++rm) {
									if (*rip == ipstr) {
										regIps.erase(rip);
										regSubnetMasks.erase(rm);
										_setRegistryIPv4Value("IPAddress", regIps);
										_setRegistryIPv4Value("SubnetMask", regSubnetMasks);
										break;
									}
								}
							}

							return true;
						}
					}
				}
			}
			FreeMibTable((PVOID)ipt);
		}
	} catch ( ... ) {}
	return false;
}

std::vector<InetAddress> WindowsEthernetTap::ips() const
{
	static const InetAddress linkLocalLoopback("fe80::1/64"); // what is this and why does Windows assign it?
	std::vector<InetAddress> addrs;

	if (!_initialized)
		return addrs;

	try {
		MIB_UNICASTIPADDRESS_TABLE *ipt = (MIB_UNICASTIPADDRESS_TABLE *)0;
		if (GetUnicastIpAddressTable(AF_UNSPEC,&ipt) == NO_ERROR) {
			if ((ipt)&&(ipt->NumEntries > 0)) {
				for(DWORD i=0;i<(DWORD)ipt->NumEntries;++i) {
					if (ipt->Table[i].InterfaceLuid.Value == _deviceLuid.Value) {
						switch(ipt->Table[i].Address.si_family) {
							case AF_INET: {
								InetAddress ip(&(ipt->Table[i].Address.Ipv4.sin_addr.S_un.S_addr),4,ipt->Table[i].OnLinkPrefixLength);
								if (ip != InetAddress::LO4)
									addrs.push_back(ip);
							}	break;
							case AF_INET6: {
								InetAddress ip(ipt->Table[i].Address.Ipv6.sin6_addr.u.Byte,16,ipt->Table[i].OnLinkPrefixLength);
								if ((ip != linkLocalLoopback)&&(ip != InetAddress::LO6))
									addrs.push_back(ip);
							}	break;
						}
					}
				}
			}
			FreeMibTable(ipt);
		}
	} catch ( ... ) {} // sanity check, shouldn't happen unless out of memory

	std::sort(addrs.begin(),addrs.end());
	addrs.erase(std::unique(addrs.begin(),addrs.end()),addrs.end());

	return addrs;
}

void WindowsEthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	if ((!_initialized)||(!_enabled)||(_tap == INVALID_HANDLE_VALUE)||(len > _mtu))
		return;

	Mutex::Lock _l(_injectPending_m);
	_injectPending.emplace();
	_injectPending.back().len = len + 14;
	char *const d = _injectPending.back().data;
	to.copyTo((uint8_t *)d);
	from.copyTo((uint8_t *)(d + 6));
	d[12] = (char)((etherType >> 8U) & 0xffU);
	d[13] = (char)(etherType & 0xffU);
	memcpy(d + 14,data,len);

	ReleaseSemaphore(_injectSemaphore,1,NULL);
}

std::string WindowsEthernetTap::deviceName() const
{
	char tmp[1024];
	if (ConvertInterfaceLuidToNameA(&_deviceLuid,tmp,sizeof(tmp)) != NO_ERROR)
		return std::string("[ConvertInterfaceLuidToName() failed]");
	return std::string(tmp);
}

void WindowsEthernetTap::setFriendlyName(const char *dn)
{
	if (!_initialized)
		return;
	HKEY ifp;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,(std::string("SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\") + _netCfgInstanceId).c_str(),0,KEY_READ|KEY_WRITE,&ifp) == ERROR_SUCCESS) {
		RegSetKeyValueA(ifp,"Connection","Name",REG_SZ,(LPCVOID)dn,(DWORD)(strlen(dn)+1));
		RegCloseKey(ifp);
	}

	HRESULT hr = CoInitialize(nullptr);
	if (hr != S_OK) return;
	CoInitializeSecurity(NULL, -1, NULL, NULL,
		RPC_C_AUTHN_LEVEL_PKT,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		NULL, EOAC_NONE, NULL);
	if (hr != S_OK) return;

	INetSharingManager *nsm;
	//hr = CoCreateInstance(__uuidof(NetSharingManager), NULL, CLSCTX_ALL, __uuidof(INetSharingManager), (void**)&nsm);
	hr = CoCreateInstance(CLSID_NetSharingManager, NULL, CLSCTX_ALL, IID_INetSharingManager, (void**)&nsm);
	if (hr != S_OK)	return;

	bool found = false;
	INetSharingEveryConnectionCollection *nsecc = nullptr;
	hr = nsm->get_EnumEveryConnection(&nsecc);
	if (!nsecc) {
		fprintf(stderr, "Failed to get NSM connections");
		return;
	}

	IEnumVARIANT *ev = nullptr;
	IUnknown *unk = nullptr;
	hr = nsecc->get__NewEnum(&unk);
	if (unk) {
		//hr = unk->QueryInterface(__uuidof(IEnumVARIANT), (void**)&ev);
		hr = unk->QueryInterface(IID_IEnumVARIANT, (void**)&ev);
		unk->Release();
	}
	if (ev) {
		VARIANT v;
		VariantInit(&v);

		while ((S_OK == ev->Next(1, &v, NULL)) && found == FALSE) {
			if (V_VT(&v) == VT_UNKNOWN) {
				INetConnection *nc = nullptr;
				//V_UNKNOWN(&v)->QueryInterface(__uuidof(INetConnection), (void**)&nc);
				V_UNKNOWN(&v)->QueryInterface(IID_INetConnection, (void**)&nc);
				if (nc) {
					NETCON_PROPERTIES *ncp = nullptr;
					nc->GetProperties(&ncp);

					GUID curId = ncp->guidId;
					if (curId == _deviceGuid) {
						wchar_t wtext[255];
						mbstowcs(wtext, dn, strlen(dn)+1);
						nc->Rename(wtext);
						found = true;
					}
					nc->Release();
				}
			}
			VariantClear(&v);
		}
		ev->Release();
	}
	nsecc->Release();
}

void WindowsEthernetTap::scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed)
{
	if (!_initialized)
		return;
	HANDLE t = _tap;
	if (t == INVALID_HANDLE_VALUE)
		return;

	std::vector<MulticastGroup> newGroups;

	// The ZT1 tap driver supports an IOCTL to get multicast memberships at the L2
	// level... something Windows does not seem to expose ordinarily. This lets
	// pretty much anything work... IPv4, IPv6, IPX, oldskool Netbios, who knows...
	unsigned char mcastbuf[TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS_OUTPUT_BUF_SIZE];
	DWORD bytesReturned = 0;
	if (DeviceIoControl(t,TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS,(LPVOID)mcastbuf,sizeof(mcastbuf),(LPVOID)mcastbuf,sizeof(mcastbuf),&bytesReturned,NULL)) {
		if ((bytesReturned > 0)&&(bytesReturned <= TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS_OUTPUT_BUF_SIZE)) { // sanity check
			MAC mac;
			DWORD i = 0;
			while ((i + 6) <= bytesReturned) {
				mac.setTo((uint8_t *)(mcastbuf + i));
				i += 6;
				if ((mac.isMulticast())&&(!mac.isBroadcast())) {
					// exclude the nulls that may be returned or any other junk Windows puts in there
					newGroups.push_back(MulticastGroup(mac,0));
				}
			}
		}
	}

	std::vector<InetAddress> allIps(ips());
	for(std::vector<InetAddress>::iterator ip(allIps.begin());ip!=allIps.end();++ip)
		newGroups.push_back(MulticastGroup::deriveMulticastGroupForAddressResolution(*ip));

	std::sort(newGroups.begin(),newGroups.end());
	newGroups.erase(std::unique(newGroups.begin(),newGroups.end()),newGroups.end());

	for(std::vector<MulticastGroup>::iterator m(newGroups.begin());m!=newGroups.end();++m) {
		if (!std::binary_search(_multicastGroups.begin(),_multicastGroups.end(),*m))
			added.push_back(*m);
	}
	for(std::vector<MulticastGroup>::iterator m(_multicastGroups.begin());m!=_multicastGroups.end();++m) {
		if (!std::binary_search(newGroups.begin(),newGroups.end(),*m))
			removed.push_back(*m);
	}

	_multicastGroups.swap(newGroups);
}

void WindowsEthernetTap::setMtu(unsigned int mtu)
{
	if (mtu != _mtu) {
		_mtu = mtu;
		HKEY nwAdapters;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}", 0, KEY_READ | KEY_WRITE, &nwAdapters) == ERROR_SUCCESS) {
			char tmps[64];
			unsigned int tmpsl = OSUtils::ztsnprintf(tmps, sizeof(tmps), "%d", mtu);
			RegSetKeyValueA(nwAdapters, _mySubkeyName.c_str(), "MTU", REG_SZ, tmps, tmpsl);
			RegCloseKey(nwAdapters);
		}
	}
}

NET_IFINDEX WindowsEthernetTap::interfaceIndex() const
{
	NET_IFINDEX idx = -1;
	if (ConvertInterfaceLuidToIndex(&_deviceLuid,&idx) == NO_ERROR)
		return idx;
	return -1;
}

void WindowsEthernetTap::threadMain()
	throw()
{
	char tapReadBuf[ZT_MAX_MTU + 32];
	char tapPath[128];
	HANDLE wait4[3];
	OVERLAPPED tapOvlRead,tapOvlWrite;

	OSUtils::ztsnprintf(tapPath,sizeof(tapPath),"\\\\.\\Global\\%s.tap",_netCfgInstanceId.c_str());

	try {
		while (_run) {
			// Because Windows
			Sleep(250);
			setPersistentTapDeviceState(_deviceInstanceId.c_str(),false);
			Sleep(250);
			setPersistentTapDeviceState(_deviceInstanceId.c_str(),true);
			Sleep(250);
			setPersistentTapDeviceState(_deviceInstanceId.c_str(),false);
			Sleep(250);
			setPersistentTapDeviceState(_deviceInstanceId.c_str(),true);
			Sleep(250);

			_tap = CreateFileA(tapPath,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_SYSTEM|FILE_FLAG_OVERLAPPED,NULL);
			if (_tap == INVALID_HANDLE_VALUE) {
				Sleep(250);
				continue;
			}

			{
				uint32_t tmpi = 1;
				DWORD bytesReturned = 0;
				DeviceIoControl(_tap,TAP_WIN_IOCTL_SET_MEDIA_STATUS,&tmpi,sizeof(tmpi),&tmpi,sizeof(tmpi),&bytesReturned,NULL);
			}

#ifdef ZT_WINDOWS_CREATE_FAKE_DEFAULT_ROUTE
			{
				/* This inserts a fake default route and a fake ARP entry, forcing
				 * Windows to detect this as a "real" network and apply proper
				 * firewall rules.
				 *
				 * This hack is completely stupid, but Windows made me do it
				 * by being broken and insane.
				 *
				 * Background: Windows tries to detect its network location by
				 * matching it to the ARP address of the default route. Networks
				 * without default routes are "unidentified networks" and cannot
				 * have their firewall classification changed by the user (easily).
				 *
				 * Yes, you read that right.
				 *
				 * The common workaround is to set *NdisDeviceType to 1, which
				 * totally disables all Windows firewall functionality. This is
				 * the answer you'll find on most forums for things like OpenVPN.
				 *
				 * Yes, you read that right.
				 *
				 * The default route workaround is also known, but for this to
				 * work there must be a known default IP that resolves to a known
				 * ARP address. This works for an OpenVPN tunnel, but not here
				 * because this isn't a tunnel. It's a mesh. There is no "other
				 * end," or any other known always on IP.
				 *
				 * So let's make a fake one and shove it in there along with its
				 * fake static ARP entry. Also makes it instant-on and static.
				 *
				 * We'll have to see what DHCP does with this. In the future we
				 * probably will not want to do this on DHCP-enabled networks, so
				 * when we enable DHCP we will go in and yank this wacko hacko from
				 * the routing table before doing so.
				 *
				 * Like Jesse Pinkman would say: "YEEEEAAH BITCH!" */
				const uint32_t fakeIp = htonl(0x19fffffe); // 25.255.255.254 -- unrouted IPv4 block
				for(int i=0;i<8;++i) {
					MIB_IPNET_ROW2 ipnr;
					memset(&ipnr,0,sizeof(ipnr));
					ipnr.Address.si_family = AF_INET;
					ipnr.Address.Ipv4.sin_addr.s_addr = fakeIp;
					ipnr.InterfaceLuid.Value = _deviceLuid.Value;
					ipnr.PhysicalAddress[0] = _mac[0] ^ 0x10; // just make something up that's consistent and not part of this net
					ipnr.PhysicalAddress[1] = 0x00;
					ipnr.PhysicalAddress[2] = (UCHAR)((_deviceGuid.Data1 >> 24) & 0xff);
					ipnr.PhysicalAddress[3] = (UCHAR)((_deviceGuid.Data1 >> 16) & 0xff);
					ipnr.PhysicalAddress[4] = (UCHAR)((_deviceGuid.Data1 >> 8) & 0xff);
					ipnr.PhysicalAddress[5] = (UCHAR)(_deviceGuid.Data1 & 0xff);
					ipnr.PhysicalAddressLength = 6;
					ipnr.State = NlnsPermanent;
					ipnr.IsRouter = 1;
					ipnr.IsUnreachable = 0;
					ipnr.ReachabilityTime.LastReachable = 0x0fffffff;
					ipnr.ReachabilityTime.LastUnreachable = 1;
					DWORD result = CreateIpNetEntry2(&ipnr);
					if (result != NO_ERROR)
						Sleep(250);
					else break;
				}
				for(int i=0;i<8;++i) {
					MIB_IPFORWARD_ROW2 nr;
					memset(&nr,0,sizeof(nr));
					InitializeIpForwardEntry(&nr);
					nr.InterfaceLuid.Value = _deviceLuid.Value;
					nr.DestinationPrefix.Prefix.si_family = AF_INET; // rest is left as 0.0.0.0/0
					nr.NextHop.si_family = AF_INET;
					nr.NextHop.Ipv4.sin_addr.s_addr = fakeIp;
					nr.Metric = 9999; // do not use as real default route
					nr.Protocol = (NL_ROUTE_PROTOCOL)MIB_IPPROTO_NETMGMT;
					DWORD result = CreateIpForwardEntry2(&nr);
					if (result != NO_ERROR)
						Sleep(250);
					else break;
				}
			}
#endif

			// Assign or re-assign any should-be-assigned IPs in case we have restarted
			{
				Mutex::Lock _l(_assignedIps_m);
				_syncIps();
			}

			memset(&tapOvlRead,0,sizeof(tapOvlRead));
			tapOvlRead.hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
			memset(&tapOvlWrite,0,sizeof(tapOvlWrite));
			tapOvlWrite.hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

			wait4[0] = _injectSemaphore;
			wait4[1] = tapOvlRead.hEvent;
			wait4[2] = tapOvlWrite.hEvent; // only included if writeInProgress is true

			ReadFile(_tap,tapReadBuf,sizeof(tapReadBuf),NULL,&tapOvlRead);
			bool writeInProgress = false;
			ULONGLONG timeOfLastBorkCheck = GetTickCount64();
			_initialized = true;
			unsigned int oldmtu = _mtu;

			setFriendlyName(_friendlyName.c_str());

			while (_run) {
				DWORD waitResult = WaitForMultipleObjectsEx(writeInProgress ? 3 : 2,wait4,FALSE,2500,TRUE);
				if (!_run) break; // will also break outer while(_run) since _run is false

				// Check for changes in MTU and break to restart tap device to reconfigure in this case
				if (_mtu != oldmtu)
					break;

				// Check for issues with adapter and close/reopen if any are detected. This
				// check fixes a while boatload of Windows adapter 'coma' issues after
				// sleep/wake and when adapters are added/removed. Basically if the tap
				// device is borked, whack it.
				{
					ULONGLONG tc = GetTickCount64();
					if ((tc - timeOfLastBorkCheck) >= 2500) {
						timeOfLastBorkCheck = tc;
						char aabuf[16384];
						ULONG aalen = sizeof(aabuf);
						if (GetAdaptersAddresses(AF_UNSPEC,GAA_FLAG_SKIP_UNICAST|GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER|GAA_FLAG_SKIP_FRIENDLY_NAME,(void *)0,reinterpret_cast<PIP_ADAPTER_ADDRESSES>(aabuf),&aalen) == NO_ERROR) {
							bool isBorked = false;

							PIP_ADAPTER_ADDRESSES aa = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(aabuf);
							while (aa) {
								if (_deviceLuid.Value == aa->Luid.Value) {
									isBorked = (aa->OperStatus != IfOperStatusUp);
									break;
								}
								aa = aa->Next;
							}

							if (isBorked) {
								// Close and reopen tap device if there's an issue (outer loop)
								break;
							}
						}
					}
				}

				if ((waitResult == WAIT_TIMEOUT)||(waitResult == WAIT_FAILED)) {
					Sleep(250); // guard against spinning under some conditions
					continue;
				}

				if (HasOverlappedIoCompleted(&tapOvlRead)) {
					DWORD bytesRead = 0;
					if (GetOverlappedResult(_tap,&tapOvlRead,&bytesRead,FALSE)) {
						if ((bytesRead > 14)&&(_enabled)) {
							MAC to((uint8_t *)tapReadBuf);
							MAC from((uint8_t *)(tapReadBuf + 6));
							unsigned int etherType = ((((unsigned int)tapReadBuf[12]) & 0xffU) << 8U) | (((unsigned int)tapReadBuf[13]) & 0xffU);
							try {
								_handler(_arg,nullptr,_nwid,from,to,etherType,0,tapReadBuf + 14,bytesRead - 14);
							} catch ( ... ) {} // handlers should not throw
						}
					}
					ReadFile(_tap,tapReadBuf,ZT_MAX_MTU + 32,NULL,&tapOvlRead);
				}

				if (writeInProgress) {
					if (HasOverlappedIoCompleted(&tapOvlWrite)) {
						writeInProgress = false;
						_injectPending_m.lock();
						_injectPending.pop();
					} else continue; // still writing, so skip code below and wait
				} else _injectPending_m.lock();

				if (!_injectPending.empty()) {
					WriteFile(_tap,_injectPending.front().data,_injectPending.front().len,NULL,&tapOvlWrite);
					writeInProgress = true;
				}

				_injectPending_m.unlock();
			}

			CancelIo(_tap);

			CloseHandle(tapOvlRead.hEvent);
			CloseHandle(tapOvlWrite.hEvent);
			CloseHandle(_tap);
			_tap = INVALID_HANDLE_VALUE;

			// We will restart and re-open the tap unless _run == false
		}
	} catch ( ... ) {} // catch unexpected exceptions -- this should not happen but would prevent program crash or other weird issues since threads should not throw
}

NET_IFINDEX WindowsEthernetTap::_getDeviceIndex()
{
	MIB_IF_TABLE2 *ift = (MIB_IF_TABLE2 *)0;

	if (GetIfTable2Ex(MibIfTableRaw,&ift) != NO_ERROR)
		throw std::runtime_error("GetIfTable2Ex() failed");

	if (ift->NumEntries > 0) {
		for(ULONG i=0;i<ift->NumEntries;++i) {
			if (ift->Table[i].InterfaceLuid.Value == _deviceLuid.Value) {
				NET_IFINDEX idx = ift->Table[i].InterfaceIndex;
				FreeMibTable(ift);
				return idx;
			}
		}
	}

	FreeMibTable(&ift);

	throw std::runtime_error("interface not found");
}

std::vector<std::string> WindowsEthernetTap::_getRegistryIPv4Value(const char *regKey)
{
	std::vector<std::string> value;
	HKEY tcpIpInterfaces;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\services\\Tcpip\\Parameters\\Interfaces",0,KEY_READ|KEY_WRITE,&tcpIpInterfaces) == ERROR_SUCCESS) {
		char buf[16384];
		DWORD len = sizeof(buf);
		DWORD kt = REG_MULTI_SZ;
		if (RegGetValueA(tcpIpInterfaces,_netCfgInstanceId.c_str(),regKey,0,&kt,&buf,&len) == ERROR_SUCCESS) {
			switch(kt) {
				case REG_SZ:
					if (len > 0)
						value.push_back(std::string(buf));
					break;
				case REG_MULTI_SZ: {
					for(DWORD k=0,s=0;k<len;++k) {
						if (!buf[k]) {
							if (s < k) {
								value.push_back(std::string(buf + s));
								s = k + 1;
							} else break;
						}
					}
				}	break;
			}
		}
		RegCloseKey(tcpIpInterfaces);
	}
	return value;
}

void WindowsEthernetTap::_setRegistryIPv4Value(const char *regKey,const std::vector<std::string> &value)
{
	std::string regMulti;
	for(std::vector<std::string>::const_iterator s(value.begin());s!=value.end();++s) {
		regMulti.append(*s);
		regMulti.push_back((char)0);
	}
	HKEY tcpIpInterfaces;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\services\\Tcpip\\Parameters\\Interfaces",0,KEY_READ|KEY_WRITE,&tcpIpInterfaces) == ERROR_SUCCESS) {
		if (regMulti.length() > 0) {
			regMulti.push_back((char)0);
			RegSetKeyValueA(tcpIpInterfaces,_netCfgInstanceId.c_str(),regKey,REG_MULTI_SZ,regMulti.data(),(DWORD)regMulti.length());
		} else {
			RegDeleteKeyValueA(tcpIpInterfaces,_netCfgInstanceId.c_str(),regKey);
		}
		RegCloseKey(tcpIpInterfaces);
	}
}

void WindowsEthernetTap::_syncIps()
{
	// assumes _assignedIps_m is locked

	if (!_initialized)
		return;

	std::vector<InetAddress> haveIps(ips());

	for(std::vector<InetAddress>::const_iterator aip(_assignedIps.begin());aip!=_assignedIps.end();++aip) {
		if (std::find(haveIps.begin(),haveIps.end(),*aip) == haveIps.end()) {
			MIB_UNICASTIPADDRESS_ROW ipr;

			InitializeUnicastIpAddressEntry(&ipr);
			if (aip->isV4()) {
				ipr.Address.Ipv4.sin_family = AF_INET;
				ipr.Address.Ipv4.sin_addr.S_un.S_addr = *((const uint32_t *)aip->rawIpData());
				ipr.OnLinkPrefixLength = aip->netmaskBits();
				if (ipr.OnLinkPrefixLength >= 32)
					continue;
			} else if (aip->isV6()) {
				ipr.Address.Ipv6.sin6_family = AF_INET6;
				memcpy(ipr.Address.Ipv6.sin6_addr.u.Byte,aip->rawIpData(),16);
				ipr.OnLinkPrefixLength = aip->netmaskBits();
				if (ipr.OnLinkPrefixLength >= 128)
					continue;
			} else continue;

			ipr.PrefixOrigin = IpPrefixOriginManual;
			ipr.SuffixOrigin = IpSuffixOriginManual;
			ipr.ValidLifetime = 0xffffffff;
			ipr.PreferredLifetime = 0xffffffff;

			ipr.InterfaceLuid = _deviceLuid;
			ipr.InterfaceIndex = _getDeviceIndex();

			CreateUnicastIpAddressEntry(&ipr);
		}

		if (aip->isV4()) {
			char ipbuf[64];
			std::string ipStr(aip->toIpString(ipbuf));
			std::vector<std::string> regIps(_getRegistryIPv4Value("IPAddress"));
			if (std::find(regIps.begin(), regIps.end(), ipStr) == regIps.end()) {
				std::vector<std::string> regSubnetMasks(_getRegistryIPv4Value("SubnetMask"));
				regIps.push_back(ipStr);
				regSubnetMasks.push_back(aip->netmask().toIpString(ipbuf));
				_setRegistryIPv4Value("IPAddress", regIps);
				_setRegistryIPv4Value("SubnetMask", regSubnetMasks);
			}
		}
	}
}

} // namespace ZeroTier

#endif
