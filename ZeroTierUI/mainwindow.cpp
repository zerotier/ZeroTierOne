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
#include "aboutwindow.h"
#include "networkwidget.h"
#include "ui_mainwindow.h"
#include "installdialog.h"

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

QNetworkAccessManager *nam;

// Globally visible
ZeroTier::Node::LocalClient *zeroTierClient = (ZeroTier::Node::LocalClient *)0;

// Main window instance for app
static MainWindow *mainWindow = (MainWindow *)0;

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
	ui->setupUi(this);
	if (ui->networkListWidget->verticalScrollBar())
		ui->networkListWidget->verticalScrollBar()->setSingleStep(8);
	QWidgetList widgets = this->findChildren<QWidget*>();
	foreach(QWidget *widget, widgets)
		widget->setAttribute(Qt::WA_MacShowFocusRect,false);

	mainWindow = this;

	this->pollServiceTimerId = this->startTimer(1000);
	this->setEnabled(false); // gets enabled when updates are received
	this->cyclesSinceResponseFromService = 0;
}

MainWindow::~MainWindow()
{
	delete ui;
	delete zeroTierClient;
	zeroTierClient = (ZeroTier::Node::LocalClient *)0;
	mainWindow = (MainWindow *)0;
}

void MainWindow::timerEvent(QTimerEvent *event)
{
	event->accept();

	if (this->isHidden())
		return;
	if (pollServiceTimerId < 0)
		return;

	if (!zeroTierClient) {
		std::string authToken;
		if (!ZeroTier::Utils::readFile(ZeroTier::Node::LocalClient::authTokenDefaultUserPath().c_str(),authToken)) {
#ifdef __APPLE__
			if (QFile::exists("/Library/Application Support/ZeroTier/One/zerotier-one")) {
				QMessageBox::information(this,"Authorization Required","You must authenticate to authorize this user to administrate ZeroTier One on this computer.\n\n(This only needs to be done once.)",QMessageBox::Ok,QMessageBox::NoButton);
				QString authHelperPath(QCoreApplication::applicationDirPath() + "/../Resources/helpers/mac/ZeroTier One (Authenticate).app/Contents/MacOS/applet");
				if (!QFile::exists(authHelperPath)) {
					QMessageBox::critical(this,"Unable to Locate Helper","Unable to locate authorization helper, cannot obtain authentication token.",QMessageBox::Ok,QMessageBox::NoButton);
					QApplication::exit(1);
					return;
				}
				QProcess::execute(authHelperPath,QStringList());
			} else {
				doInstallDialog();
				return;
			}
#endif

			if (!ZeroTier::Utils::readFile(ZeroTier::Node::LocalClient::authTokenDefaultUserPath().c_str(),authToken)) {
				QMessageBox::critical(this,"Cannot Authorize","Unable to authorize this user to administrate ZeroTier One. (Did you enter your password correctly?)",QMessageBox::Ok,QMessageBox::NoButton);
				QApplication::exit(1);
				return;
			}
		}

		zeroTierClient = new ZeroTier::Node::LocalClient(authToken.c_str(),0,&handleZTMessage,this);
	}

	// TODO: do something more user-friendly here... or maybe try to restart
	// the service?
	if (++this->cyclesSinceResponseFromService == 4)
		QMessageBox::critical(this,"No Response from Service","The ZeroTier One service does not appear to be running.",QMessageBox::Ok,QMessageBox::NoButton);

	zeroTierClient->send("info");
	zeroTierClient->send("listnetworks");
	zeroTierClient->send("listpeers");
}

void MainWindow::customEvent(QEvent *event)
{
	ZTMessageEvent *m = (ZTMessageEvent *)event; // only one custom event type so far
	if (m->ztMessage.size() == 0)
		return;

	std::vector<std::string> hdr(ZeroTier::Node::LocalClient::splitLine(m->ztMessage[0]));
	if (hdr.size() < 2)
		return;
	if (hdr[0] != "200")
		return;

	this->cyclesSinceResponseFromService = 0;

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

	ui->noNetworksLabel->setVisible(ui->networkListWidget->count() == 0);

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

	if (this->myStatus == "ONLINE") {
		if (!this->isEnabled())
			this->setEnabled(true);
	} else {
		if (this->isEnabled())
			this->setEnabled(false);
	}
}

void MainWindow::showEvent(QShowEvent *event)
{
#ifdef __APPLE__
	if (!QFile::exists("/Library/Application Support/ZeroTier/One/zerotier-one"))
		doInstallDialog();
#endif
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

void MainWindow::doInstallDialog()
{
#ifdef __APPLE__
	this->setEnabled(false);
	if (pollServiceTimerId >= 0) {
		this->killTimer(pollServiceTimerId);
		pollServiceTimerId = -1;
	}

	InstallDialog *id = new InstallDialog(this);
	id->setModal(true);
	id->show();
#endif
}
