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

#include <string>
#include <map>
#include <set>
#include <vector>
#include <stdexcept>
#include <utility>

#include <QClipboard>
#include <QMutex>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QDebug>
#include <QProcess>
#include <QStringList>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QEventLoop>
#include <QFont>

#include "main.h"
#include "mainwindow.h"
#include "aboutwindow.h"
#include "networkwidget.h"
#include "ui_mainwindow.h"
#include "ui_quickstartdialog.h"

#ifdef __APPLE__
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "mac_doprivileged.h"
#endif

// Globally visible
ZeroTier::Node::LocalClient *zeroTierClient = (ZeroTier::Node::LocalClient *)0;

// Main window instance for app
QMainWindow *mainWindow = (MainWindow *)0;

// Handles message from ZeroTier One service
static void handleZTMessage(void *arg,unsigned long id,const char *line)
{
	static std::map< unsigned long,std::vector<std::string> > ztReplies;
	static QMutex ztReplies_m;

	ztReplies_m.lock();
	if (*line) {
		ztReplies[id].push_back(std::string(line));
		ztReplies_m.unlock();
	} else { // empty lines conclude transmissions
		std::map< unsigned long,std::vector<std::string> >::iterator r(ztReplies.find(id));
		if (r != ztReplies.end()) {
			// The message is packed into an event and sent to the main window where
			// the actual parsing code lives.
			MainWindow::ZTMessageEvent *event = new MainWindow::ZTMessageEvent(r->second);
			ztReplies.erase(r);
			ztReplies_m.unlock();
			QCoreApplication::postEvent(mainWindow,event); // must post since this may be another thread
		} else ztReplies_m.unlock();
	}
}

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow),
	pollServiceTimerId(-1)
{
	mainWindow = this;

	ui->setupUi(this);
	if (ui->networkListWidget->verticalScrollBar())
		ui->networkListWidget->verticalScrollBar()->setSingleStep(8);

#ifdef __APPLE__
	QWidgetList widgets = this->findChildren<QWidget*>();
	foreach(QWidget *widget, widgets)
		widget->setAttribute(Qt::WA_MacShowFocusRect,false);
#endif

#ifdef __WINDOWS__
	// Windows operates at a different DPI, so we have to rescale the default Qt
	// font sizes so everything isn't huge. Yeah.
	QWidgetList widgets = this->findChildren<QWidget*>();
	foreach(QWidget *widget, widgets) {
		if (typeid(*widget) != typeid(*ui->menuFile)) { // menus don't need the DPI shift apparently
			QFont font(widget->font());
			font.setPointSizeF(font.pointSizeF() * 0.75);
			widget->setFont(font);
		}
	}
#endif

	ui->noNetworksLabel->setVisible(true);
	ui->noNetworksLabel->setText("Connecting to Service...");
	ui->bottomContainerWidget->setVisible(false);
	ui->networkListWidget->setVisible(false);

	this->firstTimerTick = true;
	this->pollServiceTimerId = this->startTimer(200);
	this->cyclesSinceResponseFromService = 0;
}

MainWindow::~MainWindow()
{
	delete ui;
	delete zeroTierClient;
	zeroTierClient = (ZeroTier::Node::LocalClient *)0;
	mainWindow = (MainWindow *)0;
}

