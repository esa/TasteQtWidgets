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

#pragma once

#include <QItemSelection>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QToolButton>
#include <QDialog>
#include <QThread>
#include <QProcessEnvironment>
#include <QDir>
#include <QFile>
#include <QSettings>

#include "excel.h"

class QHeaderView;

namespace Ui {
class VirtualAssistantDialog;
}

namespace requirement {

static const QString tmpExcelFile = "/var/tmp/tmpExcelModel.xlsx";
static const QString vaPath = "/home/taste/Development/VA/virtual-assistant/";
static const QString CONFIG_PATH = "/.local";
static const QString CONFIG_FILE = "vaconfig.ini";

static const QString VA_ROOT_PATH = "VaRootPath";
static const QString CONFIG_FILE_PATH = "ConfigFilePath";
static const QString QUERY_DEFINITIONS_FILE_PATH = "QueryDefinitionsFilePath";
static const QString QUERY_DEFINITIONS_BASE_DIRECTORY = "QueryDefinitionsBaseDirectory";
static const QString VENV_PATH = "VenvPath";
static const QString VERBOSITY = "Verbosity";

static const QString DEFAULT_VA_ROOT_PATH = "/home/taste/tool-inst/share/virtual-assistant";
static const QString DEFAULT_CONFIG_FILE_PATH = "data/default_config_corrected.json";
static const QString DEFAULT_QUERY_DEFINITIONS_FILE_PATH = "data/mbep_queries.json";
static const QString DEFAULT_QUERY_DEFINITIONS_BASE_DIRECTORY = "data";
static const QString DEFAULT_VENV_PATH = "venv";
static const QString DEFAULT_VERBOSITY = "debug";

/*!
   Worker class for launching Ollama Server
 */
class OllamaWorker : public QObject
{
    Q_OBJECT
public:
    OllamaWorker();
    void stopOllama();

public Q_SLOTS:
    void runOllama();

private:
    QProcess *m_ollamaProcess;
};

/*!
   Worker class for launching virtual assistant server
 */
class VaWorker : public QObject
{
    Q_OBJECT
public:
    VaWorker();
    void stopVa();

public Q_SLOTS:
    void runVa();

private:
    void initConfig();
    void readConfigData();
    void writeConfigData();

    QString m_vaRootPath;
    QString m_configFilePath;
    QString m_queryDefinitionsFilePath;
    QString m_queryDefinitionsBaseDirectory;
    QString m_venvPath;
    QString m_verbosity;
    QString m_tmpExcelFilePath;
    QString m_settingsFilePath;
    QProcess *m_vaProcess;
};


class VirtualAssistantDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VirtualAssistantDialog(RequirementsModelBase *model, Requirement *requirement = nullptr, QDialog *parent = nullptr);
    ~VirtualAssistantDialog();

private:
    void queryEndPoint(QString query, QString reqIfId = QString()) const;
    void onChatButtonPressed() const;
    void onDuplicationButtonPressed() const;
    void onConflictButtonPressed() const;
    void onReviewButtonPressed() const;
    void onRewordButtonPressed() const;
    void onAssignTypeButtonPressed() const;
    void displayResponse(const QString response) const;
    Ui::VirtualAssistantDialog *ui;

    Requirement *m_requirement;

    QString m_vaPath;

    QString m_reqIfId;
    QString m_description;
    RequirementsModelBase *m_model;

    QThread *m_ollamaThread;
    OllamaWorker *m_ollama = nullptr;
    friend class OllamaWorker;

    QThread *m_vaThread;
    VaWorker *m_va = nullptr;
    friend class VaWorker;
};

}
