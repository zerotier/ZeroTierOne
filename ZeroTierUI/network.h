#ifndef NETWORK_H
#define NETWORK_H

#include <QScrollArea>

namespace Ui {
class Network;
}

class Network : public QScrollArea
{
	Q_OBJECT

public:
	explicit Network(QWidget *parent = 0);
	~Network();

private:
	Ui::Network *ui;
};

#endif // NETWORK_H
