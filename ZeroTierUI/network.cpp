#include "network.h"
#include "ui_network.h"

Network::Network(QWidget *parent) :
	QScrollArea(parent),
	ui(new Ui::Network)
{
	ui->setupUi(this);
}

Network::~Network()
{
	delete ui;
}
