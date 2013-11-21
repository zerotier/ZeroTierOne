#include "network.h"
#include "mainwindow.h"
#include "ui_network.h"

#include <QClipboard>
#include <QString>
#include <QStringList>
#include <QCoreApplication>
#include <QProcess>
#include <QList>
#include <QMessageBox>

Network::Network(QWidget *parent,const std::string &nwid) :
	QWidget(parent),
	ui(new Ui::Network),
	networkIdStr(nwid)
{
	ui->setupUi(this);
	ui->networkIdPushButton->setText(QString(nwid.c_str()));
	QFontMetrics fm(ui->ipListWidget->font());
	int lineHeight = ui->ipListWidget->spacing() + fm.height();
	ui->ipListWidget->setMinimumHeight(lineHeight * 3);
	ui->ipListWidget->setMaximumHeight(lineHeight * 3);
}

Network::~Network()
{
	delete ui;
}

void Network::setStatus(const std::string &status)
{
	ui->statusLabel->setText(QString(status.c_str()));
}

void Network::setNetworkName(const std::string &name)
{
	ui->nameLabel->setText(QString(name.c_str()));
}

void Network::setNetworkType(const std::string &type)
{
	ui->networkTypeLabel->setText(QString(type.c_str()));
	if (type == "?")
		ui->networkTypeLabel->setToolTip("Waiting for configuration...");
	else if (type == "public")
		ui->networkTypeLabel->setToolTip("This network can be joined by anyone.");
	else if (type == "private")
		ui->networkTypeLabel->setToolTip("This network is private, only authorized peers can join.");
	else ui->networkTypeLabel->setToolTip(QString());
}

void Network::setNetworkDeviceName(const std::string &dev)
{
	ui->deviceLabel->setText(QString(dev.c_str()));
}

void Network::setIps(const std::string &commaSeparatedList)
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

const std::string &Network::networkId()
{
	return networkIdStr;
}

void Network::on_leaveNetworkButton_clicked()
{
	if (QMessageBox::question(this,"Leave Network?",QString("Are you sure you want to leave network '") + networkIdStr.c_str() + "'?",QMessageBox::No,QMessageBox::Yes) == QMessageBox::Yes) {
		zeroTierClient->send((QString("leave ") + networkIdStr.c_str()).toStdString());
		this->setEnabled(false);
	}
}

void Network::on_networkIdPushButton_clicked()
{
	QApplication::clipboard()->setText(ui->networkIdPushButton->text());
}
