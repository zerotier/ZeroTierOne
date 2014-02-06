/****************************** Module Header ******************************\
* Module Name:  SampleService.h
* Project:      CppWindowsService
* Copyright (c) Microsoft Corporation.
* 
* Provides a sample service class that derives from the service base class - 
* CServiceBase. The sample service logs the service start and stop 
* information to the Application event log, and shows how to run the main 
* function of the service in a thread pool worker thread.
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

#include "ServiceBase.h"

#define ZT_SERVICE_NAME "ZeroTierOneService"
#define ZT_SERVICE_DISPLAY_NAME "ZeroTier One"
#define ZT_SERVICE_START_TYPE SERVICE_AUTO_START
#define ZT_SERVICE_DEPENDENCIES ""
#define ZT_SERVICE_ACCOUNT "NT AUTHORITY\\LocalService"
#define ZT_SERVICE_PASSWORD NULL

class ZeroTierOneService : public CServiceBase
{
public:
    ZeroTierOneService();
    virtual ~ZeroTierOneService(void);

protected:
    virtual void OnStart(DWORD dwArgc, PSTR *pszArgv);
    virtual void OnStop();
    virtual void OnPause();
    virtual void OnContinue();
};
