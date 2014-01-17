#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "licensedialog.h"
#include "ui_licensedialog.h"

LicenseDialog::LicenseDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::LicenseDialog)
{
	ui->setupUi(this);
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
