#ifndef NETWORK_H
#define NETWORK_H

#include <QWidget>

namespace Ui {
class Network;
}

class Network : public QWidget
{
	Q_OBJECT

public:
	explicit Network(QWidget *parent = 0);
	~Network();

private slots:
	void on_leaveNetworkButton_clicked();

	void on_networkIdPushButton_clicked();

private:
	Ui::Network *ui;
};

#endif // NETWORK_H
