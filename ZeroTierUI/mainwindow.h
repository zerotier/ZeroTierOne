#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QEvent>
#include <QString>

#include "../node/Node.hpp"
#include "../node/Utils.hpp"

namespace Ui {
class MainWindow;
}

// Globally visible instance of local client for communicating with ZT1
// Can be null if not connected, or will point to current
extern ZeroTier::Node::LocalClient *zeroTierClient;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	// Event used to pass messages from the Node::LocalClient thread to the
	// main window to update network lists and stats.
	class ZTMessageEvent : public QEvent
	{
	public:
		ZTMessageEvent(const std::vector<std::string> &m) :
			QEvent(QEvent::User),
			ztMessage(m)
		{
		}

		std::vector<std::string> ztMessage;
	};

	explicit MainWindow(QWidget *parent = 0);
	virtual ~MainWindow();

protected:
	virtual void timerEvent(QTimerEvent *event);
	virtual void customEvent(QEvent *event);

private slots:
	void on_joinNetworkButton_clicked();
	void on_actionAbout_triggered();
	void on_networkIdLineEdit_textChanged(const QString &text);
	void on_statusAndAddressButton_clicked();

private:
	Ui::MainWindow *ui;

	QString myAddress;
	QString myStatus;
	QString myVersion;
	unsigned int numPeers;
	unsigned int cyclesSinceResponseFromService;
};

#endif // MAINWINDOW_H
