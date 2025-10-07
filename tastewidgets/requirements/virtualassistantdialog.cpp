/*
   Copyright (C) 2023 European Space Agency - <maxime.perrotin@esa.int>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public License
along with this program. If not, see <https://www.gnu.org/licenses/lgpl-2.1.html>.
*/

#include "virtualassistantdialog.h"
#include "ui_virtualassistantdialog.h"

#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QCursor>
#include <QDesktopServices>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableView>
#include <QToolButton>
#include <QFile>

#include <iostream>

#include <cstddef>

namespace requirement {
const int kIconSize = 16;

VirtualAssistantDialog::VirtualAssistantDialog(RequirementsModelBase *model, QString description, QString reqIfId, QDialog *parent)
    : QDialog(parent)
    , ui(new Ui::VirtualAssistantDialog)
    , m_model(model)
    , m_reqIfId(reqIfId)
    , m_description(description)
{
    ui->setupUi(this);

    QFile::remove(tmpExcelFile);
    QFileInfo fileinfo(tmpExcelFile);

    auto type = m_model->getState();
//    enum RequirementsModelBase::modelType type = RequirementsModelBase::Both;

    Excel::createDefault(tmpExcelFile, type);

    int rowCount = m_model->rowCount(QModelIndex());

    if (rowCount) {
        Excel excel(tmpExcelFile, m_model, type);
        excel.exportExcel();
    }

    m_ollamaThread = new QThread();
    m_ollama = new OllamaWorker();
    m_ollama->moveToThread(m_ollamaThread);
    connect(m_ollamaThread, &QThread::started, m_ollama, &OllamaWorker::runOllama);
    connect(m_ollamaThread, &QThread::finished, m_ollama, &QObject::deleteLater);
    connect(m_ollamaThread, &QThread::finished, m_ollamaThread, &QObject::deleteLater);
    m_ollamaThread->start();

    m_vaThread = new QThread();
    m_va = new VaWorker();
    m_va->moveToThread(m_vaThread);
    connect(m_vaThread, &QThread::started, m_va, &VaWorker::runVa);
    connect(m_vaThread, &QThread::finished, m_va, &QObject::deleteLater);
    connect(m_vaThread, &QThread::finished, m_vaThread, &QObject::deleteLater);
    m_vaThread->start();

    if(reqIfId.isEmpty()) {
        connect(ui->oneButton, &QPushButton::clicked, this, &VirtualAssistantDialog::onDuplicationButtonPressed);
        connect(ui->twoButton, &QPushButton::clicked, this, &VirtualAssistantDialog::onConflictButtonPressed);

        ui->oneButton->setText("Duplication ?");
        ui->twoButton->setText("Conflict ?");
        ui->threeButton->hide();
    } else {
        QString title = QString("Virtual Assistant - %1").arg(reqIfId);
        this->setWindowTitle(title);

        connect(ui->oneButton, &QPushButton::clicked, this, &VirtualAssistantDialog::onAssignTypeButtonPressed);
        connect(ui->twoButton, &QPushButton::clicked, this, &VirtualAssistantDialog::onRewordButtonPressed);
        connect(ui->threeButton, &QPushButton::clicked, this, &VirtualAssistantDialog::onReviewButtonPressed);

        ui->oneButton->setText("Assign-Type");
        ui->twoButton->setText("Reword");
        ui->threeButton->setText("Review");
    }

    connect(ui->chatButton, &QPushButton::clicked, this, &VirtualAssistantDialog::onChatButtonPressed);
}

VirtualAssistantDialog::~VirtualAssistantDialog()
{
    m_va->stopVa();
    m_ollama->stopOllama();
    QFile::remove(tmpExcelFile);
    delete ui;
}


void VirtualAssistantDialog::onChatButtonPressed() const
{
    qDebug() << "Chat button pressed";

    QNetworkAccessManager *mgr = new QNetworkAccessManager();

    QString Question = QString("http://localhost:8080/chat/%1").arg(ui->questionLineEdit->text());

    const QUrl url(Question);

    QNetworkRequest request(url);

    QNetworkReply *reply = mgr->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [=](){
        if(reply->error() == QNetworkReply::NoError){
            QString contents = QString::fromUtf8(reply->readAll());
            qDebug() << "Message received OK";
            displayResponse(contents);
        }
        else{
            QString err = reply->errorString();
            qDebug() << err;
        }
        reply->deleteLater();
    });
}


