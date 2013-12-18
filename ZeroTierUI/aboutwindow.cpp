#include "aboutwindow.h"
#include "ui_aboutwindow.h"

#include <QMessageBox>
#include "../node/Defaults.hpp"

AboutWindow::AboutWindow(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::AboutWindow)
{
	ui->setupUi(this);
}

AboutWindow::~AboutWindow()
{
	delete ui;
}
