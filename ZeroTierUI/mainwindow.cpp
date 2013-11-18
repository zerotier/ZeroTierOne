#include "mainwindow.h"
#include "aboutwindow.h"
#include "ui_mainwindow.h"

#include <string>
#include <map>
#include <vector>

#include <QClipboard>
#include <QMutex>

static std::map< unsigned long,std::vector<std::string> > ztReplies;
static QMutex ztReplies_m;
static void handleZTMessage(void *arg,unsigned long id,const char *line)
{
	ztReplies_m.lock();
	if (*line) {
		ztReplies[id].push_back(std::string(line));
		ztReplies_m.unlock();
	} else {
		std::vector<std::string> resp(ztReplies[id]);
		ztReplies.erase(id);
		ztReplies_m.unlock();
	}
}

// Globally visible
ZeroTier::Node::LocalClient *zeroTierClient = (ZeroTier::Node::LocalClient *)0;

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::on_joinNetworkButton_clicked()
{
}

void MainWindow::on_actionAbout_triggered()
{
	AboutWindow *about = new AboutWindow(this);
	about->show();
}

void MainWindow::on_actionJoin_Network_triggered()
{
	// Does the same thing as clicking join button on main UI
	on_joinNetworkButton_clicked();
}

void MainWindow::on_actionShow_Detailed_Status_triggered()
{
}

void MainWindow::on_networkIdLineEdit_textChanged(const QString &text)
{
	QString newText;
	for(QString::const_iterator i(text.begin());i!=text.end();++i) {
		switch(i->toLatin1()) {
			case '0': newText.append('0'); break;
			case '1': newText.append('1'); break;
			case '2': newText.append('2'); break;
			case '3': newText.append('3'); break;
			case '4': newText.append('4'); break;
			case '5': newText.append('5'); break;
			case '6': newText.append('6'); break;
			case '7': newText.append('7'); break;
			case '8': newText.append('8'); break;
			case '9': newText.append('9'); break;
			case 'a': newText.append('a'); break;
			case 'b': newText.append('b'); break;
			case 'c': newText.append('c'); break;
			case 'd': newText.append('d'); break;
			case 'e': newText.append('e'); break;
			case 'f': newText.append('f'); break;
			case 'A': newText.append('a'); break;
			case 'B': newText.append('b'); break;
			case 'C': newText.append('c'); break;
			case 'D': newText.append('d'); break;
			case 'E': newText.append('e'); break;
			case 'F': newText.append('f'); break;
			default: break;
		}
	}
	ui->networkIdLineEdit->setText(newText);
}

void MainWindow::on_statusAndAddressButton_clicked()
{
	//	QApplication::clipboard()->setText(ui->myAddressCopyButton->text());
}
