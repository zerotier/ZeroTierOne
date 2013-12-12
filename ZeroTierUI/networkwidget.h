#ifndef NETWORK_H
#define NETWORK_H

#include <string>

#include <QWidget>

namespace Ui {
class NetworkWidget;
}

class NetworkWidget : public QWidget
{
	Q_OBJECT

public:
	explicit NetworkWidget(QWidget *parent = 0,const std::string &nwid = std::string());
	virtual ~NetworkWidget();

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
	Ui::NetworkWidget *ui;
	std::string networkIdStr;
};

#endif // NETWORK_H
