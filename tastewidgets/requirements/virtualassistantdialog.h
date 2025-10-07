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

#include "excel.h"

class QHeaderView;

namespace Ui {
class VirtualAssistantDialog;
}

namespace requirement {

static const QString tmpExcelFile = "/var/tmp/tmpExcelModel.xlsx";
static const QString vaPath = "/home/taste/Development/VA/virtual-assistant/";
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
    QProcess *m_vaProcess;
};


class VirtualAssistantDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VirtualAssistantDialog(RequirementsModelBase *model, QString description = QString(), QString reqIfId = QString(), QDialog *parent = nullptr);
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
