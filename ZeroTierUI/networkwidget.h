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

#ifndef NETWORK_H
#define NETWORK_H

#include <string>

#include <QWidget>
#include <QListWidgetItem>

namespace Ui {
class NetworkWidget;
}

class NetworkWidget : public QWidget
{
	Q_OBJECT

public:
	explicit NetworkWidget(QWidget *parent = 0,const std::string &nwid = std::string());
	virtual ~NetworkWidget();

	void setStatus(const std::string &status,const std::string &age);
	void setNetworkName(const std::string &name);
	void setNetworkType(const std::string &type);
	void setNetworkDeviceName(const std::string &dev);
	void setIps(const std::string &commaSeparatedList);

	const std::string &networkId();

private slots:
	void on_leaveNetworkButton_clicked();
	void on_networkIdButton_clicked();
	void on_ipListWidget_itemActivated(QListWidgetItem *item);
	void on_ipListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

private:
	Ui::NetworkWidget *ui;
	std::string networkIdStr;
	bool publicWarningShown;
};

#endif // NETWORK_H
