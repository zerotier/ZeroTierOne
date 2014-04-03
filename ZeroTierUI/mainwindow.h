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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QEvent>
#include <QString>
#include <QShowEvent>
#include <QTimerEvent>
#include <QSettings>

#include <map>
#include <vector>
#include <string>

#include "../node/Constants.hpp"
#include "../node/Node.hpp"
#include "../node/Utils.hpp"

namespace Ui {
class MainWindow;
}

// Globally visible instance of local client for communicating with ZT1
// Can be null if not connected, or will point to current
extern ZeroTier::Node::NodeControlClient *zeroTierClient;

// Globally visible pointer to main app window
extern QMainWindow *mainWindow;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	// Event used to pass messages from the Node::LocalClient thread to the
	// main window to update network lists and stats.
	class ZTMessageEvent : public QEvent
	{
	public:
		ZTMessageEvent(const std::vector<std::string> &m) :
			QEvent(QEvent::User),
			ztMessage(m) {}
		std::vector<std::string> ztMessage;
	};

	explicit MainWindow(QWidget *parent = 0);
	virtual ~MainWindow();

protected:
	virtual void timerEvent(QTimerEvent *event);
	virtual void customEvent(QEvent *event);

private slots:
	void on_joinNetworkButton_clicked();
	void on_actionAbout_triggered();
	void on_networkIdLineEdit_textChanged(const QString &text);
	void on_addressButton_clicked();
	void on_actionQuick_Start_triggered();

private:
	Ui::MainWindow *ui;

	QString myAddress;
	QString myStatus;
	QString myVersion;
	bool firstTimerTick;
	int pollServiceTimerId;
	unsigned int numPeers;
	unsigned int cyclesSinceResponseFromService;
	std::map< std::string,std::vector<std::string> > networks;
};

#endif // MAINWINDOW_H
