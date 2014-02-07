/****************************** Module Header ******************************\
* Module Name:  ServiceBase.h
* Project:      CppWindowsService
* Copyright (c) Microsoft Corporation.
* 
* Provides a base class for a service that will exist as part of a service 
* application. CServiceBase must be derived from when creating a new service 
* class.
* 
* This source is subject to the Microsoft Public License.
* See http://www.microsoft.com/en-us/openness/resources/licenses.aspx#MPL.
* All other rights reserved.
* 
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, 
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/

#pragma once

#include <WinSock2.h>
#include <windows.h>

class CServiceBase
{
public:

    // Register the executable for a service with the Service Control Manager 
    // (SCM). After you call Run(ServiceBase), the SCM issues a Start command, 
    // which results in a call to the OnStart method in the service. This 
    // method blocks until the service has stopped.
    static BOOL Run(CServiceBase &service);

    // Service object constructor. The optional parameters (fCanStop, 
    // fCanShutdown and fCanPauseContinue) allow you to specify whether the 
    // service can be stopped, paused and continued, or be notified when 
    // system shutdown occurs.
    CServiceBase(LPSTR pszServiceName, 
        BOOL fCanStop = TRUE, 
        BOOL fCanShutdown = TRUE, 
        BOOL fCanPauseContinue = FALSE);

    // Service object destructor. 
    virtual ~CServiceBase(void);

    // Stop the service.
    void Stop();

protected:

    // When implemented in a derived class, executes when a Start command is 
    // sent to the service by the SCM or when the operating system starts 
    // (for a service that starts automatically). Specifies actions to take 
    // when the service starts.
    virtual void OnStart(DWORD dwArgc, PSTR *pszArgv);

    // When implemented in a derived class, executes when a Stop command is 
    // sent to the service by the SCM. Specifies actions to take when a 
    // service stops running.
    virtual void OnStop();

    // When implemented in a derived class, executes when a Pause command is 
    // sent to the service by the SCM. Specifies actions to take when a 
    // service pauses.
    virtual void OnPause();

    // When implemented in a derived class, OnContinue runs when a Continue 
    // command is sent to the service by the SCM. Specifies actions to take 
    // when a service resumes normal functioning after being paused.
    virtual void OnContinue();

    // When implemented in a derived class, executes when the system is 
    // shutting down. Specifies what should occur immediately prior to the 
    // system shutting down.
    virtual void OnShutdown();

    // Set the service status and report the status to the SCM.
    void SetServiceStatus(DWORD dwCurrentState, 
        DWORD dwWin32ExitCode = NO_ERROR, 
        DWORD dwWaitHint = 0);

    // Log a message to the Application event log.
    void WriteEventLogEntry(PSTR pszMessage, WORD wType);

    // Log an error message to the Application event log.
    void WriteErrorLogEntry(PSTR pszFunction, 
        DWORD dwError = GetLastError());

private:

    // Entry point for the service. It registers the handler function for the 
    // service and starts the service.
    static void WINAPI ServiceMain(DWORD dwArgc, LPSTR *lpszArgv);

    // The function is called by the SCM whenever a control code is sent to 
    // the service.
    static void WINAPI ServiceCtrlHandler(DWORD dwCtrl);

    // Start the service.
    void Start(DWORD dwArgc, PSTR *pszArgv);
    
    // Pause the service.
    void Pause();

    // Resume the service after being paused.
    void Continue();

    // Execute when the system is shutting down.
    void Shutdown();

    // The singleton service instance.
    static CServiceBase *s_service;

    // The name of the service
    LPSTR m_name;

    // The status of the service
    SERVICE_STATUS m_status;

    // The service status handle
    SERVICE_STATUS_HANDLE m_statusHandle;
};