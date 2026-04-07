/*
   Copyright (C) 2025 European Space Agency - <maxime.perrotin@esa.int>

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

#include <QDialog>
#include <QFileDialog>
#include <QPointer>
#include <QMessageBox>
#include "issuesmanager.h"
#include "requirementsmanager.h"
#include "requirementsmodelbase.h"

namespace Ui {
class SelectSourceDialog;
}

namespace requirement {

class RequirementsModelBase;

class RequirementManager;

class IssuesManager;
/*!
 * \brief The dialog for adding a new Requirement
 */
class SelectSourceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SelectSourceDialog(QString title, QString label, QString url, QString token, enum RequirementsModelBase::modelType selection, QWidget *parent = nullptr);
    ~SelectSourceDialog();
//    void enableGitLab(bool enable);
    enum RequirementsModelBase::modelType type();

    void accept() override;

    QString getUrl();
    QString getToken();

    void setUrl(const QUrl &url);
    void setToken(const QString &token);

    enum Selection
    {
        GitLabSelected,
        ReqIfSelected,
        ExcelSelected
    };

    enum Selection result();

Q_SIGNALS:
    void requirementsUrlChanged(const QUrl &url);

protected Q_SLOTS:
    void updateServerStatus();

private:
    void setOptions(enum RequirementsModelBase::modelType type);

    volatile bool m_wait;
    QString m_url;
    QString m_token;
    enum RequirementsModelBase::modelType m_type;
    void onRadioButtonChanged();
    void onChangeOfCredentials();
    Ui::SelectSourceDialog *ui;
    QPointer<RequirementsManager> m_manager;
};

}
