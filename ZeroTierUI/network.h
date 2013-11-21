#ifndef NETWORK_H
#define NETWORK_H

#include <string>

#include <QWidget>

namespace Ui {
class Network;
}

class Network : public QWidget
{
	Q_OBJECT

public:
	explicit Network(QWidget *parent = 0,const std::string &nwid = std::string());
	virtual ~Network();

	void setStatus(const std::string &status,const std::string &age);
	void setNetworkName(const std::string &name);
	void setNetworkType(const std::string &type);
	void setNetworkDeviceName(const std::string &dev);
	void setIps(const std::string &commaSeparatedList);

	const std::string &networkId();

private slots:
	void on_leaveNetworkButton_clicked();
	void on_networkIdPushButton_clicked();

private:
	Ui::Network *ui;
	std::string networkIdStr;
};

#endif // NETWORK_H
