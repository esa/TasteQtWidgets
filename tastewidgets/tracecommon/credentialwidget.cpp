/*
   Copyright (C) 2024 European Space Agency - <maxime.perrotin@esa.int>

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

#include "credentialwidget.h"

#include "ui_credentialwidget.h"

#include <QDesktopServices>

namespace tracecommon {

CredentialWidget::CredentialWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CredentialWidget)
{
    ui->setupUi(this);

    connect(ui->urlLineEdit, &QLineEdit::textChanged, this, &CredentialWidget::updateTockenButton);

    const QString urlTooltip = tr("Set the Gitlab server URL including the project path");
    ui->urlLabel->setToolTip(urlTooltip);
    ui->urlLineEdit->setToolTip(urlTooltip);
    const QString tokenTooltip = tr(
            "To create a personal access token, go to  Profile > Preferences > Access Tokens \n in your gitlab server");
    ui->tokenLabel->setToolTip(tokenTooltip);
    ui->tokenLineEdit->setToolTip(tokenTooltip);
    connect(ui->createTokenButton, &QPushButton::clicked, this, &CredentialWidget::openTokenSettingsPage);
    connect(ui->urlLineEdit, &QLineEdit::editingFinished, this, &CredentialWidget::urlChanged);
    connect(ui->tokenLineEdit, &QLineEdit::editingFinished, this, &CredentialWidget::tokenChanged);
}

CredentialWidget::~CredentialWidget()
{
    delete ui;
}

/*!
 * Sets the URL to fetch the requirements from
 */
void CredentialWidget::setUrl(const QUrl &url)
{
    const QString urlStr = url.toString();
    if (urlStr == ui->urlLineEdit->text()) {
        return;
    }
    ui->urlLineEdit->setText(urlStr);
}

/*!
 * Returns the URL to fetch the requiremenst from
 */
QUrl CredentialWidget::url() const
{
    return QUrl(ui->urlLineEdit->text());
}

/*!
 * Sets the token to authenticate for fetching the requirements
 */
void CredentialWidget::setToken(const QString &token)
{
    if (token == ui->tokenLineEdit->text()) {
        return;
    }
    ui->tokenLineEdit->setText(token);
}

/*!
 * Returns the token to authenticate for fetching the requirements
 */
QString CredentialWidget::token() const
{
    return ui->tokenLineEdit->text();
}

void CredentialWidget::updateTockenButton()
{
    ui->createTokenButton->setEnabled(url().isValid());
}

void CredentialWidget::openTokenSettingsPage()
{
    QUrl url(ui->urlLineEdit->text());
    url.setPath("/-/user_settings/personal_access_tokens");
    QDesktopServices::openUrl(url);
}

void CredentialWidget::setStatus(const QString &status)
{
    ui->statusLabel->setText(status);
}

} // namespace tracecommon
