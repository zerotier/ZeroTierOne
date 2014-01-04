#include "installdialog.h"
#include "mainwindow.h"
#include "ui_installdialog.h"

#include "../node/Defaults.hpp"

#include <QMainWindow>
#include <QMessageBox>
#include <QByteArray>
#include <QSslSocket>

InstallDialog::InstallDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::InstallDialog),
	nam(new QNetworkAccessManager(this))
{
	ui->setupUi(this);
	QObject::connect(nam,SIGNAL(finished(QNetworkReply*)),this,SLOT(on_networkReply(QNetworkReply*)));

	const char *nfoUrl = ZeroTier::ZT_DEFAULTS.updateLatestNfoURL.c_str();
	if (!*nfoUrl) {
		QMessageBox::critical(this,"Download Failed","Download failed: internal error: no update URL configured in build!",QMessageBox::Ok,QMessageBox::NoButton);
		QApplication::exit(1);
		return;
	}

	QNetworkReply *reply = nam->get(QNetworkRequest(QUrl(nfoUrl)));
	QObject::connect(reply,SIGNAL(downloadProgress(qint64,qint64)),this,SLOT(on_downloadProgress(qint64,qint64)));
}

InstallDialog::~InstallDialog()
{
	delete ui;
}

void InstallDialog::on_networkReply(QNetworkReply *reply)
{
	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError) {
		QMessageBox::critical(this,"Download Failed",QString("Download failed: ") + reply->errorString(),QMessageBox::Ok,QMessageBox::NoButton);
		QApplication::exit(1);
		return;
	} else {
		if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) == 200) {
			QByteArray installerData(reply->readAll());
			installerData.append((char)0);
			printf("%s\n",installerData.data());
		} else {
			QMessageBox::critical(this,"Download Failed",QString("Download failed: HTTP status code ") + reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString(),QMessageBox::Ok,QMessageBox::NoButton);
			QApplication::exit(1);
			return;
		}
	}
}

void InstallDialog::on_InstallDialog_rejected()
{
	QApplication::exit();
}

//((QMainWindow *)this->parent())->setHidden(false);

void InstallDialog::on_cancelButton_clicked()
{
	QApplication::exit();
}

void InstallDialog::on_downloadProgress(qint64 bytesReceived,qint64 bytesTotal)
{
	if (bytesTotal <= 0) {
		ui->progressBar->setValue(0);
		ui->progressBar->setMinimum(0);
		ui->progressBar->setMaximum(0);
	} else {
		double pct = ((double)bytesReceived / (double)bytesTotal) * 100.0;
		if (pct > 100.0)
			pct = 100.0;
		ui->progressBar->setMinimum(0);
		ui->progressBar->setMaximum(100);
		ui->progressBar->setValue((int)pct);
	}
}
