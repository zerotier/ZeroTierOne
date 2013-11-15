#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

private slots:
	void on_joinNetworkButton_clicked();
	void on_actionAbout_triggered();
	void on_actionJoin_Network_triggered();
	void on_actionShow_Detailed_Status_triggered();
	void on_networkIdLineEdit_textChanged(const QString &arg1);
	void on_statusAndAddressButton_clicked();

private:
	Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
