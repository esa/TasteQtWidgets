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

#include "issuesmanager.h"
#include "requirement.h"

#include <memory>
#include "../qgitlabapi/issue.h"

using namespace gitlab;

namespace requirement {

/*!
 * The RequirementsManager is responsible to communicate to the server/storage that handles the requirments for
 * components
 */
class RequirementsManager : public tracecommon::IssuesManager
{
    Q_OBJECT

public:
    RequirementsManager(REPO_TYPE repoType = tracecommon::IssuesManager::REPO_TYPE::GITLAB, QObject *parent = nullptr);
    ~RequirementsManager();

    /*!
     * \brief Makes a request to retrieve all the requirements
     * \return Returns true if there's a pending request otherwise false.
     */
    bool requestAllRequirements(QString typeLabel);

Q_SIGNALS:
    /*!
     * \brief This signal marks the start of the requirements fetching
     */
    void startingFetchingRequirements();
    /*!
     * \brief This signal marks the end of the requirements fetching
     */
    void fetchingRequirementsEnded();
    /*!
     * \brief This signal carries the list of requirements fetched from Gitlab server
     */
    void listOfRequirements(const QList<requirement::Requirement> &);
    /*!
     * \brief This signal is triggered when a Requirement is created
     */
    void requirementAdded();
    /*!
     * \brief This signal is triggered when a Requirement is closed
     */
    void requirementClosed();

    void reportProgress(const QString &) const;

//    void updateIssue();

public Q_SLOTS:
    void removeRequirement(const Requirement &requirement) const;
    void createRequirement(const Requirement &requirement) const;
    void editRequirement(const Requirement &requirement) const;

private:
    class RequirementsManagerPrivate;
    std::unique_ptr<RequirementsManagerPrivate> d;
    volatile bool m_issueBufferFlag;
    gitlab::Issue *m_issuebuffer;
    QString m_message;
};

}
