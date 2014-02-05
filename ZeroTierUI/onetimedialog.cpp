#include "onetimedialog.h"
#include "ui_onetimedialog.h"
#include "main.h"

OneTimeDialog::OneTimeDialog(QWidget *parent,const char *propName,const QString &title,const QString &message) :
	QDialog(parent),
	ui(new Ui::OneTimeDialog)
{
	ui->setupUi(this);

	ui->label->setText(message);
	this->setWindowTitle(title);
	_propName = propName;

#ifdef __WINDOWS__
	QWidgetList widgets = this->findChildren<QWidget*>();
	foreach(QWidget *widget, widgets) {
		QFont font(widget->font());
		font.setPointSizeF(font.pointSizeF() * 0.75);
		widget->setFont(font);
	}
#endif
}

OneTimeDialog::~OneTimeDialog()
{
	delete ui;
}

void OneTimeDialog::on_pushButton_clicked()
{
	if (_propName) {
		settings->setValue(_propName,ui->checkBox->isChecked());
		settings->sync();
	}
	this->close();
}
