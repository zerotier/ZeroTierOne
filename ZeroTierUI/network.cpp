#include "network.h"
#include "ui_network.h"

#include <QClipboard>

Network::Network(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::Network)
{
	ui->setupUi(this);
}

Network::~Network()
{
	delete ui;
}

void Network::on_leaveNetworkButton_clicked()
{
}

void Network::on_networkIdPushButton_clicked()
{
	QApplication::clipboard()->setText(ui->networkIdPushButton->text());
}
