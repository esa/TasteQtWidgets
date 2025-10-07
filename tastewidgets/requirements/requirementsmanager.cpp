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

#include "requirementsmanager.h"

#include "gitlab/gitlabrequirements.h"
#include "issuerequestoptions.h"
#include "issuesmanagerprivate.h"
#include "qgitlabclient.h"

#include <QDir>
#include <QApplication>
#include <QThread>

namespace requirement {

struct RequirementsManager::RequirementsManagerPrivate : public tracecommon::IssuesManagerPrivate {
    RequirementsManagerPrivate(RequirementsManager::REPO_TYPE rType)
        : IssuesManagerPrivate(rType)
    {
        switch (repoType) {
        case REPO_TYPE::GITLAB:
            gitlabRequirements = std::make_unique<requirement::GitLabRequirements>();
        }
    }

    std::unique_ptr<requirement::GitLabRequirements> gitlabRequirements;
};

RequirementsManager::RequirementsManager(REPO_TYPE repoType, QObject *parent)
    : tracecommon::IssuesManager(parent)
    , d(std::make_unique<RequirementsManagerPrivate>(repoType))
    , m_issueBufferFlag(false)
{
    init(d.get());
    switch (d->repoType) {
    case (REPO_TYPE::GITLAB): {
        connect(d->gitlabClient.get(), &gitlab::QGitlabClient::listOfIssues, d->gitlabRequirements.get(),
                &requirement::GitLabRequirements::listOfIssues);
//        connect(d->gitlabClient.get(), &gitlab::QGitlabClient::issueCreated, this,
//                &RequirementsManager::requirementAdded);
//        connect(d->gitlabClient.get(), &gitlab::QGitlabClient::issueClosed, this,
//                &RequirementsManager::requirementClosed);
        connect(d->gitlabClient.get(), &gitlab::QGitlabClient::issueFetchingDone, this,
                &RequirementsManager::fetchingRequirementsEnded);
        connect(d->gitlabClient.get(), &gitlab::QGitlabClient::listOfLabels, this, [this](QList<gitlab::Label> labels) {
            m_tagsBuffer.append(GitLabRequirements::tagsFromLabels(labels));
        });
        connect(d->gitlabRequirements.get(), &requirement::GitLabRequirements::listOfRequirements, this,
                &RequirementsManager::listOfRequirements);
        break;
    }
    default:
        qDebug() << "unknown repository type";
    }

//    connect(this, &RequirementsManager::updateIssue, this, &RequirementsManager::issueEdit);
}

RequirementsManager::~RequirementsManager() { }

/*!
 * Starts a request to load all requirements from the server. The requirements will be delivered by the
 * listOfRequirements signal.
 * \return Returns true when everything went well.
 */
bool RequirementsManager::requestAllRequirements(QString typeLabel)
{
    QStringList labels;

    labels << k_requirementsTypeLabel;

    if (!typeLabel.isEmpty()) {
        labels << typeLabel;
    }

    switch (d->repoType) {
    case (REPO_TYPE::GITLAB): {
        gitlab::IssueRequestOptions options;
        options.mProjectID = m_projectID;
        options.mLabels = { labels };
        const bool wasBusy = d->gitlabClient->requestIssues(options);
        if (wasBusy) {
            qDebug() << "Manager Request All Requirements GitLab busy";
            return false;
        }
        Q_EMIT startingFetchingRequirements();
        return true;
    }
    default:
        qDebug() << "unknown repository type";
    }
    return false;
}

/*!
 * Creates a new requirement on the server.
 */
void RequirementsManager::createRequirement(const Requirement &requirement) const
{
    while (isBusy())
    {
        QApplication::processEvents();
    }

    qDebug() << "Create " << requirement.m_id;

//    const QString message  = QString("Create %1").arg(requirement.m_id);

//    Q_EMIT report(m_message);

    Q_EMIT reportProgress(QString("Create %1").arg(requirement.m_id));

    switch (d->repoType) {
    case (REPO_TYPE::GITLAB): {

        const bool wasBusy = d->gitlabClient->createIssue(m_projectID, requirement.m_issue.mTitle, requirement.m_issue.mDescription, requirement.m_issue.mLabels);

        if (wasBusy) qDebug() << requirement.m_id  << " Requirement NOT Created";
        return;
    }
    default:
        qDebug() << "unknown repository type";
    }
    return;
}


void RequirementsManager::editRequirement(const Requirement &requirement) const
{
    while (d->gitlabClient->isBusy()) {
        QApplication::processEvents();
    }

    qDebug() << "Edit " << requirement.m_id;

    Q_EMIT reportProgress(QString("Edit %1").arg(requirement.m_id));

    switch (d->repoType) {
    case (REPO_TYPE::GITLAB): {
        const bool wasBusy = d->gitlabClient->editIssue(m_projectID, requirement.m_issue.mIssueIID, requirement.m_issue);

        if (wasBusy) qDebug() << requirement.m_id  << " Requirement NOT Edited";
        return;
    }
    default:
        qDebug() << "unknown repository type";
    }

    return;
}


void RequirementsManager::removeRequirement(const Requirement &requirement) const
{
    while (isBusy()) {
        QApplication::processEvents();
    }

    qDebug() << "Remove " << requirement.m_id;
    Q_EMIT reportProgress(QString("Remove %1").arg(requirement.m_id));

    switch (d->repoType) {
    case (REPO_TYPE::GITLAB): {
        const bool wasBusy = d->gitlabClient->closeIssue(m_projectID, requirement.m_issueIID);

        if (wasBusy) qDebug() << requirement.m_id << " Requirement NOT Deleted";
        return;
    }
    default:
        qDebug() << "unknown repository type";
    }
}


}
