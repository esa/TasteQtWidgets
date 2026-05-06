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
#include "selectsourcedialog.h"

#include "ui_selectsourcedialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>

namespace requirement {

SelectSourceDialog::SelectSourceDialog(QString title, QString label, QString url, QString token, enum RequirementsModelBase::modelType type, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SelectSourceDialog)
    , m_wait(false)
    , m_url(url)
    , m_token(token)
    , m_type(type)
{
    ui->setupUi(this);
    this->setWindowTitle(title);
    ui->titleLabel->setText(label);
    setOptions(m_type);

    m_manager = new requirement::RequirementsManager(tracecommon::IssuesManager::REPO_TYPE::GITLAB, parent);
    m_manager->setRequirementsCredentials(url, token);

    connect(m_manager, &tracecommon::IssuesManager::projectUrlChanged, ui->credentialWidget,
            &tracecommon::CredentialWidget::setUrl);
    connect(m_manager, &tracecommon::IssuesManager::tokenChanged, ui->credentialWidget,
            &tracecommon::CredentialWidget::setToken);

    connect(ui->credentialWidget, &tracecommon::CredentialWidget::urlChanged, this,
            &SelectSourceDialog::onChangeOfCredentials);
    connect(ui->credentialWidget, &tracecommon::CredentialWidget::tokenChanged, this,
            &SelectSourceDialog::onChangeOfCredentials);
    connect(m_manager, &RequirementsManager::projectIDChanged, this, &SelectSourceDialog::updateServerStatus);
    connect(m_manager, &RequirementsManager::busyChanged, this, &SelectSourceDialog::updateServerStatus);
    connect(m_manager, &RequirementsManager::connectionError, this, [this](const QString &error) {
        updateServerStatus();
        QMessageBox::warning(this, tr("Connection error"), tr("Connection failed for this error:\n%1").arg(error));
    });

    connect(ui->gitLabRadioButton, &QRadioButton::clicked, this, &SelectSourceDialog::onRadioButtonChanged);
    connect(ui->reqIfRadioButton, &QRadioButton::clicked, this, &SelectSourceDialog::onRadioButtonChanged);
    connect(ui->excelRadioButton, &QRadioButton::clicked, this, &SelectSourceDialog::onRadioButtonChanged);

    if (!m_manager->projectUrl().isEmpty()) {
        ui->credentialWidget->setUrl(m_manager->projectUrl());
    }
    if (!m_manager->token().isEmpty()) {
        ui->credentialWidget->setToken(m_manager->token());
    }

    updateServerStatus();
}

SelectSourceDialog::~SelectSourceDialog()
{
    delete ui;
    delete m_manager;
}

void SelectSourceDialog::onRadioButtonChanged()
{
    if (ui->gitLabRadioButton->isChecked()) {
        ui->credentialWidget->setEnabled(true);
        if(m_type == RequirementsModelBase::Both) {
            ui->comboBox->removeItem(2);
            ui->comboBox->insertItem(2, "Both");
        }
    } else {
        ui->credentialWidget->setEnabled(false);
        if (ui->comboBox->currentIndex() == 2) {
            ui->comboBox->setCurrentIndex(0);
        }
        ui->comboBox->removeItem(2);
    }
}

void SelectSourceDialog::onChangeOfCredentials()
{
    const QUrl url(ui->credentialWidget->url());
    QString token(ui->credentialWidget->token());

    qDebug() << "SelectSourceDialog::onChangeOfCredentials - URL:" << url.toString() << "Token length:" << token.length();
    qDebug() << "SelectSourceDialog::onChangeOfCredentials - Current Manager Project ID:" << m_manager->projectID();

    if ((url.toString().compare(m_manager->projectUrl()) != 0)
    || (token.compare(m_manager->token()) != 0))
    {
        qDebug() << "SelectSourceDialog::onChangeOfCredentials - Credentials changed, updating manager and notifying parent";
        m_wait = true;
        m_manager->setRequirementsCredentials(url.toString(), token);
        Q_EMIT requirementsUrlChanged(url);
    } else {
        qDebug() << "SelectSourceDialog::onChangeOfCredentials - Credentials unchanged";
    }
}

void SelectSourceDialog::setUrl(const QUrl &url)
{
    ui->credentialWidget->setUrl(url);
}

void SelectSourceDialog::setToken(const QString &token)
{
    ui->credentialWidget->setToken(token);
}


void SelectSourceDialog::updateServerStatus()
{
    if (!m_manager) {
        qDebug() << "Update Server wait no manager";
        return;
    }

    qDebug() << "Update Server Status Check";
    m_wait = false;

    // DEBUG: Log the current state
    qDebug() << "  Manager URL:" << m_manager->projectUrl();
    qDebug() << "  Manager Token:" << (m_manager->token().isEmpty() ? "EMPTY" : "SET");
    qDebug() << "  Project ID:" << m_manager->projectID();
    qDebug() << "  Is Busy:" << m_manager->isBusy();
    qDebug() << "  Has Valid Project ID:" << m_manager->hasValidProjectID();

    if (m_manager->hasValidProjectID()) {
        ui->credentialWidget->setStatus("Url/Token OK");
        qDebug() << "Update Server Url - Status set to OK";
        m_url = m_manager->projectUrl();
        m_token = m_manager->token();
    } else {
        ui->credentialWidget->setStatus("Url/Token Invalid");
        qDebug() << "Update Server Url - Status set to INVALID"
                 << "\n  Reason: projectID =" << m_manager->projectID()
                 << "(needs to be >= 0)";
    }
}


void SelectSourceDialog::accept()
{
    qDebug() << "Select waiting";
    while(m_wait) {
        QApplication::processEvents();
    }

    if ((m_manager->hasValidProjectID()) || (result() != GitLabSelected)) {
        qDebug() << "Select accepted";
        QDialog::accept();
    } else {
        QMessageBox::warning(this, tr("Select Error"), tr("Try re-entering URL and/or token"));
    }
}

QString SelectSourceDialog::getUrl()
{
    return ui->credentialWidget->url().toString();
}


QString SelectSourceDialog::getToken()
{
    return ui->credentialWidget->token();
}

#if 0
void SelectSourceDialog::enableGitLab(bool enable)
{
    if (!enable) {
        ui->gitLabRadioButton->setEnabled(false);
        ui->gitLabRadioButton->setCheckable(false);
        ui->excelRadioButton->setChecked(true);
    }
}
#endif

void SelectSourceDialog::setOptions(enum RequirementsModelBase::modelType type)
{
    switch(type) {
    case RequirementsModelBase::SRS:
        ui->comboBox->setCurrentIndex(0);
        ui->comboBox->setEnabled(false);
        break;

    case RequirementsModelBase::SSS:
        ui->comboBox->setCurrentIndex(1);
        ui->comboBox->setEnabled(false);
        break;

    case RequirementsModelBase::Both:
        ui->comboBox->setCurrentIndex(2);
        ui->comboBox->setEnabled(true);
        break;

    default:
        break;
    }
}


enum RequirementsModelBase::modelType SelectSourceDialog::type()
{
    QString selection = ui->comboBox->currentText();
    enum RequirementsModelBase::modelType type = RequirementsModelBase::Empty;

    if (selection.compare(k_SRSLabel) == 0) {
        type = RequirementsModelBase::SRS;
    }

    if (selection.compare(k_SSSLabel) == 0) {
        type = RequirementsModelBase::SSS;
    }

    if (selection.compare("Both") == 0) {
        type = RequirementsModelBase::Both;
    }

    return type;
}

enum SelectSourceDialog::Selection SelectSourceDialog::result()
{
    if (ui->gitLabRadioButton->isChecked()) return GitLabSelected;
    if (ui->reqIfRadioButton->isChecked()) return ReqIfSelected;
    return ExcelSelected;
}

}
