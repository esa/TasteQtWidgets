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

#include "issuesmanager.h"

#include "issuesmanagerprivate.h"
#include "labelsrequestoptions.h"
#include "qgitlabclient.h"

#include <QDebug>

namespace tracecommon {

IssuesManager::IssuesManager(QObject *parent)
    : QObject { parent }
{
}

/*!
 * Sets the url and authentification credentials to be able to load data from the server.
 * It does trigger a re-load of all requirements after the data is validated by the server (project ID set).
 * \param url The URL of the project to fetch the requirements
 * \param token The authentikation token for the server
 * \return Returns true when everything went well.
 */
bool IssuesManager::setCredentials(const QString &url, const QString &token)
{
    if (m_projectUrl == url && token == m_token) {
        return true;
    }

    m_projectUrl = url;
    m_token = token;
    setProjectID(-1);

    Q_EMIT projectUrlChanged(m_projectUrl);
    Q_EMIT tokenChanged(m_token);

    if (m_projectUrl.isEmpty() || m_token.isEmpty()) {
        return false;
    }

    QUrl _url;
    _url.setScheme("https");
    _url.setHost(QUrl(url).host());
    _url.setPath("/api/v4/");

    switch (m_d->repoType) {

    case (REPO_TYPE::GITLAB):
        m_d->gitlabClient->setCredentials(_url.scheme() + "://" + _url.host(), token);
    }
    return requestProjectID(url);
}

bool IssuesManager::setRequirementsCredentials(const QString &url, const QString &token)
{
    return setCredentials(url, token);
}
/*!
 * URL of the project to load the requirements data from
 */
QString IssuesManager::projectUrl() const
{
    return m_projectUrl.toString();
}

/*!
 * The token used to authenticate on the server
 */
const QString &IssuesManager::token() const
{
    return m_token;
}

/*!
 * Returns true when there is currently a request to load data running
 */
bool IssuesManager::isBusy() const
{
    switch (m_d->repoType) {

    case (REPO_TYPE::GITLAB):
        return m_d->gitlabClient->isBusy();
    }
    return false;
}

/*!
 * Returns the internal ID of the project
 */
const int &IssuesManager::projectID() const
{
    return m_projectID;
}
/*!
 * Returns if the manager has a project to work on
 */
bool IssuesManager::hasValidProjectID() const
{
    return m_projectID >= 0;
}

bool IssuesManager::requestTags()
{
    switch (m_d->repoType) {
    case (REPO_TYPE::GITLAB): {
        gitlab::LabelsRequestOptions options;
        options.mProjectID = m_projectID;
        const bool wasBusy = m_d->gitlabClient->requestListofLabels(options);
        if (wasBusy) {
            return false;
        }
        m_tagsBuffer.clear();
        return true;
    }
    default:
        qDebug() << "unknown repository type";
    }
    return false;
}

QStringList IssuesManager::tagsBuffer()
{
    return m_tagsBuffer;
}

void IssuesManager::setProjectID(const int &newProjectID)
{
// Always emit projectIDChanged because multiple invalid projects will all have projectID -1
// So you cannot test multiple incorrect Urls if you don't get multiple projectIDChanged messages

    m_projectID = newProjectID;
    Q_EMIT projectIDChanged();
}

void IssuesManager::init(IssuesManagerPrivate *priv)
{
    m_d = priv;
    Q_ASSERT(m_d);
    switch (m_d->repoType) {
    case (REPO_TYPE::GITLAB): {
        connect(m_d->gitlabClient.get(), &gitlab::QGitlabClient::connectionError, this,
                &IssuesManager::connectionError);
        connect(m_d->gitlabClient.get(), &gitlab::QGitlabClient::busyStateChanged, this, &IssuesManager::busyChanged);
        connect(m_d->gitlabClient.get(), &gitlab::QGitlabClient::requestedProjectID, this,
                &IssuesManager::setProjectID);
        connect(m_d->gitlabClient.get(), &gitlab::QGitlabClient::labelsFetchingDone, this,
                [this] { Q_EMIT listOfTags(m_tagsBuffer); });
        break;
    }
    default:
        qDebug() << "unknown repository type";
    }
}

bool IssuesManager::requestProjectID(const QUrl &url)
{
    switch (m_d->repoType) {
    case (REPO_TYPE::GITLAB): {
        const bool wasBusy = m_d->gitlabClient->requestProjectId(url);
        return !wasBusy;
    }
    default:
        qDebug() << "unknown repository type";
    }
    return false;
}

} // namespace tracecommon
