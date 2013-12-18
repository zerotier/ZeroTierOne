#include "networkwidget.h"
#include "mainwindow.h"
#include "ui_networkwidget.h"

#include <QClipboard>
#include <QString>
#include <QStringList>
#include <QCoreApplication>
#include <QProcess>
#include <QList>
#include <QMessageBox>

NetworkWidget::NetworkWidget(QWidget *parent,const std::string &nwid) :
	QWidget(parent),
	ui(new Ui::NetworkWidget),
	networkIdStr(nwid)
{
	ui->setupUi(this);
	ui->networkIdPushButton->setText(QString(nwid.c_str()));
	QFontMetrics fm(ui->ipListWidget->font());
	int lineHeight = ui->ipListWidget->spacing() + fm.height();
	ui->ipListWidget->setMinimumHeight(lineHeight * 3);
	ui->ipListWidget->setMaximumHeight(lineHeight * 3);
}

NetworkWidget::~NetworkWidget()
{
	delete ui;
}

void NetworkWidget::setStatus(const std::string &status,const std::string &age)
{
	ui->statusLabel->setText(QString(status.c_str()));
	if (status == "OK")
		ui->ageLabel->setText(QString("(configuration is ") + age.c_str() + " seconds old)");
	else ui->ageLabel->setText(QString());
}

void NetworkWidget::setNetworkName(const std::string &name)
{
	ui->nameLabel->setText(QString(name.c_str()));
}

void NetworkWidget::setNetworkType(const std::string &type)
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

const std::string &NetworkWidget::networkId()
{
	return networkIdStr;
}

void NetworkWidget::on_leaveNetworkButton_clicked()
{
	if (QMessageBox::question(this,"Leave Network?",QString("Are you sure you want to leave network '") + networkIdStr.c_str() + "'?",QMessageBox::No,QMessageBox::Yes) == QMessageBox::Yes) {
		zeroTierClient->send((QString("leave ") + networkIdStr.c_str()).toStdString());
		this->setEnabled(false);
	}
}

void NetworkWidget::on_networkIdPushButton_clicked()
{
	QApplication::clipboard()->setText(ui->networkIdPushButton->text());
}

void NetworkWidget::on_ipListWidget_doubleClicked(const QModelIndex &index)
{
	QListWidgetItem *ci = ui->ipListWidget->currentItem();
	if (ci)
		QApplication::clipboard()->setText(ci->text());
}
