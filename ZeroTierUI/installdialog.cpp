/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2015  ZeroTier Networks
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

#include "installdialog.h"
#include "mainwindow.h"
#include "ui_installdialog.h"

#include "../node/Constants.hpp"
#include "../node/Defaults.hpp"
#include "../node/SoftwareUpdater.hpp"

#ifdef __UNIX_LIKE__
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#endif

#ifdef __APPLE__
#include "mac_doprivileged.h"
#endif

#include <QMainWindow>
#include <QMessageBox>
#include <QByteArray>
#include <QSslSocket>
#include <QFile>
#include <QDir>
#include <QProcess>

InstallDialog::InstallDialog(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::InstallDialog),
	nam(new QNetworkAccessManager(this)),
	phase(FETCHING_NFO)
{
	ui->setupUi(this);
	QObject::connect(nam,SIGNAL(finished(QNetworkReply*)),this,SLOT(on_networkReply(QNetworkReply*)));

	const char *nfoUrl = ZeroTier::ZT_DEFAULTS.updateLatestNfoURL.c_str();
	if (!*nfoUrl) {
		QMessageBox::critical(this,"Download Failed","Download failed: internal error: no update URL configured in build!",QMessageBox::Ok,QMessageBox::NoButton);
		QApplication::exit(1);
		return;
	}

	QNetworkReply *reply = nam->get(QNetworkRequest(QUrl(nfoUrl)));
	QObject::connect(reply,SIGNAL(downloadProgress(qint64,qint64)),this,SLOT(on_downloadProgress(qint64,qint64)));
}

InstallDialog::~InstallDialog()
{
	delete ui;
}

