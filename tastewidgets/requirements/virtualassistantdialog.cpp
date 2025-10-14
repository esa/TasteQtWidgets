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

VirtualAssistantDialog::VirtualAssistantDialog(RequirementsModelBase *model, Requirement *requirement, QDialog *parent)
    : QDialog(parent)
    , ui(new Ui::VirtualAssistantDialog)
    , m_model(model)
    , m_requirement(requirement)
{
    ui->setupUi(this);

    QFile::remove(tmpExcelFile);
    QFileInfo fileinfo(tmpExcelFile);

    Excel::createDefault(tmpExcelFile, RequirementsModelBase::SRS);

    int rowCount = m_model->rowCount(QModelIndex());

    if (rowCount) {
        Excel excel(tmpExcelFile, m_model, RequirementsModelBase::SRS);
        excel.exportAllExcel(m_requirement);
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

    if(m_requirement == nullptr) {
        connect(ui->oneButton, &QPushButton::clicked, this, &VirtualAssistantDialog::onDuplicationButtonPressed);
        connect(ui->twoButton, &QPushButton::clicked, this, &VirtualAssistantDialog::onConflictButtonPressed);

        ui->oneButton->setText("Duplication ?");
        ui->twoButton->setText("Conflict ?");
        ui->threeButton->hide();
    } else {
        QString title = QString("Virtual Assistant - %1").arg(m_requirement->m_id);
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

    ui->textEdit->setText("Thinking...");

    const QUrl url(Question);

    QNetworkRequest request(url);

    QNetworkReply *reply = mgr->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [=](){
        if(reply->error() == QNetworkReply::NoError){
            QString contents = QString::fromUtf8(reply->readAll());
            qDebug() << "Message received OK";
            displayChatResponse(contents);
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

    ui->textEdit->setText("Thinking...");

    const QUrl url(Question);

    QNetworkRequest request(url);

    QNetworkReply *reply = mgr->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [=](){
        if(reply->error() == QNetworkReply::NoError){
            QString contents = QString::fromUtf8(reply->readAll());
            ui->textEdit->setText("");
            qDebug() << "Message received OK\n" << contents;
            displayQueryResponse(contents);
        }
        else{
            QString err = reply->errorString();
            qDebug() << err;
            ui->textEdit->setText(err);
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

void VirtualAssistantDialog::displayChatResponse(const QString response) const
{
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    QJsonObject json = doc.object();

    QString res = json.value("reply").toString();

    if (!res.isEmpty())
    {
        ui->textEdit->setText(res);
    }
}

void VirtualAssistantDialog::displayQueryResponse(const QString response) const
{
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    QJsonObject json = doc.object();

    QJsonValue replyValue = json.value("reply");

    ui->textEdit->setText(response);

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

void VaWorker::initConfig()
{
    QString homedir = QDir::homePath();
    QString configPath = QString("%1%2").arg(homedir).arg(CONFIG_PATH);
    QString configFile = QString("%1/%2").arg(configPath).arg(CONFIG_FILE);

    m_settingsFilePath = configFile;

    if(std::filesystem::exists(configFile.toStdString()))
    {
        qDebug() << "Settings File OK";
        m_settingsFilePath = configFile;
    }
    else
    {
        qDebug()<< "Settings File Not found " << configFile;
        QFile::copy(CONFIG_FILE, configFile);

        if(std::filesystem::exists(configFile.toStdString()))
        {
            qDebug()  << "Settings File Now OK";
            m_settingsFilePath = configFile;
        }
    }

}

void VaWorker::readConfigData()
{
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);

    // General settings.
    settings.beginGroup("VA_CONFIGURATION");

    m_vaRootPath = settings.value(VA_ROOT_PATH, DEFAULT_VA_ROOT_PATH).toString();
    m_configFilePath = settings.value(CONFIG_FILE_PATH, DEFAULT_CONFIG_FILE_PATH).toString();
    m_queryDefinitionsFilePath = settings.value(QUERY_DEFINITIONS_FILE_PATH, DEFAULT_QUERY_DEFINITIONS_FILE_PATH).toString();
    m_queryDefinitionsBaseDirectory = settings.value(QUERY_DEFINITIONS_BASE_DIRECTORY, DEFAULT_QUERY_DEFINITIONS_BASE_DIRECTORY).toString();
    m_venvPath = settings.value(VENV_PATH, DEFAULT_VENV_PATH).toString();
    m_verbosity = settings.value(VERBOSITY, DEFAULT_VERBOSITY).toString();

    settings.endGroup();

}

void VaWorker::writeConfigData()
{
    QSettings settings(m_settingsFilePath, QSettings::IniFormat);

    settings.beginGroup("VA_CONFIGURATION");

    settings.setValue(VA_ROOT_PATH, m_vaRootPath);
    settings.setValue(CONFIG_FILE_PATH, m_configFilePath);
    settings.setValue(QUERY_DEFINITIONS_FILE_PATH, m_queryDefinitionsFilePath);
    settings.setValue(QUERY_DEFINITIONS_BASE_DIRECTORY, m_queryDefinitionsBaseDirectory);
    settings.setValue(VENV_PATH, m_venvPath);
    settings.setValue(VERBOSITY, m_verbosity);

    settings.endGroup();

    settings.sync();
}

void VaWorker::runVa()
{
    m_vaProcess = new QProcess();

    QStringList arguments;

    initConfig();
    readConfigData();
    writeConfigData();

    QString vaRoot = QString("/%1/").arg(m_vaRootPath);

    arguments << vaRoot + m_venvPath + "/bin/vareq";
    arguments << "--mode" << "serve";
    arguments << "--verbosity" << m_verbosity;
    arguments << "--config-path" << vaRoot + m_configFilePath;
    arguments << "--query-definitions-path" << vaRoot + m_queryDefinitionsFilePath;
    arguments << "--query-definitions-base-directory" << vaRoot + m_queryDefinitionsBaseDirectory;
    arguments << "--requirements" << tmpExcelFile;

    qDebug() << "First finish";
    m_vaProcess->setProgram(vaRoot + m_venvPath + "/bin/python3");
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
