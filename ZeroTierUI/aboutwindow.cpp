#include "aboutwindow.h"
#include "ui_aboutwindow.h"

#include <QMessageBox>
#include "../node/Defaults.hpp"

AboutWindow::AboutWindow(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::AboutWindow)
{
	ui->setupUi(this);
#ifndef __APPLE__
	ui->uninstallButton->hide();
#endif
}

AboutWindow::~AboutWindow()
{
	delete ui;
}

void AboutWindow::on_uninstallButton_clicked()
{
	// Apple only... other OSes have more intrinsic mechanisms for uninstalling.
	QMessageBox::information(
				this,
				"Uninstalling ZeroTier One",
				QString("Uninstalling ZeroTier One is easy!\n\nJust remove ZeroTier One from your Applications folder and the service will automatically shut down within a few seconds. Then, on your next reboot, all other support files will be automatically deleted.\n\nIf you wish to uninstall the service and support files now, you can run the 'uninstall.sh' script found in ") + ZeroTier::ZT_DEFAULTS.defaultHomePath.c_str() + " using the 'sudo' command in a terminal.",
				QMessageBox::Ok,
				QMessageBox::NoButton);
}
