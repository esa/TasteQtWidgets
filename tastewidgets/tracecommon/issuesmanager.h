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

#include <QObject>
#include <QStringList>
#include <QUrl>

namespace tracecommon {
class IssuesManagerPrivate;

class IssuesManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int projectID READ projectID WRITE setProjectID NOTIFY projectIDChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QStringList tagsBuffer READ tagsBuffer)
    Q_PROPERTY(QUrl projectUrl READ projectUrl NOTIFY projectUrlChanged)
    Q_PROPERTY(QString token READ token NOTIFY tokenChanged)

public:
    enum class REPO_TYPE
    {
        GITLAB
    };

    explicit IssuesManager(QObject *parent = nullptr);

    bool setCredentials(const QString &url, const QString &token);
    bool setRequirementsCredentials(const QString &url, const QString &token);
    QString projectUrl() const;
    const QString &token() const;

    bool isBusy() const;
    const int &projectID() const;
    virtual bool hasValidProjectID() const;

    QStringList tagsBuffer();

public Q_SLOTS:
    bool requestTags();
    void setProjectID(const int &newProjectID);

Q_SIGNALS:
    void busyChanged();
    void projectIDChanged();
    void listOfTags(const QStringList &);
    void connectionError(QString errorString);
    void projectUrlChanged(const QUrl &url);
    void tokenChanged(const QString &token);

protected:
    void init(IssuesManagerPrivate *priv);

    int m_projectID = -1;
    QUrl m_projectUrl = {};
    QString m_token = "";
    QStringList m_tagsBuffer;
    bool requestProjectID(const QUrl &url);

    IssuesManagerPrivate *m_d = nullptr;
};

} // namespace tracecommon
