/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>

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

#include "requirementsmodelbase.h"
#include "requirementsmanager.h"
#include <QApplication>
#include <QMessageBox>

namespace requirement {

/*!
 * \brief Model to hold requirements for a Qt view .
 */
RequirementsModelBase::RequirementsModelBase(requirement::RequirementsManager *manager, QObject *parent)
    : RequirementsModelCommon(manager, parent)
    , m_local(false)
    , m_syncRefs(false)
    , m_type(Both)
    , m_checkingServer(false)
    , m_deleted(QList<Requirement>())
    , m_pendingEdits(false)
    , m_remote(QList<Requirement>())
{
//    m_local = false;
//    m_syncRefs = false;
//    m_type = Both;
    connect(m_manager, &RequirementsManager::projectIDChanged, this, &RequirementsModelBase::updateServerStatus);
    connect(m_manager, &RequirementsManager::fetchingRequirementsEnded, this, &RequirementsModelBase::fetchingFinished);
    connect(this, &RequirementsModelBase::newRequirement, m_manager, &RequirementsManager::createRequirement);
    connect(this, &RequirementsModelBase::updateRequirement, m_manager, &RequirementsManager::editRequirement);
    connect(this, &RequirementsModelBase::deleteRequirement, m_manager, &RequirementsManager::removeRequirement);
}

/*!
 * \brief Model data function retrieves specific data items from the model
 * based on index to a specific requirement and the role for the data item
 * in that requirement.
 */

QVariant RequirementsModelBase::data(const QModelIndex &index, int role) const
{
    QVariant retVar;

    if (!index.isValid() || index.row() >= m_requirements.size()) {
        qDebug() << "Invalid Data Index";
        return QVariant();
    }

    const Requirement &requirement = m_requirements[index.row()];

    switch(role) {
        case TraceCommonModelBase::IssueLinkRole:
            retVar = requirement.m_link;
            break;

        case Qt::DisplayRole:
            switch (index.column()) {
            case REQUIREMENT_ID:
                retVar = requirement.m_id;
                break;
            case TITLE:
                retVar = requirement.m_longName;
                break;
            }
            break;

        case Qt::ToolTipRole:
            retVar = requirement.m_description;
            break;

        case Qt::CheckStateRole:
            if(index.column() == CHECKED) {
                retVar = m_selectedRequirements.contains(getReqIfIdFromModelIndex(index)) ? Qt::Checked : Qt::Unchecked;
            }
            break;

        case RequirementsModelBase::RoleNames::ReqIfIdRole:
            retVar = requirement.m_id;
            break;

        case RequirementsModelBase::RoleNames::TypeRole:
            retVar = requirement.m_type;
            break;

        case RequirementsModelBase::RoleNames::PriorityRole:
            retVar = requirement.m_priority;
            break;

        case RequirementsModelBase::RoleNames::StatusRole:
            retVar = requirement.m_status;
            break;

        case RequirementsModelBase::RoleNames::ValidationRole:
            retVar = validationStringFromLabels(requirement.m_validation);
            break;

        case RequirementsModelBase::RoleNames::JustificationRole:
            retVar = requirement.m_justification;
            break;

        case RequirementsModelBase::RoleNames::ValVersionRole:
            retVar = requirement.m_valVersion;
            break;

        case RequirementsModelBase::RoleNames::ValDescriptionRole:
            retVar = requirement.m_valDescription;
            break;

        case RequirementsModelBase::RoleNames::ValStatusRole:
            retVar = requirement.m_valStatus;
            break;

        case RequirementsModelBase::RoleNames::ValEvidenceRole:
            retVar = requirement.m_valEvidence;
            break;

        case RequirementsModelBase::RoleNames::ComplianceRole:
            retVar = requirement.m_compliance;
            break;

        case RequirementsModelBase::RoleNames::ComplianceStatusRole:
            retVar = requirement.m_complianceStatus;
            break;

        case RequirementsModelBase::RoleNames::NoteRole:
            retVar = requirement.m_note;
            break;

        case RequirementsModelBase::RoleNames::ParentsRole:
            retVar = requirement.m_parents.join('\n');
            break;

        case RequirementsModelBase::RoleNames::ChildrenRole:
            retVar = requirement.m_children.join('\n');
            break;

        case RequirementsModelBase::IssueIdRole:
            retVar = requirement.m_issueIID;
            break;

        case RequirementsModelBase::TagsRole:
            retVar = requirement.m_issue.mLabels;
            break;
        case RequirementsModelBase::TitleRole:
            retVar = requirement.m_longName;
            break;

        case RequirementsModelBase::DetailDescriptionRole:
            retVar = requirement.m_description;
            break;
    }

    return retVar;
}
/*!
 * \brief Clears the checking server flag
 * \note Called when checking server has been completed
 */
void RequirementsModelBase::updateServerStatus()
{
    qDebug() << "Model Checking server ";
    m_checkingServer = false;
}

/*!
 * \brief Changes model type
 * \note Used when requirements are loaded or add to specify requirements types in the model
 */
void RequirementsModelBase::changeModelState(enum RequirementsModelBase::modelType selection)
{
    m_type = selection;
}

/*!
 * \brief Adds a list of requirements to the main model or to the list of remote requirements.
 * \note This functionality is convoluted. The list of remote requirements is fetched when the
 * application want to decide if the changes to be made to the gitlab are new requirements or
 * edits of existing requirements. So it makes a list of remote requirements for comparison.
 *
 * If it is adding the requirements to the current model, then it will filter them depending on
 * the current model type.
 */
void RequirementsModelBase::addRequirements(const QList<Requirement> &requirements)
{
    if(m_local) {
        m_remote << requirements;
        qDebug() << "Add to remote " << requirements.size();
        return;
    }

    qDebug() << "Add to main model " << requirements.size();

    switch (m_type) {
    case SRS: {
            QList<Requirement> filtered;
            for (const Requirement &requirement : requirements) {
                if (requirement.m_issue.mLabels.contains(k_SRSLabel)) {
                    filtered << requirement;
                }
            }
            RequirementsModelCommon::addRequirements(filtered);
            break;
        }
    case SSS: {
            QList<Requirement> filtered;
            for (const Requirement &requirement : requirements) {
                if (requirement.m_issue.mLabels.contains(k_SSSLabel)) {
                    filtered << requirement;
                }
            }
            RequirementsModelCommon::addRequirements(filtered);
            break;
        }
    default:
        RequirementsModelCommon::addRequirements(requirements);
        break;
    }
}

/*!
 * \brief Switch the local flag so that addRequirements will add to
 * the local copy of the remote requirements
 */

void RequirementsModelBase::setLocal()
{
    m_local = true;
}

/*!
 * \brief will clear the model of all requirements
 */

void RequirementsModelBase::clearRequirements()
{
    if (!m_local) {
        setRequirements({});
    }
}

/*!
 * \brief This will start the process of applying the model edits to the gitlab
 * \note It will first delete from the gitlab any requirements deleted in the model.
 * It will then clear the remote list, and refresh the model to start the editing process.
 */

void RequirementsModelBase::applyGitLabEdits(bool allowDelete)
{
    qDebug() << "RequirementsModelBase::applyGitLabEdits allowDelete = " << allowDelete;
    
    if (allowDelete) {
        for (auto &requirement: m_deleted) {
            qDebug() << "Deleting requirement";
            if (!reqIfIDExists(requirement.m_id)) {
                if(requirement.m_issueIID) {
                    qDebug() << "Removing requirement";
                    m_manager->removeRequirement(requirement);
                }
                else
                {
                   qDebug() << "Error " << __FILE__ << __LINE__;
                }
            }
            else
            {
                qDebug() << "Error " << __FILE__ << __LINE__;
            }
        }
    }

    QApplication::processEvents();

    while(m_manager->isBusy()) {
        QApplication::processEvents();
    }

    m_newTarget =  true;
    m_exportType = m_type;
    m_edit = true;
    m_local = true;
    m_remote = QList<Requirement>();
    m_manager->requestAllRequirements(""); /// Starts the process of updating the gitlab
}

/*!
 * \brief Export the current model to the gitlab
 * \note First it will update the gitlab url
 * When the gitlab client is ready it will start the process of updating the gitlab
 */

void RequirementsModelBase::exportGitLabRequirements(QString url, QString token, enum RequirementsModelBase::modelType type)
{
    if (m_newTarget == false) {
        m_url = m_manager->projectUrl();
        m_token = m_manager->token();

        if ((m_url.compare(url) != 0)
        || (m_token.compare(token) != 0)) {
            m_checkingServer = true;
            m_manager->setRequirementsCredentials(url, token);

            qDebug() << "Swap Server IP";
        }
    }

    m_newTarget =  true;
    m_exportType = type;
    m_edit = true;
    m_local = true;

    m_export = QList<Requirement>();
    m_remote = QList<Requirement>();

    while (m_checkingServer)
    {
        QThread::msleep(100);
        QApplication::processEvents();
    }
    qDebug() << "Request remote load";
    m_manager->requestAllRequirements(""); /// Starts the process of updating the gitlab
}

/*!
 * \brief Returns a complete gitlab Issue from the model
 * if the input ReqIf ID is also found in the remote list.
 */
Issue RequirementsModelBase::remoteMatch(QString reqIfId)
{
    for (auto &requirement: m_remote) {
        if (reqIfId.compare(requirement.m_id) == 0) {
            return requirement.m_issue;
        }
    }
    return Issue();
}

/*!
 * \brief This is called when requestAllRequirements has finished re-loading from gitlab
 * \note This functionality is convoluted.
 * If requestAllRequirements has just update the remote list the it will use this to
 * apply edits to existing requirements, or add new requirements. Then it will call
 * requestAllRequirements again to reload the model from the update gitlab.
 *
 * Otherwise - It will check the synchronization of the newly loaded model to see
 * if all the internal references from gitlab and update them if needed. If the model
 * is updated as a result, it will then call requestAllRequirements again to re-load
 * the model again from the gitlab.
 *
 * The new target flag indicates that the gitlab url is for a new gitlab requirements repo
 * as opposed to an existing one.
 */

void RequirementsModelBase::fetchingFinished()
{
    if(m_local && m_edit) {
        qDebug() << "Export from main model";

        for (auto &requirement: m_requirements) {
            if ((m_exportType == RequirementsModelBase::SSS)
            && (requirement.m_reqType.compare(k_SRSLabel)==0)) {
                continue;
            }

            if ((m_exportType == RequirementsModelBase::SRS)
            && (requirement.m_reqType.compare(k_SSSLabel)==0)) {
                continue;
            }

            Issue issue = remoteMatch(requirement.m_id);

            if(issue.mIssueIID) {
                requirement.m_issue = issue;
                requirement.updateIssue(&m_requirements);
                m_manager->editRequirement(requirement);
            }
            else {
                m_manager->createRequirement(requirement);
            }
        }

        QApplication::processEvents();
        while (m_manager->isBusy()) {
            QApplication::processEvents();
        }

        qDebug() << "Export main model complete - request requirements";

        m_remote = QList<Requirement>();

        if (m_newTarget) {
            m_edit = false;
            qDebug() << "Set edit false For new target";
            m_manager->requestAllRequirements("");
        } else {
            qDebug() << "Set local false For Apply";
            m_local = false;
            switch(m_type) {
            case RequirementsModelBase::SSS:
                m_manager->requestAllRequirements(k_SSSLabel);
                break;
            case RequirementsModelBase::SRS:
                m_manager->requestAllRequirements(k_SRSLabel);
                break;
            default:
                m_manager->requestAllRequirements("");
            }
        }
    }
    else {
        qDebug() << __FILE__ << __LINE__;;
        if(syncRequirements())  /// Check synchronization and update the model if needed
        {
            m_syncRefs = false;
            qDebug() << "Sync found edits required";
            pushRequirements();

            if(m_newTarget) {

                qDebug() << "Export Completed reload original IP";

                m_newTarget = false;
                m_local = false;
                if ((m_url.compare(m_manager->projectUrl()) != 0)
                || (m_token.compare(m_manager->token()) != 0)) {
                    m_manager->setRequirementsCredentials(m_url, m_token);
                }
                Q_EMIT exportCompleted();
            } else {

                qDebug() << "Reload original model from Apply";

                switch(m_type) {
                case RequirementsModelBase::SSS:
                    m_manager->requestAllRequirements(k_SSSLabel);
                    break;
                case RequirementsModelBase::SRS:
                    m_manager->requestAllRequirements(k_SRSLabel);
                    break;
                default:
                    m_manager->requestAllRequirements("");
                }
            }
        }
        else {
            m_deleted = QList<Requirement>();

            Q_EMIT exportCompleted();
            qDebug() << "Sync OK -- Apply Completed";
        }
    }
}

/*!
 * \brief Updates all the model requirements on the gitlab
 */
void RequirementsModelBase::pushRequirements()
{
    QList<Requirement> *req_ptr;

    if(m_newTarget) {
        req_ptr = &m_remote;
    } else {
        req_ptr = &m_requirements;
    }

    qDebug() << "Push Called " << req_ptr->size();
    for (auto &requirement : *req_ptr) {

        requirement.updateIssue(req_ptr);

        while(m_manager->isBusy()) {
            QThread::msleep(100);
            QApplication::processEvents();
        }

        m_manager->editRequirement(requirement);
    }
}

/*!
 * \brief Adds a requirement to the requirements model and synchronizes its references locally.
 */

void RequirementsModelBase::createModelRequirement(Requirement &requirement)
{
    QList<Requirement> reqList;

    reqList.swap(m_requirements);
    reqList.append(requirement);
    clearRequirements();
    addRequirements(reqList);

    syncRequirements();
    // Immediately request creation on GitLab (if manager configured).
    // RequirementsModelBase already connects newRequirement -> RequirementsManager::createRequirement
    if (m_manager && m_manager->hasValidProjectID()) {
        Q_EMIT newRequirement(requirement);
        // wait for async manager to finish (pattern used elsewhere)
        while (m_manager->isBusy()) {
            QThread::msleep(100);
            QApplication::processEvents();
        }
    }

}

/*!
 * \brief edits a requirement in the requirements model and synchronizes its references locally.
 */

void RequirementsModelBase::editModelRequirement(Requirement &requirement)
{
    for (int i = 0; i < m_requirements.size(); i++) {
        if (m_requirements[i].m_id.compare(requirement.m_id) == 0) {
            m_requirements[i] = requirement;
            syncRequirements();
            // Immediately request update on GitLab (if manager configured).
            // RequirementsModelBase already connects updateRequirement -> RequirementsManager::editRequirement
            if (m_manager && m_manager->hasValidProjectID()) {
                Q_EMIT updateRequirement(requirement);
                while (m_manager->isBusy()) {
                    QThread::msleep(100);
                    QApplication::processEvents();
                }
            }
            return;
        }
    }

    qDebug() << "Edit Failed";
}
/*!
 * \brief Removes a requirement from the current requirements model.
 * \note  The delete is only applied the local model, the deleted requirement
 * is then added to the delete list and will be applied to gitlab if Apply Edits is called.
 */
void RequirementsModelBase::deleteModelRequirement(const Requirement &requirement)
{
    if(m_requirements.removeAll(requirement)) {
        QList<Requirement> reqList;

        m_deleted.append(requirement);
        syncRequirements();

        for (const auto &req : m_requirements) {
            reqList.append(req);
        }

        clearRequirements();
        addRequirements(reqList);
    }
}

/*!
 * \brief Removes a requirement from the current requirements model without
 *        adding it to the deleted list (server-side deletion already done).
 */
void RequirementsModelBase::deleteModelRequirementDirect(const Requirement &requirement)
{
    if(m_requirements.removeAll(requirement)) {
        QList<Requirement> reqList;

        // No m_deleted append here because the deletion has been performed on the server.
        syncRequirements();

        for (const auto &req : m_requirements) {
            reqList.append(req);
        }

        clearRequirements();
        addRequirements(reqList);
    }
}

void RequirementsModelBase::setPendingEdits(bool pending)
{
    m_pendingEdits = pending;
}

/*!
 * \brief Returns true when there are pending deletions/edits that still require Apply Edits
 *
 * Currently the model uses m_deleted to track deletions that will be applied
 * when Apply Edits is pressed. If this list is empty there are no pending
 * edits to commit to GitLab.
 */
bool RequirementsModelBase::hasPendingEdits() const
{
    return m_pendingEdits || !m_deleted.isEmpty();
}

/*!
 * \brief From the input ReqIf ID, searches for new parents
 * and removes references to parents that no-longer exist.
 */

QStringList RequirementsModelBase::parentsFromId(const QString &reqIfID, const QStringList parents) const
{
    QStringList referenced(parents);

    for (const auto &requirement : m_requirements) {
        if (requirement.m_children.contains(reqIfID)) {
            referenced.append(requirement.m_id);
        }
        if (referenced.contains(requirement.m_id)) {
            if (!requirement.m_children.contains(reqIfID)) {
                referenced.removeAll(requirement.m_id);
            }
        }
    }

    referenced.removeDuplicates();

    return referenced;
}

/*!
 * \brief From the input ReqIf ID, searches for new children
 * and removes references to children that no-longer exist.
 */

QStringList RequirementsModelBase::childrenFromId(const QString &reqIfID, const QStringList children) const
{
    QStringList referenced(children);

    for (const auto &requirement : m_requirements) {
        if (requirement.m_parents.contains(reqIfID)) {
            referenced.append(requirement.m_id);
        }
        if (referenced.contains(requirement.m_id)) {
            if (!requirement.m_parents.contains(reqIfID)) {
                referenced.removeAll(requirement.m_id);
            }
        }
    }

    referenced.removeDuplicates();

    return referenced;
}

/*!
 * \brief Compiles list of unreferenced requirements(ReqIf Ids) for potential choice
 * as parents.
 */

QStringList RequirementsModelBase::unreferencedFromId(const QString &reqIfID) const
{
    QStringList notReferenced;

    for (const auto &requirement : m_requirements) {
        if (reqIfID.isEmpty()
        ||((!requirement.m_parents.contains(reqIfID))
        && (!requirement.m_children.contains(reqIfID))
        && (reqIfID.compare(requirement.m_id) != 0))) {
            notReferenced.append(requirement.m_id);
        }
    }
    return notReferenced;
}

/*!
 * \brief Returns requirement structure from the model based on input ReqIf Id
 */

Requirement RequirementsModelBase::requirementFromIdExtended(const QString &reqIfID, QList<Requirement> *requirements)
{
    auto it = std::find_if(requirements->begin(), requirements->end(),
            [reqIfID](const Requirement &req) { return req.m_id.compare(reqIfID) == 0; });

    if (it != requirements->end()) {
        return *it;
    } else {
        qDebug() << "From reqIfID not found:" << reqIfID;
        return Requirement();
    }
}

/*!
 * \brief Add gitlab markdown local references to a list of ReqIf Ids
 */

QStringList RequirementsModelBase::addInternalRefs(const QStringList &refIfIDs, QList<Requirement> *requirements)
{
    QStringList processed;

    for (QString ref : refIfIDs) {
        if (!ref.isEmpty()) {
            Requirement req = RequirementsModelBase::requirementFromIdExtended(ref, requirements);

            QString internal = QString("[%1](#%2)").arg(ref).arg(req.m_issue.mIssueIID);
            processed << internal;
        }
    }

    return processed;
}

/*!
 * \brief Returns true input ReqIf Id is found in the requirements list
 */

bool RequirementsModelBase::reqIfIDExistsExtended(const QString &reqIfID, QList<Requirement> *requirements) const
{
    return std::any_of(requirements->begin(), requirements->end(),
            [reqIfID](const Requirement &req) { return req.m_id.compare(reqIfID) == 0; });
}

/*!
 * \brief Checks and updates the parent child relationships on the model
 * and then if the model has been altered it returns true so that the calling
 * function can update gitlab with the changes.
 */

bool RequirementsModelBase::syncRequirements()
{
    bool updated = false;
    enum modelType type;
    QList<Requirement> *req_ptr;
    if(m_newTarget) {
        req_ptr = &m_remote;
        type = m_exportType;
        updated = true;
    } else {
        req_ptr = &m_requirements;
        type = m_type;
    }

    qDebug() << "Sync Called size" << req_ptr->size() << " req_ptr " << req_ptr << " type " << type;
    for (auto &requirement : *req_ptr) {
        for (const QString parent : requirement.m_parents) {
            if (reqIfIDExistsExtended(parent, req_ptr)) {
                for (auto &parentRequirement : *req_ptr) {
                    if (parentRequirement.m_id.compare(parent) == 0) {
                        if (!parentRequirement.m_children.contains(requirement.m_id)) {
                            parentRequirement.m_children.append(requirement.m_id);
                            updated = true;
                        }
                    }
                }
            }
            else {
                if (type != RequirementsModelBase::SRS) {
                    qDebug() << "Remove all SRS";
                    requirement.m_parents.removeAll(parent);
                    updated = true;
                }
            }
        }
    }

    for (auto &requirement : *req_ptr) {
        for (const QString dependent : requirement.m_children) {
            if (reqIfIDExistsExtended(dependent, req_ptr)) {
                auto dependentRequirement = requirementFromIdExtended(dependent, req_ptr);
                if(!dependentRequirement.m_parents.contains(requirement.m_id)) {
                    requirement.m_children.removeAll(dependent);
                    updated = true;
                }
            }
            else {
                if (type != RequirementsModelBase::SSS) {
                    requirement.m_children.removeAll(dependent);
                    updated = true;
                }
            }
        }
    }

    if(updated)
    {
        for (auto &requirement : *req_ptr) {
            requirement.updateIssue(req_ptr);
        }
        qDebug() << "Sync does second Issue Update";
   }

    for (auto &requirement : *req_ptr) {
        if (requirement.m_issue.mIssueIID == 0) {
            updated = true;
            qDebug() << "Sync missing IID for " << requirement.m_id;
        }
    }

    return updated;
}

} // namespace shared