void MainWindow::timerEvent(QTimerEvent *event) // event can be null since code also calls this directly
{
	if (this->isHidden())
		return;
	if (this->pollServiceTimerId < 0)
		return;

	if (this->firstTimerTick) {
		this->firstTimerTick = false;
		this->killTimer(this->pollServiceTimerId);

		if (!settings->value("shown_quickStart",false).toBool()) {
			on_actionQuick_Start_triggered();
			settings->setValue("shown_quickStart",true);
			settings->sync();
		}

		this->pollServiceTimerId = this->startTimer(1500);
	}

	if (!zeroTierClient) {
		std::string authToken;
		if (!ZeroTier::Utils::readFile(ZeroTier::Node::LocalClient::authTokenDefaultUserPath().c_str(),authToken)) {
#ifdef __APPLE__
			if (QFile::exists("/Library/Application Support/ZeroTier/One/zerotier-one")) {
				// Authorize user by copying auth token into local home directory
				QMessageBox::information(this,"Authorization Needed","Administrator privileges are required to allow the current user to control ZeroTier One on this computer. (You only have to do this once.)",QMessageBox::Ok,QMessageBox::NoButton);

				std::string homePath(QDir::homePath().toStdString());
				QString zt1Caches(QDir::homePath() + "/Library/Caches/ZeroTier/One");
				QDir::root().mkpath(zt1Caches);
				std::string tmpPath((zt1Caches + "/auth.sh").toStdString());

				FILE *scr = fopen(tmpPath.c_str(),"w");
				if (!scr) {
					QMessageBox::critical(this,"Cannot Authorize","Unable to authorize this user to administrate ZeroTier One. (Cannot write to temporary Library/Caches/ZeroTier/One folder.)",QMessageBox::Ok,QMessageBox::NoButton);
					QApplication::exit(1);
					return;
				}

				fprintf(scr,"#!/bin/bash\n");
				fprintf(scr,"export PATH=\"/bin:/usr/bin:/sbin:/usr/sbin\"\n");
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
			}
#endif

			if (!ZeroTier::Utils::readFile(ZeroTier::Node::LocalClient::authTokenDefaultUserPath().c_str(),authToken)) {
				if (!ZeroTier::Utils::readFile(ZeroTier::Node::LocalClient::authTokenDefaultSystemPath().c_str(),authToken)) {
					QMessageBox::critical(this,"Cannot Authorize","Unable to authorize this user to administrate ZeroTier One. (Did you enter your password correctly?)",QMessageBox::Ok,QMessageBox::NoButton);
					QApplication::exit(1);
					return;
				}
			}
		}

		zeroTierClient = new ZeroTier::Node::LocalClient(authToken.c_str(),0,&handleZTMessage,this);
	}

	if (++this->cyclesSinceResponseFromService >= 3) {
		if (this->cyclesSinceResponseFromService == 3)
			QMessageBox::warning(this,"Service Not Running","Can't connect to the ZeroTier One service. Is it running?",QMessageBox::Ok);
		ui->noNetworksLabel->setVisible(true);
		ui->noNetworksLabel->setText("Connecting to Service...");
		ui->bottomContainerWidget->setVisible(false);
		ui->networkListWidget->setVisible(false);
	}

	zeroTierClient->send("info");
	zeroTierClient->send("listnetworks");
	zeroTierClient->send("listpeers");
}

void MainWindow::customEvent(QEvent *event)
{
	ZTMessageEvent *m = (ZTMessageEvent *)event; // only one custom event type so far
	if (m->ztMessage.size() == 0)
		return;

	this->cyclesSinceResponseFromService = 0;

	std::vector<std::string> hdr(ZeroTier::Node::LocalClient::splitLine(m->ztMessage[0]));
	if (hdr.size() < 2)
		return;
	if (hdr[0] != "200")
		return;

	if (hdr[1] == "info") {
		if (hdr.size() >= 3)
			this->myAddress = hdr[2].c_str();
		if (hdr.size() >= 4)
			this->myStatus = hdr[3].c_str();
		if (hdr.size() >= 5)
			this->myVersion = hdr[4].c_str();
	} else if (hdr[1] == "listnetworks") {
		std::map< std::string,std::vector<std::string> > newNetworks;
		for(unsigned long i=1;i<m->ztMessage.size();++i) {
			std::vector<std::string> l(ZeroTier::Node::LocalClient::splitLine(m->ztMessage[i]));
			// 200 listnetworks <nwid> <name> <status> <config age> <type> <dev> <ips>
			if ((l.size() == 9)&&(l[2].length() == 16))
				newNetworks[l[2]] = l;
		}

		if (newNetworks != networks) {
			networks = newNetworks;

			for (bool removed=true;removed;) {
				removed = false;
				for(int r=0;r<ui->networkListWidget->count();++r) {
					NetworkWidget *nw = (NetworkWidget *)ui->networkListWidget->itemWidget(ui->networkListWidget->item(r));
					if (!networks.count(nw->networkId())) {
						ui->networkListWidget->setVisible(false); // HACK to prevent an occasional crash here, discovered through hours of shotgun debugging... :P
						delete ui->networkListWidget->takeItem(r);
						removed = true;
						break;
					}
				}
			}
			ui->networkListWidget->setVisible(true);
			std::set<std::string> alreadyDisplayed;
			for(int r=0;r<ui->networkListWidget->count();++r) {
				NetworkWidget *nw = (NetworkWidget *)ui->networkListWidget->itemWidget(ui->networkListWidget->item(r));
				if (networks.count(nw->networkId()) > 0) {
					alreadyDisplayed.insert(nw->networkId());
					std::vector<std::string> &l = networks[nw->networkId()];
					nw->setNetworkName(l[3]);
					nw->setStatus(l[4],l[5]);
					nw->setNetworkType(l[6]);
					nw->setNetworkDeviceName(l[7]);
					nw->setIps(l[8]);
				}
			}
			for(std::map< std::string,std::vector<std::string> >::iterator nwdata(networks.begin());nwdata!=networks.end();++nwdata) {
				if (alreadyDisplayed.count(nwdata->first) == 0) {
					std::vector<std::string> &l = nwdata->second;
					NetworkWidget *nw = new NetworkWidget((QWidget *)0,nwdata->first);
					nw->setNetworkName(l[3]);
					nw->setStatus(l[4],l[5]);
					nw->setNetworkType(l[6]);
					nw->setNetworkDeviceName(l[7]);
					nw->setIps(l[8]);
					QListWidgetItem *item = new QListWidgetItem();
					item->setSizeHint(nw->sizeHint());
					ui->networkListWidget->addItem(item);
					ui->networkListWidget->setItemWidget(item,nw);
				}
			}
		}
	} else if (hdr[1] == "listpeers") {
		this->numPeers = 0;
		for(unsigned long i=1;i<m->ztMessage.size();++i) {
			std::vector<std::string> l(ZeroTier::Node::LocalClient::splitLine(m->ztMessage[i]));
			if ((l.size() >= 5)&&((l[3] != "-")||(l[4] != "-")))
				++this->numPeers; // number of direct peers online -- check for active IPv4 and/or IPv6 address
		}
	}

	if (!ui->networkListWidget->count()) {
		ui->noNetworksLabel->setText("You Have Not Joined Any Networks");
		ui->noNetworksLabel->setVisible(true);
	} else ui->noNetworksLabel->setVisible(false);

	if (!ui->bottomContainerWidget->isVisible())
		ui->bottomContainerWidget->setVisible(true);
	if (!ui->networkListWidget->isVisible())
		ui->networkListWidget->setVisible(true);

	if (this->myAddress.size())
		ui->addressButton->setText(this->myAddress);
	else ui->addressButton->setText("          ");

	QString st(this->myStatus);
	st += ", v";
	st += this->myVersion;
	st += ", ";
	st += QString::number(this->numPeers);
	st += " direct links to peers";
	ui->statusLabel->setText(st);
}

