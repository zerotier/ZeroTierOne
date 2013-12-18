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
	virtual ~AboutWindow();

private:
	Ui::AboutWindow *ui;
};

#endif // AboutWindow_H
