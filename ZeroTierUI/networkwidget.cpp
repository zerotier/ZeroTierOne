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
	ui->ipListWidget->setMinimumHeight(lineHeight * 4);
	ui->ipListWidget->setMaximumHeight(lineHeight * 4);

	QWidgetList widgets = this->findChildren<QWidget*>();
	foreach(QWidget* widget, widgets)
		widget->setAttribute(Qt::WA_MacShowFocusRect,false);
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
	else if (type == "public")
		ui->networkTypeLabel->setStatusTip("This network can be joined by anyone in the world.");
	else if (type == "private")
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

void NetworkWidget::on_ipListWidget_itemActivated(QListWidgetItem *item)
{
		if (item)
			QApplication::clipboard()->setText(item->text());
}