void MainWindow::on_joinNetworkButton_clicked()
{
	QString toJoin(ui->networkIdLineEdit->text());
	ui->networkIdLineEdit->setText(QString());

	if (!zeroTierClient) // sanity check
		return;

	if (toJoin.size() != 16) {
		QMessageBox::information(this,"Invalid Network ID","The network ID you entered was not valid. Enter a 16-digit hexadecimal network ID, like '8056c2e21c000001'.",QMessageBox::Ok,QMessageBox::NoButton);
		return;
	}

	zeroTierClient->send((QString("join ") + toJoin).toStdString());
}

void MainWindow::on_actionAbout_triggered()
{
	AboutWindow *about = new AboutWindow(this);
	about->show();
}

void MainWindow::on_networkIdLineEdit_textChanged(const QString &text)
{
	QString newText;
	for(QString::const_iterator i(text.begin());i!=text.end();++i) {
		switch(i->toLatin1()) {
			case '0': newText.append('0'); break;
			case '1': newText.append('1'); break;
			case '2': newText.append('2'); break;
			case '3': newText.append('3'); break;
			case '4': newText.append('4'); break;
			case '5': newText.append('5'); break;
			case '6': newText.append('6'); break;
			case '7': newText.append('7'); break;
			case '8': newText.append('8'); break;
			case '9': newText.append('9'); break;
			case 'a': newText.append('a'); break;
			case 'b': newText.append('b'); break;
			case 'c': newText.append('c'); break;
			case 'd': newText.append('d'); break;
			case 'e': newText.append('e'); break;
			case 'f': newText.append('f'); break;
			case 'A': newText.append('a'); break;
			case 'B': newText.append('b'); break;
			case 'C': newText.append('c'); break;
			case 'D': newText.append('d'); break;
			case 'E': newText.append('e'); break;
			case 'F': newText.append('f'); break;
			default: break;
		}
	}
	if (newText.size() > 16)
		newText.truncate(16);
	ui->networkIdLineEdit->setText(newText);
}

void MainWindow::on_addressButton_clicked()
{
	QApplication::clipboard()->setText(this->myAddress);
}

void MainWindow::on_actionQuick_Start_triggered()
{
	Ui::QuickstartDialog qd;
	QDialog *qdd = new QDialog(this);
	qd.setupUi(qdd);
	qdd->setModal(false);
	qdd->show();
}
