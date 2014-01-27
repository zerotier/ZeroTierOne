#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "licensedialog.h"
#include "ui_licensedialog.h"

#include "../node/Constants.hpp"

LicenseDialog::LicenseDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::LicenseDialog)
{
	ui->setupUi(this);

#ifdef __WINDOWS__
	QWidgetList widgets = this->findChildren<QWidget*>();
	foreach(QWidget *widget, widgets) {
		QFont font(widget->font());
		font.setPointSizeF(font.pointSizeF() * 0.75);
		widget->setFont(font);
	}
#endif
}

LicenseDialog::~LicenseDialog()
{
	delete ui;
}

void LicenseDialog::on_buttonBox_accepted()
{
	settings->setValue("acceptedLicenseV1",true);
	settings->sync();

	this->setResult(QDialog::Accepted);
}

void LicenseDialog::on_buttonBox_rejected()
{
	::exit(0);
}
