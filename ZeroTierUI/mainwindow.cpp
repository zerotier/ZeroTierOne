#include "mainwindow.h"
#include "aboutwindow.h"
#include "ui_mainwindow.h"

#include <QClipboard>

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::on_joinNetworkButton_clicked()
{
}

void MainWindow::on_actionAbout_triggered()
{
	AboutWindow *about = new AboutWindow(this);
	about->show();
}

void MainWindow::on_actionJoin_Network_triggered()
{
	on_joinNetworkButton_clicked();
}

void MainWindow::on_actionShow_Detailed_Status_triggered()
{
}

void MainWindow::on_networkIdLineEdit_textChanged(const QString &text)
{
}

void MainWindow::on_statusAndAddressButton_clicked()
{
	//	QApplication::clipboard()->setText(ui->myAddressCopyButton->text());
}
