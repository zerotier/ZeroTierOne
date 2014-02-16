/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#include "aboutwindow.h"
#include "ui_aboutwindow.h"

#include <QMessageBox>
#include <QFont>

#include "../node/Constants.hpp"
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
