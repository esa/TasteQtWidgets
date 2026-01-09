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

#include "requirementsmodelcommon.h"

#include "requirementsmanager.h"
#include <iostream>
#include <QtConcurrent>
#include <QFuture>
#include <QApplication>

using namespace tracecommon;

namespace requirement {


RequirementsModelCommon::RequirementsModelCommon(RequirementsManager *manager, QObject *parent)
    : TraceCommonModelBase(parent)
    , m_manager(manager)
{
    if (m_manager != nullptr) {
        connect(m_manager, &RequirementsManager::listOfRequirements, this, &RequirementsModelCommon::addRequirements);
        connect(m_manager, &RequirementsManager::startingFetchingRequirements, this, &RequirementsModelCommon::clearRequirements);
    }
}


void RequirementsModelCommon::clearRequirements()
{
    setRequirements({});
}

/*!
 * Replaces the set of existing requirements with the given one
 */
void RequirementsModelCommon::setRequirements(const QList<Requirement> &requirements)
{
    beginResetModel();
    m_requirements = requirements;
    endResetModel();
}

/*!
 * Appends the given \a requiremnets to the existing ones
 */
void RequirementsModelCommon::addRequirements(const QList<Requirement> &requirements)
{
    QList<Requirement> reqs;

    std::copy_if(requirements.begin(), requirements.end(), std::back_inserter(reqs),
            [this](const Requirement &req) { return !m_requirements.contains(req); });

    beginInsertRows(QModelIndex(), m_requirements.size(), m_requirements.size() + reqs.size() - 1);
    m_requirements.append(reqs);
    endInsertRows();
}


QVariant RequirementsModelCommon::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        if (section == REQUIREMENT_ID) {
            return tr("Requirement Identifier");
        }
        if (section == TITLE) {
            return tr("Title");
        }
        if (section == CHECKED) {
            return tr("Selected");
        }
    }

    return {};
}

int RequirementsModelCommon::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_requirements.size();
}

int RequirementsModelCommon::columnCount(const QModelIndex &parent) const
{
    return 3;
}

const gitlab::Issue* RequirementsModelCommon::issue(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() >= m_requirements.size()) {
        return nullptr;
    }

    const Requirement &requirement = m_requirements[index.row()];

    return &requirement.m_issue;
}

QString RequirementsModelCommon::validationStringFromLabels(const QStringList &labels)
{
    QString val = "";
    QStringList mutableLabels(labels);
    mutableLabels.sort();

    for (const QString &label : mutableLabels) {
        if (label == k_validationTestLabel) {
            val  = val + " T,";
        }

        if (label == k_validationAnalysisLabel) {
            val  = val + " A,";
        }

        if (label == k_validationDesignLabel) {
            val  = val + " D,";
        }

        if (label == k_validationInspectionLabel) {
            val  = val + " I,";
        }
    }
    if (!val.isEmpty()) {
        val.remove(0,1);
        val.chop(1);
    }
    return val;
}

QStringList RequirementsModelCommon::validationLabelsFromString(const QString &string)
{
    QStringList lab;
    QString mutableString(string);

    if (!mutableString.isEmpty()) {
        mutableString.insert(0,' ');
        mutableString.append(',');

        if (mutableString.contains(" A,")) {
            lab << k_validationAnalysisLabel;
        }
        if (mutableString.contains(" D,")) {
            lab << k_validationDesignLabel;
        }
        if (mutableString.contains(" I,")) {
            lab << k_validationInspectionLabel;
        }
        if (mutableString.contains(" T,")) {
            lab << k_validationTestLabel;
        }

        lab.sort();
    }

    return lab;
}

/*!
 * \brief Model data function retrieves specific data items from the model
 * based on index to a specific requirement and the role for the data item
 * in that requirement.
 */

QVariant RequirementsModelCommon::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_requirements.size()) {
        return QVariant();
    }

    const Requirement &requirement = m_requirements[index.row()];

    if (role == TraceCommonModelBase::IssueLinkRole) {
        return requirement.m_link;
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case REQUIREMENT_ID:
            return requirement.m_id;
        case TITLE:
            return requirement.m_longName;
        }
    }
    if (role == Qt::ToolTipRole) {
        return requirement.m_description;
    }

    if (role == Qt::CheckStateRole && index.column() == CHECKED) {
        return m_selectedRequirements.contains(getReqIfIdFromModelIndex(index)) ? Qt::Checked : Qt::Unchecked;
    }

    if (role == RequirementsModelCommon::RoleNames::ReqIfIdRole) {
        return requirement.m_id;
    }

    if (role == TraceCommonModelBase::IssueIdRole) {
        return requirement.m_issueIID;
    }

    if (role == TraceCommonModelBase::TitleRole) {
        return requirement.m_longName;
    }

    if (role == TraceCommonModelBase::DetailDescriptionRole) {
        return requirement.m_description;
    }

    return QVariant();
}


bool RequirementsModelCommon::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == Qt::CheckStateRole && index.column() == CHECKED) {
        const QString requirementID = getReqIfIdFromModelIndex(index);
        bool checked = value.toBool();
        if (checked) {
            m_selectedRequirements.append(requirementID);
        } else {
            m_selectedRequirements.removeAll(requirementID);
        }
        emit dataChanged(index, index, { role });
        return true;
    }

    return QAbstractTableModel::setData(index, value, role);
}


QString RequirementsModelCommon::getReqIfIdFromModelIndex(const QModelIndex &index) const
{
    auto idx = this->index(index.row(), REQUIREMENT_ID);
    return this->data(idx, RequirementsModelCommon::ReqIfIdRole).toString();
}

Qt::ItemFlags RequirementsModelCommon::flags(const QModelIndex &index) const
{
    auto flags = QAbstractTableModel::flags(index);

    if (index.column() == CHECKED) {
        flags |= Qt::ItemIsUserCheckable;
    }

    return flags;
}

QStringList RequirementsModelCommon::selectedRequirements() const
{
    return m_selectedRequirements;
}

void RequirementsModelCommon::setSelectedRequirements(const QStringList &selected)
{
    beginResetModel();
    m_selectedRequirements = selected;
    endResetModel();
}

Requirement RequirementsModelCommon::requirementFromIndex(const QModelIndex &idx)
{
    QString reqIfId = getReqIfIdFromModelIndex(idx);

    for (const auto &requirement : m_requirements) {
        if (requirement.m_id.compare(reqIfId) == 0) {
            return requirement;
        }
    }
    qDebug() << "From Index not found:" << reqIfId;
    return Requirement();
}

bool RequirementsModelCommon::reqIfIDExists(const QString &reqIfID) const
{
    return std::any_of(m_requirements.begin(), m_requirements.end(),
            [reqIfID](const Requirement &req) { return req.m_id.compare(reqIfID) == 0; });
}


Requirement RequirementsModelCommon::requirementFromId(const QString &reqIfID) const
{
    auto it = std::find_if(m_requirements.begin(), m_requirements.end(),
            [reqIfID](const Requirement &req) { return req.m_id.compare(reqIfID) == 0; });

    if (it != m_requirements.end()) {
        return *it;
    } else {
        qDebug() << "From reqIfID not found:" << reqIfID;
        return Requirement();
    }
}


} // namespace requirement