void VirtualAssistantDialog::queryEndPoint(QString query, QString reqIfId) const
{
    QNetworkAccessManager *mgr = new QNetworkAccessManager();

    QString Question;

    if(m_reqIfId.isEmpty()) {
        Question = QString("http://localhost:8080/query/%1").arg(query);
    } else {
        Question = QString("http://localhost:8080/query/%1/%2").arg(query).arg(m_reqIfId);
    }

    const QUrl url(Question);

    QNetworkRequest request(url);

    QNetworkReply *reply = mgr->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [=](){
        if(reply->error() == QNetworkReply::NoError){
            QString contents = QString::fromUtf8(reply->readAll());
            qDebug() << "Message received OK";
            displayResponse(contents);
        }
        else{
            QString err = reply->errorString();
            qDebug() << err;
        }
        reply->deleteLater();
    });
}

void VirtualAssistantDialog::onDuplicationButtonPressed() const
{
    qDebug() << "Duplication button pressed";
    queryEndPoint("detect-duplicate");
}

void VirtualAssistantDialog::onConflictButtonPressed() const
{
    qDebug() << "Conflict button pressed";
    queryEndPoint("detect-conflicts");
}

void VirtualAssistantDialog::onReviewButtonPressed() const
{
    qDebug() << "Review button pressed";
    queryEndPoint("review", m_reqIfId);
}

void VirtualAssistantDialog::onRewordButtonPressed() const
{
    qDebug() << "Reword button pressed";
    queryEndPoint("reword", m_reqIfId);
}

void VirtualAssistantDialog::onAssignTypeButtonPressed() const
{
    qDebug() << "Assign type button pressed";
    queryEndPoint("assign-type", m_reqIfId);
}

void VirtualAssistantDialog::displayResponse(const QString response) const
{
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    QJsonObject json = doc.object();

    QString res = json.value("reply").toString();

    if (!res.isEmpty())
    {
        ui->textEdit->setText(res);
    }
}

/*!
   \brief OllamaWorker::OllamaWorker
   \launch ollama server
 */
OllamaWorker::OllamaWorker()
    : QObject()
{}

void OllamaWorker::stopOllama()
{
    m_ollamaProcess->terminate();
    m_ollamaProcess->deleteLater();
    m_ollamaProcess = nullptr;
}

void OllamaWorker::runOllama()
{
    m_ollamaProcess = new QProcess();
    m_ollamaProcess->setProgram("ollama");
    m_ollamaProcess->setArguments(QStringList({"serve"}));
    m_ollamaProcess->start();
    m_ollamaProcess->waitForFinished(-1);
}

/*!
   \brief OllamaWorker::OllamaWorker
   \launch ollama server
 */
VaWorker::VaWorker()
    : QObject()
{}

void VaWorker::stopVa()
{
    m_vaProcess->terminate();
    m_vaProcess->deleteLater();
    m_vaProcess = nullptr;
}

void VaWorker::runVa()
{
    m_vaProcess = new QProcess();

    QStringList arguments;

    arguments << vaPath + "venv/bin/vareq";
    arguments << "--mode" << "serve";
    arguments << "--verbosity" << "debug";
    arguments << "--query-definitions-path" << vaPath + "data/mbep_queries.json";
    arguments << "--query-definitions-base-directory" << vaPath + "data";
    arguments << "--requirements" << tmpExcelFile;
    arguments << "--config-path" << vaPath + "data/default_config_corrected.json";

    qDebug() << "First finish";
    m_vaProcess->setProgram(vaPath + "venv/bin/python3");
    m_vaProcess->setArguments(arguments);

    connect(m_vaProcess, &QProcess::readyReadStandardOutput, [&]() {
            QByteArray output = m_vaProcess->readAllStandardOutput();
            qDebug() << "Python output:" << output;
        });

    qDebug() << "Start VA";
    m_vaProcess->start();
    m_vaProcess->waitForFinished(-1);

}

}
