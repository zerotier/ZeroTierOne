#ifndef AboutWindow_H
#define AboutWindow_H

#include <QDialog>

namespace Ui {
class AboutWindow;
}

class AboutWindow : public QDialog
{
	Q_OBJECT

public:
	explicit AboutWindow(QWidget *parent = 0);
	~AboutWindow();

private slots:
	void on_uninstallButton_clicked();

private:
	Ui::AboutWindow *ui;
};

#endif // AboutWindow_H
