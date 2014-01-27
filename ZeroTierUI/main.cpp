/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#include "mainwindow.h"
#include "installdialog.h"
#include "licensedialog.h"

#include <QApplication>
#include <QDir>
#include <QString>
#include <QFont>

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <windows.h>
#endif

QSettings *settings = (QSettings *)0;

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

#ifdef __WINDOWS__
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
		id.show();
		return a.exec();
	}
#endif

#ifdef __APPLE__
	{
		// Put QSettings here because this is one of the writable directories allowed
		// in Apple's app store sandbox specs. We might end up in app store someday.
		QString zt1AppSupport(QDir::homePath() + "/Library/Application Support/ZeroTier/One");
		QDir::root().mkpath(zt1AppSupport);
		settings = new QSettings(zt1AppSupport + "/ui.ini",QSettings::IniFormat);
	}
#else
	settings = new QSettings("ZeroTier Networks","ZeroTier One");
#endif

	if (!settings->value("acceptedLicenseV1",false).toBool()) {
		LicenseDialog ld;
		ld.setStyleSheet(a.styleSheet());
		ld.exec();
	}

	MainWindow w;
	w.show();
	return a.exec();
}
