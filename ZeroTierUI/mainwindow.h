#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "../node/Node.hpp"
#include "../node/Utils.hpp"

namespace Ui {
class MainWindow;
}

// Globally visible instance of local client for communicating with ZT1
// Can be null if not connected, or will point to current
extern ZeroTier::Node::LocalClient *volatile zeroTierClient;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

protected:
	virtual void timerEvent(QTimerEvent *event);

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
