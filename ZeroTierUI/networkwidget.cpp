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

#include "networkwidget.h"
#include "mainwindow.h"
#include "ui_networkwidget.h"
#include "onetimedialog.h"
#include "main.h"

#include <QClipboard>
#include <QString>
#include <QStringList>
#include <QCoreApplication>
#include <QProcess>
#include <QList>
#include <QMessageBox>
#include <QFont>

#include "../node/Constants.hpp"

NetworkWidget::NetworkWidget(QWidget *parent,const std::string &nwid) :
	QWidget(parent),
	ui(new Ui::NetworkWidget),
	networkIdStr(nwid),
	publicWarningShown(false)
{
	ui->setupUi(this);
	ui->networkIdButton->setText(QString(nwid.c_str()));

	QFontMetrics fm(ui->ipListWidget->font());
	int lineHeight = ui->ipListWidget->spacing() + fm.height();
	ui->ipListWidget->setMinimumHeight(lineHeight * 6);
	ui->ipListWidget->setMaximumHeight(lineHeight * 6);

#ifdef __APPLE__
	QWidgetList widgets = this->findChildren<QWidget*>();
	foreach(QWidget* widget, widgets)
		widget->setAttribute(Qt::WA_MacShowFocusRect,false);
#endif

#ifdef __WINDOWS__
	QWidgetList widgets = this->findChildren<QWidget*>();
	foreach(QWidget *widget, widgets) {
		QFont font(widget->font());
		font.setPointSizeF(font.pointSizeF() * 0.75);
		widget->setFont(font);
	}
#endif
}

NetworkWidget::~NetworkWidget()
{
	delete ui;
}

void NetworkWidget::setStatus(const std::string &status,const std::string &age)
{
	ui->statusLabel->setText(QString(status.c_str()));
}

void NetworkWidget::setNetworkName(const std::string &name)
{
	if (name == "?") {
		ui->nameLabel->setText("... waiting ...");
		ui->nameLabel->setEnabled(false);
	} else {
		ui->nameLabel->setText(QString(name.c_str()));
		ui->nameLabel->setEnabled(true);
	}
}

void NetworkWidget::setNetworkType(const std::string &type)
{
	ui->networkTypeLabel->setText(QString(type.c_str()));
	if (type == "?")
		ui->networkTypeLabel->setStatusTip("Waiting for configuration...");
	else if (type == "public") {
		if ((!publicWarningShown)&&(!settings->value("shown_publicWarning",false).toBool())) {
			publicWarningShown = true;
			OneTimeDialog *d = new OneTimeDialog(mainWindow,"shown_publicWarning","Security Notice","Security Notice:"ZT_EOL_S""ZT_EOL_S"You have joined a public network. Anyone can join these. We recommend making sure that your system's automatic software updates are enabled and turning off any shared network services that you do not want people to access.");
			d->setModal(false);
			d->show();
		}
		ui->networkTypeLabel->setStatusTip("This network can be joined by anyone in the world.");
	} else if (type == "private")
		ui->networkTypeLabel->setStatusTip("This network is private; only authorized peers can join.");
	else ui->networkTypeLabel->setStatusTip("Unknown network type.");
}

void NetworkWidget::setNetworkDeviceName(const std::string &dev)
{
	ui->deviceLabel->setText(QString(dev.c_str()));
}

void NetworkWidget::setIps(const std::string &commaSeparatedList)
{
	QStringList ips(QString(commaSeparatedList.c_str()).split(QChar(','),QString::SkipEmptyParts));
	if (commaSeparatedList == "-")
		ips.clear();

	QStringList tmp;
	ips.sort();
	for(QStringList::iterator i(ips.begin());i!=ips.end();++i) {
		QString ipOnly(*i);
		int slashIdx = ipOnly.indexOf('/');
		if (slashIdx > 0)
			ipOnly.truncate(slashIdx);
		tmp.append(ipOnly);
	}
	ips = tmp;

	for(QStringList::iterator i(ips.begin());i!=ips.end();++i) {
		if (ui->ipListWidget->findItems(*i,Qt::MatchCaseSensitive).size() == 0)
			ui->ipListWidget->addItem(*i);
	}

	for(int i=0;i<ui->ipListWidget->count();++i) {
		QListWidgetItem *item = ui->ipListWidget->item(i);
		if (!ips.contains(item->text()))
			ui->ipListWidget->removeItemWidget(item);
	}
}

void NetworkWidget::setMAC(const std::string &mac)
{
	ui->macLabel->setText(QString(mac.c_str()));
}

const std::string &NetworkWidget::networkId()
{
	return networkIdStr;
}

void NetworkWidget::on_leaveNetworkButton_clicked()
{
	if (QMessageBox::question(this,"Leave Network?",QString("Are you sure you want to leave network '") + networkIdStr.c_str() + "'?",QMessageBox::No,QMessageBox::Yes) == QMessageBox::Yes) {
		this->setEnabled(false);
		zeroTierClient->send((QString("leave ") + networkIdStr.c_str()).toStdString());
	}
}

void NetworkWidget::on_networkIdButton_clicked()
{
	QApplication::clipboard()->setText(ui->networkIdButton->text());
}

void NetworkWidget::on_ipListWidget_itemActivated(QListWidgetItem *item)
{
	if (item)
		QApplication::clipboard()->setText(item->text());
}

void NetworkWidget::on_ipListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
	if (current)
		QApplication::clipboard()->setText(current->text());
}
