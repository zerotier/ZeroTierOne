/****************************** Module Header ******************************\
* Module Name:  ServiceInstaller.cpp
* Project:      CppWindowsService
* Copyright (c) Microsoft Corporation.
* 
* The file implements functions that install and uninstall the service.
* 
* This source is subject to the Microsoft Public License.
* See http://www.microsoft.com/en-us/openness/resources/licenses.aspx#MPL.
* All other rights reserved.
* 
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, 
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/

#pragma region "Includes"
#include <stdio.h>
#include <windows.h>
#include "ServiceInstaller.h"
#pragma endregion


//
//   FUNCTION: InstallService
//
//   PURPOSE: Install the current application as a service to the local 
//   service control manager database.
//
//   PARAMETERS:
//   * pszServiceName - the name of the service to be installed
//   * pszDisplayName - the display name of the service
//   * dwStartType - the service start option. This parameter can be one of 
//     the following values: SERVICE_AUTO_START, SERVICE_BOOT_START, 
//     SERVICE_DEMAND_START, SERVICE_DISABLED, SERVICE_SYSTEM_START.
//   * pszDependencies - a pointer to a double null-terminated array of null-
//     separated names of services or load ordering groups that the system 
//     must start before this service.
//   * pszAccount - the name of the account under which the service runs.
//   * pszPassword - the password to the account name.
//
//   NOTE: If the function fails to install the service, it prints the error 
//   in the standard output stream for users to diagnose the problem.
//
void InstallService(PSTR pszServiceName, 
                    PSTR pszDisplayName, 
                    DWORD dwStartType,
                    PSTR pszDependencies, 
                    PSTR pszAccount, 
                    PSTR pszPassword)
{
    char szPath[MAX_PATH];
    SC_HANDLE schSCManager = NULL;
    SC_HANDLE schService = NULL;

    if (GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)) == 0)
    {
        wprintf(L"GetModuleFileName failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    // Open the local default service control manager database
    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | 
        SC_MANAGER_CREATE_SERVICE);
    if (schSCManager == NULL)
    {
        wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    // Install the service into SCM by calling CreateService
    schService = CreateService(
        schSCManager,                   // SCManager database
        pszServiceName,                 // Name of service
        pszDisplayName,                 // Name to display
        SERVICE_QUERY_STATUS,           // Desired access
        SERVICE_WIN32_OWN_PROCESS,      // Service type
        dwStartType,                    // Service start type
        SERVICE_ERROR_NORMAL,           // Error control type
        szPath,                         // Service's binary
        NULL,                           // No load ordering group
        NULL,                           // No tag identifier
        pszDependencies,                // Dependencies
        pszAccount,                     // Service running account
        pszPassword                     // Password of the account
        );
    if (schService == NULL)
    {
        wprintf(L"CreateService failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    wprintf(L"%s is installed.\n", pszServiceName);

Cleanup:
    // Centralized cleanup for all allocated resources.
    if (schSCManager)
    {
        CloseServiceHandle(schSCManager);
        schSCManager = NULL;
    }
    if (schService)
    {
        CloseServiceHandle(schService);
        schService = NULL;
    }
}


//
//   FUNCTION: UninstallService
//
//   PURPOSE: Stop and remove the service from the local service control 
//   manager database.
//
//   PARAMETERS: 
//   * pszServiceName - the name of the service to be removed.
//
//   NOTE: If the function fails to uninstall the service, it prints the 
//   error in the standard output stream for users to diagnose the problem.
//
void UninstallService(PSTR pszServiceName)
{
    SC_HANDLE schSCManager = NULL;
    SC_HANDLE schService = NULL;
    SERVICE_STATUS ssSvcStatus = {};

    // Open the local default service control manager database
    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (schSCManager == NULL)
    {
        wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    // Open the service with delete, stop, and query status permissions
    schService = OpenService(schSCManager, pszServiceName, SERVICE_STOP | 
        SERVICE_QUERY_STATUS | DELETE);
    if (schService == NULL)
    {
        wprintf(L"OpenService failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    // Try to stop the service
    if (ControlService(schService, SERVICE_CONTROL_STOP, &ssSvcStatus))
    {
        wprintf(L"Stopping %s.", pszServiceName);
        Sleep(1000);

        while (QueryServiceStatus(schService, &ssSvcStatus))
        {
            if (ssSvcStatus.dwCurrentState == SERVICE_STOP_PENDING)
            {
                wprintf(L".");
                Sleep(1000);
            }
            else break;
        }

        if (ssSvcStatus.dwCurrentState == SERVICE_STOPPED)
        {
            wprintf(L"\n%s is stopped.\n", pszServiceName);
        }
        else
        {
            wprintf(L"\n%s failed to stop.\n", pszServiceName);
        }
    }

    // Now remove the service by calling DeleteService.
    if (!DeleteService(schService))
    {
        wprintf(L"DeleteService failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }

    wprintf(L"%s is removed.\n", pszServiceName);

Cleanup:
    // Centralized cleanup for all allocated resources.
    if (schSCManager)
    {
        CloseServiceHandle(schSCManager);
        schSCManager = NULL;
    }
    if (schService)
    {
        CloseServiceHandle(schService);
        schService = NULL;
    }
}