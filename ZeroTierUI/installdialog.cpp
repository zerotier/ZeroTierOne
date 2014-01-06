#include "installdialog.h"
#include "mainwindow.h"
#include "ui_installdialog.h"

#include "../node/Defaults.hpp"
#include "../node/SoftwareUpdater.hpp"

#include <QMainWindow>
#include <QMessageBox>
#include <QByteArray>
#include <QSslSocket>

InstallDialog::InstallDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::InstallDialog),
	nam(new QNetworkAccessManager(this)),
	phase(FETCHING_NFO)
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
	} else {
		if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) == 200) {
			QByteArray installerData(reply->readAll());

			switch(phase) {
				case FETCHING_NFO: {
					unsigned int vMajor = 0,vMinor = 0,vRevision = 0;
					installerData.append((char)0);
					const char *err = ZeroTier::SoftwareUpdater::parseNfo(installerData.data(),vMajor,vMinor,vRevision,signedBy,signature,url);

					if (err) {
						QMessageBox::critical(this,"Download Failed","Download failed: there is a problem with the software update web site.\nTry agian later. (invalid .nfo file)",QMessageBox::Ok,QMessageBox::NoButton);
						QApplication::exit(1);
						return;
					}

					phase = FETCHING_INSTALLER;
					reply = nam->get(QNetworkRequest(QUrl(url.c_str())));
					QObject::connect(reply,SIGNAL(downloadProgress(qint64,qint64)),this,SLOT(on_downloadProgress(qint64,qint64)));
				}	break;
				case FETCHING_INSTALLER: {
					if (!ZeroTier::SoftwareUpdater::validateUpdate(installerData.data(),installerData.length(),signedBy,signature)) {
						QMessageBox::critical(this,"Download Failed","Download failed: there is a problem with the software update web site.\nTry agian later. (failed signature check)",QMessageBox::Ok,QMessageBox::NoButton);
						QApplication::exit(1);
						return;
					}
				}	break;
			}

			ui->progressBar->setMinimum(0);
			ui->progressBar->setMaximum(100);
			ui->progressBar->setValue(0);
		} else {
			QMessageBox::critical(this,"Download Failed",QString("Download failed: HTTP status code ") + reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString(),QMessageBox::Ok,QMessageBox::NoButton);
			QApplication::exit(1);
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
