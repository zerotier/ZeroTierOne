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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mainwindow.h"
#include "installdialog.h"
#include "licensedialog.h"

#include <QApplication>
#include <QDir>
#include <QString>
#include <QFont>
#include <QMessageBox>

#include "../node/Constants.hpp"
#include "../node/Defaults.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <windows.h>
#include "../windows/ZeroTierOne/ZeroTierOneService.h"

// Returns true if started or already running, false if failed or not installed
static bool startWindowsService()
{
	SERVICE_STATUS ssSvcStatus;
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (schSCManager == NULL)
		return false;

	schService = OpenService(schSCManager, ZT_SERVICE_NAME, SERVICE_QUERY_STATUS | SERVICE_START);
	if (schService == NULL) {
		CloseServiceHandle(schSCManager);
		return false;
	}

	int tries = 0;
	bool running = true;

	for(;;) {
		memset(&ssSvcStatus,0,sizeof(ssSvcStatus));
		if ((++tries > 20)||(!QueryServiceStatus(schService,&ssSvcStatus))) {
			running = false;
			break;
		}

		if (ssSvcStatus.dwCurrentState == SERVICE_RUNNING) {
			break;
		} else if (ssSvcStatus.dwCurrentState == SERVICE_START_PENDING) {
			Sleep(500);
			continue;
		}

		memset(&ssSvcStatus,0,sizeof(ssSvcStatus));
		ControlService(schService, SERVICE_CONTROL_START, &ssSvcStatus);
		Sleep(500);
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
	return running;
}
#endif // __WINDOWS__

// Globally visible settings for the app
QSettings *settings = (QSettings *)0;

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

#ifdef __WINDOWS__
	// Start up Winsock2
	{
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2,2),&wsaData);
	}
#endif

	{
		QFile qss(":css/stylesheet.css");
		qss.open(QFile::ReadOnly);
		QString style(qss.readAll());
		a.setStyleSheet(style);
	}

#ifdef __APPLE__
	// If service isn't installed, download and install it
	if (!QFile::exists("/Library/Application Support/ZeroTier/One/zerotier-one")) {
		// InstallDialog is an alternative main window. It will re-launch the app
		// when done.
		InstallDialog id;
		id.setStyleSheet(a.styleSheet());
		id.show();
		return a.exec();
	}

	{
		// Put QSettings here because this is one of the writable directories allowed
		// in Apple's app store sandbox specs. We might end up in app store someday.
		QString zt1AppSupport(QDir::homePath() + "/Library/Application Support/ZeroTier/One");
		QDir::root().mkpath(zt1AppSupport);
		settings = new QSettings(zt1AppSupport + "/ui.ini",QSettings::IniFormat);
	}
#else // on non-Apple boxen put it in the standard place using the default format
	settings = new QSettings("ZeroTier Networks","ZeroTier One");
#endif

	if (!settings->value("acceptedLicenseV1",false).toBool()) {
		LicenseDialog ld;
		ld.setStyleSheet(a.styleSheet());
		ld.exec();
	}

#ifdef __WINDOWS__
	{
		bool winSvcInstalled = false;
		while (!startWindowsService()) {
			if (winSvcInstalled) {
				// Service was installed and subsequently failed to start again, so
				// something is wrong!
				QMessageBox::critical((QWidget *)0,"Service Not Available","Unable to locate or start ZeroTier One service. There may be a problem with the installation. Try installing from the .msi file again or e-mail contact@zerotier.com if you cannot install. (Error: service failed to start)",QMessageBox::Ok);
				return 1;
			}

#ifdef _WIN64
			BOOL is64Bit = TRUE;
#else
			BOOL is64Bit = FALSE;
			IsWow64Process(GetCurrentProcess(),&is64Bit);
#endif
			std::string exe(ZeroTier::ZT_DEFAULTS.defaultHomePath + "\\zerotier-one_");
			exe.append((is64Bit == TRUE) ? "x64.exe" : "x86.exe");

			if (QFile::exists(exe.c_str())) {
				STARTUPINFOA si;
				PROCESS_INFORMATION pi;
				memset(&si,0,sizeof(si));
				memset(&pi,0,sizeof(pi));
				if (CreateProcessA(NULL,const_cast <LPSTR>((exe + " -I").c_str()),NULL,NULL,FALSE,CREATE_NO_WINDOW|CREATE_NEW_PROCESS_GROUP,NULL,NULL,&si,&pi)) {
					WaitForSingleObject(pi.hProcess,INFINITE);
					CloseHandle(pi.hProcess);
					CloseHandle(pi.hThread);
					winSvcInstalled = true;
				}
			}

			if (!winSvcInstalled) {
				// Service failed to install -- installation problem like missing .exe
				QMessageBox::critical((QWidget *)0,"Service Not Available","Unable to locate or start ZeroTier One service. There may be a problem with the installation. Try installing from the .msi file again or e-mail contact@zerotier.com if you cannot install. (Error: service not installed)",QMessageBox::Ok);
				return 1;
			}
		}
	}
#endif

	MainWindow w;
	w.show();
	return a.exec();
}