void InstallDialog::on_networkReply(QNetworkReply *reply)
{
	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError) {
		QMessageBox::critical(this,"Download Failed",QString("Download failed: ") + reply->errorString() + "\n\nAre you connected to the Internet?",QMessageBox::Ok,QMessageBox::NoButton);
		QApplication::exit(1);
	} else {
		if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) == 200) {
			QByteArray installerData(reply->readAll());

			switch(phase) {
				case FETCHING_NFO: {
					unsigned int vMajor = 0,vMinor = 0,vRevision = 0;
					installerData.append((char)0);
					const char *err = ZeroTier::SoftwareUpdater::parseNfo(installerData.data(),vMajor,vMinor,vRevision,signedBy,signature,url);

					if (err) {
						QMessageBox::critical(this,"Download Failed","Download failed: there is a problem with the software update web site.\nTry agian later. (invalid .nfo file)",QMessageBox::Ok,QMessageBox::NoButton);
						QApplication::exit(1);
						return;
					}

					phase = FETCHING_INSTALLER;
					reply = nam->get(QNetworkRequest(QUrl(url.c_str())));
					QObject::connect(reply,SIGNAL(downloadProgress(qint64,qint64)),this,SLOT(on_downloadProgress(qint64,qint64)));
				}	break;
				case FETCHING_INSTALLER: {
					if (!ZeroTier::SoftwareUpdater::validateUpdate(installerData.data(),installerData.length(),signedBy,signature)) {
						QMessageBox::critical(this,"Download Failed","Download failed: there is a problem with the software update web site. Try agian later. (downloaded data failed signature check)",QMessageBox::Ok,QMessageBox::NoButton);
						QApplication::exit(1);
						return;
					}

#ifdef __APPLE__
					{
						std::string homePath(QDir::homePath().toStdString());
						QString zt1Caches(QDir::homePath() + "/Library/Caches/ZeroTier/One");
						QDir::root().mkpath(zt1Caches);
						std::string instPath((zt1Caches + "/ZeroTierOneInstaller").toStdString());
						std::string tmpPath((zt1Caches + "/inst.sh").toStdString());

						int outfd = ::open(instPath.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0755);
						if (outfd <= 0) {
							QMessageBox::critical(this,"Download Failed",QString("Installation failed: unable to write to ")+instPath.c_str(),QMessageBox::Ok,QMessageBox::NoButton);
							QApplication::exit(1);
							return;
						}
						if (::write(outfd,installerData.data(),installerData.length()) != installerData.length()) {
							QMessageBox::critical(this,"Installation Failed",QString("Installation failed: unable to write to ")+instPath.c_str(),QMessageBox::Ok,QMessageBox::NoButton);
							QApplication::exit(1);
							return;
						}
						::close(outfd);
						chmod(instPath.c_str(),0755);

						FILE *scr = fopen(tmpPath.c_str(),"w");
						if (!scr) {
							QMessageBox::critical(this,"Installation Failed","Cannot write script to temporary Library/Caches/ZeroTier/One folder.",QMessageBox::Ok,QMessageBox::NoButton);
							QApplication::exit(1);
							return;
						}

						fprintf(scr,"#!/bin/bash\n");
						fprintf(scr,"export PATH=\"/bin:/usr/bin:/sbin:/usr/sbin\"\n");
						fprintf(scr,"'%s'\n",instPath.c_str());
						fprintf(scr,"if [ -f '/Library/Application Support/ZeroTier/One/authtoken.secret' ]; then\n");
						fprintf(scr,"  mkdir -p '%s/Library/Application Support/ZeroTier/One'\n",homePath.c_str());
						fprintf(scr,"  chown %d '%s/Library/Application Support/ZeroTier'\n",(int)getuid(),homePath.c_str());
						fprintf(scr,"  chgrp %d '%s/Library/Application Support/ZeroTier'\n",(int)getgid(),homePath.c_str());
						fprintf(scr,"  chmod 0700 '%s/Library/Application Support/ZeroTier'\n",homePath.c_str());
						fprintf(scr,"  chown %d '%s/Library/Application Support/ZeroTier/One'\n",(int)getuid(),homePath.c_str());
						fprintf(scr,"  chgrp %d '%s/Library/Application Support/ZeroTier/One'\n",(int)getgid(),homePath.c_str());
						fprintf(scr,"  chmod 0700 '%s/Library/Application Support/ZeroTier/One'\n",homePath.c_str());
						fprintf(scr,"  cp -f '/Library/Application Support/ZeroTier/One/authtoken.secret' '%s/Library/Application Support/ZeroTier/One/authtoken.secret'\n",homePath.c_str());
						fprintf(scr,"  chown %d '%s/Library/Application Support/ZeroTier/One/authtoken.secret'\n",(int)getuid(),homePath.c_str());
						fprintf(scr,"  chgrp %d '%s/Library/Application Support/ZeroTier/One/authtoken.secret'\n",(int)getgid(),homePath.c_str());
						fprintf(scr,"  chmod 0600 '%s/Library/Application Support/ZeroTier/One/authtoken.secret'\n",homePath.c_str());
						fprintf(scr,"fi\n");
						fprintf(scr,"exit 0\n");

						fclose(scr);
						chmod(tmpPath.c_str(),0755);

						macExecutePrivilegedShellCommand((std::string("'")+tmpPath+"' >>/dev/null 2>&1").c_str());

						unlink(tmpPath.c_str());
						unlink(instPath.c_str());

						// Restart the binary with whatever updates may have occurred
						std::string appPath(QCoreApplication::applicationFilePath().toStdString());
						execl(appPath.c_str(),appPath.c_str(),(const char *)0);

						// We only make it here if execl() fails
						QMessageBox::critical(this,"Re-Launch Failed","An error occurred re-launching ZeroTier One.app. Try launching it manually.",QMessageBox::Ok,QMessageBox::NoButton);
						QApplication::exit(1);

						return;
					}
#endif
				}	break;
			}

			ui->progressBar->setMinimum(0);
			ui->progressBar->setMaximum(100);
			ui->progressBar->setValue(0);
		} else {
			QMessageBox::critical(this,"Download Failed",QString("Download failed: HTTP status code ") + reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString(),QMessageBox::Ok,QMessageBox::NoButton);
			QApplication::exit(1);
		}
	}
}

void InstallDialog::on_InstallDialog_rejected()
{
	QApplication::exit();
}

void InstallDialog::on_cancelButton_clicked()
{
	QApplication::exit();
}

void InstallDialog::on_downloadProgress(qint64 bytesReceived,qint64 bytesTotal)
{
	if (bytesTotal <= 0) {
		ui->progressBar->setValue(0);
		ui->progressBar->setMinimum(0);
		ui->progressBar->setMaximum(0);
	} else {
		double pct = ((double)bytesReceived / (double)bytesTotal) * 100.0;
		if (pct > 100.0)
			pct = 100.0;
		ui->progressBar->setMinimum(0);
		ui->progressBar->setMaximum(100);
		ui->progressBar->setValue((int)pct);
	}
}
