/****************************** Module Header ******************************\
* Module Name:  ServiceBase.cpp
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

#pragma region Includes
#include "ServiceBase.h"
#include <assert.h>
#include <strsafe.h>
#include <string>
#pragma endregion


#pragma region Static Members

// Initialize the singleton service instance.
CServiceBase *CServiceBase::s_service = NULL;


//
//   FUNCTION: CServiceBase::Run(CServiceBase &)
//
//   PURPOSE: Register the executable for a service with the Service Control 
//   Manager (SCM). After you call Run(ServiceBase), the SCM issues a Start 
//   command, which results in a call to the OnStart method in the service. 
//   This method blocks until the service has stopped.
//
//   PARAMETERS:
//   * service - the reference to a CServiceBase object. It will become the 
//     singleton service instance of this service application.
//
//   RETURN VALUE: If the function succeeds, the return value is TRUE. If the 
//   function fails, the return value is FALSE. To get extended error 
//   information, call GetLastError.
//
BOOL CServiceBase::Run(CServiceBase &service)
{
    s_service = &service;

    SERVICE_TABLE_ENTRYA serviceTable[] = 
    {
        { service.m_name, ServiceMain },
        { NULL, NULL }
    };

    // Connects the main thread of a service process to the service control 
    // manager, which causes the thread to be the service control dispatcher 
    // thread for the calling process. This call returns when the service has 
    // stopped. The process should simply terminate when the call returns.
    return StartServiceCtrlDispatcher(serviceTable);
}


//
//   FUNCTION: CServiceBase::ServiceMain(DWORD, PWSTR *)
//
//   PURPOSE: Entry point for the service. It registers the handler function 
//   for the service and starts the service.
//
//   PARAMETERS:
//   * dwArgc   - number of command line arguments
//   * lpszArgv - array of command line arguments
//
void WINAPI CServiceBase::ServiceMain(DWORD dwArgc, PSTR *pszArgv)
{
    assert(s_service != NULL);

    // Register the handler function for the service
    s_service->m_statusHandle = RegisterServiceCtrlHandler(
        s_service->m_name, ServiceCtrlHandler);
    if (s_service->m_statusHandle == NULL)
    {
        throw GetLastError();
    }

    // Start the service.
    s_service->Start(dwArgc, pszArgv);
}


//
//   FUNCTION: CServiceBase::ServiceCtrlHandler(DWORD)
//
//   PURPOSE: The function is called by the SCM whenever a control code is 
//   sent to the service. 
//
//   PARAMETERS:
//   * dwCtrlCode - the control code. This parameter can be one of the 
//   following values: 
//
//     SERVICE_CONTROL_CONTINUE
//     SERVICE_CONTROL_INTERROGATE
//     SERVICE_CONTROL_NETBINDADD
//     SERVICE_CONTROL_NETBINDDISABLE
//     SERVICE_CONTROL_NETBINDREMOVE
//     SERVICE_CONTROL_PARAMCHANGE
//     SERVICE_CONTROL_PAUSE
//     SERVICE_CONTROL_SHUTDOWN
//     SERVICE_CONTROL_STOP
//
//   This parameter can also be a user-defined control code ranges from 128 
//   to 255.
//
void WINAPI CServiceBase::ServiceCtrlHandler(DWORD dwCtrl)
{
    switch (dwCtrl)
    {
    case SERVICE_CONTROL_STOP: s_service->Stop(); break;
    case SERVICE_CONTROL_PAUSE: s_service->Pause(); break;
    case SERVICE_CONTROL_CONTINUE: s_service->Continue(); break;
    case SERVICE_CONTROL_SHUTDOWN: s_service->Shutdown(); break;
    case SERVICE_CONTROL_INTERROGATE: break;
    default: break;
    }
}

#pragma endregion


#pragma region Service Constructor and Destructor

//
//   FUNCTION: CServiceBase::CServiceBase(PWSTR, BOOL, BOOL, BOOL)
//
//   PURPOSE: The constructor of CServiceBase. It initializes a new instance 
//   of the CServiceBase class. The optional parameters (fCanStop, 
///  fCanShutdown and fCanPauseContinue) allow you to specify whether the 
//   service can be stopped, paused and continued, or be notified when system 
//   shutdown occurs.
//
//   PARAMETERS:
//   * pszServiceName - the name of the service
//   * fCanStop - the service can be stopped
//   * fCanShutdown - the service is notified when system shutdown occurs
//   * fCanPauseContinue - the service can be paused and continued
//
CServiceBase::CServiceBase(PSTR pszServiceName, 
                           BOOL fCanStop, 
                           BOOL fCanShutdown, 
                           BOOL fCanPauseContinue)
{
    // Service name must be a valid string and cannot be NULL.
    m_name = (pszServiceName == NULL) ? "" : pszServiceName;

    m_statusHandle = NULL;

    // The service runs in its own process.
    m_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

    // The service is starting.
    m_status.dwCurrentState = SERVICE_START_PENDING;

    // The accepted commands of the service.
    DWORD dwControlsAccepted = 0;
    if (fCanStop) 
        dwControlsAccepted |= SERVICE_ACCEPT_STOP;
    if (fCanShutdown) 
        dwControlsAccepted |= SERVICE_ACCEPT_SHUTDOWN;
    if (fCanPauseContinue) 
        dwControlsAccepted |= SERVICE_ACCEPT_PAUSE_CONTINUE;
    m_status.dwControlsAccepted = dwControlsAccepted;

    m_status.dwWin32ExitCode = NO_ERROR;
    m_status.dwServiceSpecificExitCode = 0;
    m_status.dwCheckPoint = 0;
    m_status.dwWaitHint = 0;
}


//
//   FUNCTION: CServiceBase::~CServiceBase()
//
//   PURPOSE: The virtual destructor of CServiceBase. 
//
CServiceBase::~CServiceBase(void)
{
}

#pragma endregion


#pragma region Service Start, Stop, Pause, Continue, and Shutdown

//
//   FUNCTION: CServiceBase::Start(DWORD, PWSTR *)
//
//   PURPOSE: The function starts the service. It calls the OnStart virtual 
//   function in which you can specify the actions to take when the service 
//   starts. If an error occurs during the startup, the error will be logged 
//   in the Application event log, and the service will be stopped.
//
//   PARAMETERS:
//   * dwArgc   - number of command line arguments
//   * lpszArgv - array of command line arguments
//
void CServiceBase::Start(DWORD dwArgc, PSTR *pszArgv)
{
    try
    {
        // Tell SCM that the service is starting.
        SetServiceStatus(SERVICE_START_PENDING);

        // Perform service-specific initialization.
        OnStart(dwArgc, pszArgv);

        // Tell SCM that the service is started.
        SetServiceStatus(SERVICE_RUNNING);
    }
    catch (DWORD dwError)
    {
        // Log the error.
        WriteErrorLogEntry("Service Start", dwError);

        // Set the service status to be stopped.
        SetServiceStatus(SERVICE_STOPPED, dwError);
    }
    catch (...)
    {
        // Log the error.
        WriteEventLogEntry("Service failed to start.", EVENTLOG_ERROR_TYPE);

        // Set the service status to be stopped.
        SetServiceStatus(SERVICE_STOPPED);
    }
}


//
//   FUNCTION: CServiceBase::OnStart(DWORD, PWSTR *)
//
//   PURPOSE: When implemented in a derived class, executes when a Start 
//   command is sent to the service by the SCM or when the operating system 
//   starts (for a service that starts automatically). Specifies actions to 
//   take when the service starts. Be sure to periodically call 
//   CServiceBase::SetServiceStatus() with SERVICE_START_PENDING if the 
//   procedure is going to take long time. You may also consider spawning a 
//   new thread in OnStart to perform time-consuming initialization tasks.
//
//   PARAMETERS:
//   * dwArgc   - number of command line arguments
//   * lpszArgv - array of command line arguments
//
void CServiceBase::OnStart(DWORD dwArgc, PSTR *pszArgv)
{
}


//
//   FUNCTION: CServiceBase::Stop()
//
//   PURPOSE: The function stops the service. It calls the OnStop virtual 
//   function in which you can specify the actions to take when the service 
//   stops. If an error occurs, the error will be logged in the Application 
//   event log, and the service will be restored to the original state.
//
void CServiceBase::Stop()
{
    DWORD dwOriginalState = m_status.dwCurrentState;
    try
    {
        // Tell SCM that the service is stopping.
        SetServiceStatus(SERVICE_STOP_PENDING);

        // Perform service-specific stop operations.
        OnStop();

        // Tell SCM that the service is stopped.
        SetServiceStatus(SERVICE_STOPPED);
    }
    catch (DWORD dwError)
    {
        // Log the error.
        WriteErrorLogEntry("Service Stop", dwError);

        // Set the orginal service status.
        SetServiceStatus(dwOriginalState);
    }
    catch (...)
    {
        // Log the error.
        WriteEventLogEntry("Service failed to stop.", EVENTLOG_ERROR_TYPE);

        // Set the orginal service status.
        SetServiceStatus(dwOriginalState);
    }
}


//
//   FUNCTION: CServiceBase::OnStop()
//
//   PURPOSE: When implemented in a derived class, executes when a Stop 
//   command is sent to the service by the SCM. Specifies actions to take 
//   when a service stops running. Be sure to periodically call 
//   CServiceBase::SetServiceStatus() with SERVICE_STOP_PENDING if the 
//   procedure is going to take long time. 
//
void CServiceBase::OnStop()
{
}


//
//   FUNCTION: CServiceBase::Pause()
//
//   PURPOSE: The function pauses the service if the service supports pause 
//   and continue. It calls the OnPause virtual function in which you can 
//   specify the actions to take when the service pauses. If an error occurs, 
//   the error will be logged in the Application event log, and the service 
//   will become running.
//
void CServiceBase::Pause()
{
    try
    {
        // Tell SCM that the service is pausing.
        SetServiceStatus(SERVICE_PAUSE_PENDING);

        // Perform service-specific pause operations.
        OnPause();

        // Tell SCM that the service is paused.
        SetServiceStatus(SERVICE_PAUSED);
    }
    catch (DWORD dwError)
    {
        // Log the error.
        WriteErrorLogEntry("Service Pause", dwError);

        // Tell SCM that the service is still running.
        SetServiceStatus(SERVICE_RUNNING);
    }
    catch (...)
    {
        // Log the error.
        WriteEventLogEntry("Service failed to pause.", EVENTLOG_ERROR_TYPE);

        // Tell SCM that the service is still running.
        SetServiceStatus(SERVICE_RUNNING);
    }
}


//
//   FUNCTION: CServiceBase::OnPause()
//
//   PURPOSE: When implemented in a derived class, executes when a Pause 
//   command is sent to the service by the SCM. Specifies actions to take 
//   when a service pauses.
//
void CServiceBase::OnPause()
{
}


//
//   FUNCTION: CServiceBase::Continue()
//
//   PURPOSE: The function resumes normal functioning after being paused if
//   the service supports pause and continue. It calls the OnContinue virtual 
//   function in which you can specify the actions to take when the service 
//   continues. If an error occurs, the error will be logged in the 
//   Application event log, and the service will still be paused.
//
void CServiceBase::Continue()
{
    try
    {
        // Tell SCM that the service is resuming.
        SetServiceStatus(SERVICE_CONTINUE_PENDING);

        // Perform service-specific continue operations.
        OnContinue();

        // Tell SCM that the service is running.
        SetServiceStatus(SERVICE_RUNNING);
    }
    catch (DWORD dwError)
    {
        // Log the error.
        WriteErrorLogEntry("Service Continue", dwError);

        // Tell SCM that the service is still paused.
        SetServiceStatus(SERVICE_PAUSED);
    }
    catch (...)
    {
        // Log the error.
        WriteEventLogEntry("Service failed to resume.", EVENTLOG_ERROR_TYPE);

        // Tell SCM that the service is still paused.
        SetServiceStatus(SERVICE_PAUSED);
    }
}


//
//   FUNCTION: CServiceBase::OnContinue()
//
//   PURPOSE: When implemented in a derived class, OnContinue runs when a 
//   Continue command is sent to the service by the SCM. Specifies actions to 
//   take when a service resumes normal functioning after being paused.
//
void CServiceBase::OnContinue()
{
}


//
//   FUNCTION: CServiceBase::Shutdown()
//
//   PURPOSE: The function executes when the system is shutting down. It 
//   calls the OnShutdown virtual function in which you can specify what 
//   should occur immediately prior to the system shutting down. If an error 
//   occurs, the error will be logged in the Application event log.
//
void CServiceBase::Shutdown()
{
    try
    {
        // Perform service-specific shutdown operations.
        OnShutdown();

        // Tell SCM that the service is stopped.
        SetServiceStatus(SERVICE_STOPPED);
    }
    catch (DWORD dwError)
    {
        // Log the error.
        WriteErrorLogEntry("Service Shutdown", dwError);
    }
    catch (...)
    {
        // Log the error.
        WriteEventLogEntry("Service failed to shut down.", EVENTLOG_ERROR_TYPE);
    }
}


//
//   FUNCTION: CServiceBase::OnShutdown()
//
//   PURPOSE: When implemented in a derived class, executes when the system 
//   is shutting down. Specifies what should occur immediately prior to the 
//   system shutting down.
//
void CServiceBase::OnShutdown()
{
}

#pragma endregion


#pragma region Helper Functions

//
//   FUNCTION: CServiceBase::SetServiceStatus(DWORD, DWORD, DWORD)
//
//   PURPOSE: The function sets the service status and reports the status to 
//   the SCM.
//
//   PARAMETERS:
//   * dwCurrentState - the state of the service
//   * dwWin32ExitCode - error code to report
//   * dwWaitHint - estimated time for pending operation, in milliseconds
//
void CServiceBase::SetServiceStatus(DWORD dwCurrentState, 
                                    DWORD dwWin32ExitCode, 
                                    DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure of the service.

    m_status.dwCurrentState = dwCurrentState;
    m_status.dwWin32ExitCode = dwWin32ExitCode;
    m_status.dwWaitHint = dwWaitHint;

    m_status.dwCheckPoint = 
        ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED)) ? 
        0 : dwCheckPoint++;

    // Report the status of the service to the SCM.
    ::SetServiceStatus(m_statusHandle, &m_status);
}


//
//   FUNCTION: CServiceBase::WriteEventLogEntry(PWSTR, WORD)
//
//   PURPOSE: Log a message to the Application event log.
//
//   PARAMETERS:
//   * pszMessage - string message to be logged.
//   * wType - the type of event to be logged. The parameter can be one of 
//     the following values.
//
//     EVENTLOG_SUCCESS
//     EVENTLOG_AUDIT_FAILURE
//     EVENTLOG_AUDIT_SUCCESS
//     EVENTLOG_ERROR_TYPE
//     EVENTLOG_INFORMATION_TYPE
//     EVENTLOG_WARNING_TYPE
//
void CServiceBase::WriteEventLogEntry(PSTR pszMessage, WORD wType)
{
    HANDLE hEventSource = NULL;
    LPCSTR lpszStrings[2] = { NULL, NULL };

    hEventSource = RegisterEventSource(NULL, m_name);
    if (hEventSource)
    {
        lpszStrings[0] = m_name;
        lpszStrings[1] = pszMessage;

        ReportEvent(hEventSource,  // Event log handle
            wType,                 // Event type
            0,                     // Event category
            0,                     // Event identifier
            NULL,                  // No security identifier
            2,                     // Size of lpszStrings array
            0,                     // No binary data
            lpszStrings,           // Array of strings
            NULL                   // No binary data
            );

        DeregisterEventSource(hEventSource);
    }
}


//
//   FUNCTION: CServiceBase::WriteErrorLogEntry(PWSTR, DWORD)
//
//   PURPOSE: Log an error message to the Application event log.
//
//   PARAMETERS:
//   * pszFunction - the function that gives the error
//   * dwError - the error code
//
void CServiceBase::WriteErrorLogEntry(PSTR pszFunction, DWORD dwError)
{
    char szMessage[260];
    StringCchPrintf(szMessage, ARRAYSIZE(szMessage), 
        "%s failed w/err 0x%08lx", pszFunction, dwError);
    WriteEventLogEntry(szMessage, EVENTLOG_ERROR_TYPE);
}

#pragma endregion