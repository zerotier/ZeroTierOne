#ifndef ONETIMEDIALOG_H
#define ONETIMEDIALOG_H

#include <QDialog>

namespace Ui {
class OneTimeDialog;
}

class OneTimeDialog : public QDialog
{
	Q_OBJECT

public:
	explicit OneTimeDialog(QWidget *parent = 0,const char *propName = (const char *)0,const QString &title = QString(),const QString &message = QString());
	~OneTimeDialog();

private slots:
	void on_pushButton_clicked();

private:
	Ui::OneTimeDialog *ui;
	const char *_propName;
};

#endif // ONETIMEDIALOG_H
