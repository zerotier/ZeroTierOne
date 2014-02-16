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

#ifndef INSTALLDIALOG_H
#define INSTALLDIALOG_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <string>

#include "../node/Address.hpp"

namespace Ui {
class InstallDialog;
}

class InstallDialog : public QMainWindow
{
	Q_OBJECT

public:
	explicit InstallDialog(QWidget *parent = 0);
	~InstallDialog();

private slots:
	void on_networkReply(QNetworkReply *reply);
	void on_InstallDialog_rejected();
	void on_cancelButton_clicked();
	void on_downloadProgress(qint64 bytesReceived,qint64 bytesTotal);

private:
	Ui::InstallDialog *ui;
	QNetworkAccessManager *nam;
	enum {
		FETCHING_NFO,
		FETCHING_INSTALLER
	} phase;

	ZeroTier::Address signedBy;
	std::string url,signature;
};

#endif // INSTALLDIALOG_H
