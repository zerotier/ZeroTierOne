#include "aboutwindow.h"
#include "ui_aboutwindow.h"

#include <QMessageBox>

#include "../node/Node.hpp"

AboutWindow::AboutWindow(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::AboutWindow)
{
	ui->setupUi(this);

	ui->aboutTextLabel->setText(QString("ZeroTier One\nVersion ")+ZeroTier::Node::versionString()+"\nQt Graphical User Interface\n\n(c)2011-2014 ZeroTier Networks LLC\n\nReleased under the terms of the GNU\nGeneral Public License v3.0, see: http://gplv3.fsf.org for terms.\n\nAuthor(s): Adam Ierymenko");
}

AboutWindow::~AboutWindow()
{
	delete ui;
}
