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

#pragma once

#include <QUrl>
#include <QWidget>

namespace tracecommon {

namespace Ui {
class CredentialWidget;
}

/*!
 * Widget for editing the URL/credential for loading data from a gitlab server
 */
class CredentialWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CredentialWidget(QWidget *parent = nullptr);
    ~CredentialWidget();

    void setUrl(const QUrl &url);
    QUrl url() const;
    void setToken(const QString &token);
    QString token() const;
    void setStatus(const QString &status);

Q_SIGNALS:
    void urlChanged();
    void tokenChanged();

protected Q_SLOTS:
    void updateTockenButton();
    void openTokenSettingsPage();

private:
    Ui::CredentialWidget *ui;
};

} // namespace tracecommon
