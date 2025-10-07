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
#include "gitlab/gitlabrequirements.h"
#include "requirement.h"
#include "requirementsmodelbase.h"
#include "requirementsmodelcommon.h"


namespace requirement {

Requirement::Requirement()
{
    m_id = QString();
    m_longName = QString();
    m_type = QString();
    m_description = QString();
    m_priority = QString();
    m_status = QString();
    m_parents = QStringList();
    m_children = QStringList();
    m_justification = QString();
    m_compliance = QString();
    m_complianceStatus = QString();
    m_note = QString();
    m_issueIID = 0;
    m_validation.clear();
    m_valVersion = QString();
    m_valDescription = QString();
    m_valStatus = QString();
    m_valEvidence = QString();
    m_reqType = QString();
    m_link = QUrl();
    m_issue = gitlab::Issue();
    m_issue.mLabels = {k_requirementsTypeLabel};
    m_providence = Requirement::Created;
}

Requirement::Requirement(const gitlab::Issue &issue) :
    m_id(parseReqIfId(issue)),
    m_type(parseTextField(issue, TypeField)),
    m_longName(issue.mTitle),
    m_description(parseTextField(issue, DescriptionField)),
    m_priority(parseTextField(issue, PriorityField)),
    m_status(parseTextField(issue, StatusField)),
    m_parents(removeInternalRefs(parseTextList(issue, ParentsField))),
    m_children(removeInternalRefs(parseTextList(issue, ChildrenField))),
    m_justification(parseTextField(issue, JustificationField)),
    m_compliance(parseTextField(issue, ComplianceField)),
    m_complianceStatus(parseTextField(issue, ComplianceStatusField)),
    m_note(parseTextField(issue, NoteField)),
    m_issueIID(issue.mIssueIID),
    m_validation(validationFromLabels(issue.mLabels)),
    m_valVersion(parseTextField(issue, ValVersionField)),
    m_valDescription(parseTextField(issue, ValDescriptionField)),
    m_valStatus(parseTextField(issue, ValStatusField)),
    m_valEvidence(parseTextField(issue, ValEvidenceField)),
    m_reqType(reqTypeFromLabels(issue.mLabels)),
    m_link(issue.mUrl),
    m_issue(gitlab::Issue(issue))
{
//    m_issue.mLabels = {k_requirementsTypeLabel};
}

bool Requirement::isValid() const
{
    return !m_id.isEmpty() && !m_longName.isEmpty();
}

void Requirement::updateFromRole(int role, QString text)
{

    switch(role) {

        case RequirementsModelBase::RoleNames::ReqIfIdRole:
            m_id = text;
            break;

        case RequirementsModelBase::TitleRole:
            m_longName = text;
            break;

        case RequirementsModelBase::DetailDescriptionRole:
            m_description = text;
            break;

        case RequirementsModelBase::RoleNames::TypeRole:
            m_type = text;
            break;

        case RequirementsModelBase::RoleNames::PriorityRole:
            m_priority = text;
            break;

        case RequirementsModelBase::RoleNames::StatusRole:
            m_status = text;
            break;

        case RequirementsModelBase::RoleNames::ValidationRole:
            m_validation = RequirementsModelCommon::validationLabelsFromString(text);
            break;

        case RequirementsModelBase::RoleNames::JustificationRole:
            m_justification = text;
            break;

        case RequirementsModelBase::RoleNames::ValVersionRole:
            m_valVersion = text;
            break;

        case RequirementsModelBase::RoleNames::ValDescriptionRole:
            m_valDescription = text;
            break;

        case RequirementsModelBase::RoleNames::ValStatusRole:
            m_valStatus = text;
            break;

        case RequirementsModelBase::RoleNames::ValEvidenceRole:
            m_valEvidence = text;
            break;

        case RequirementsModelBase::RoleNames::ComplianceRole:
            m_compliance = text;
            break;

        case RequirementsModelBase::RoleNames::ComplianceStatusRole:
            m_complianceStatus = text;
            break;

        case RequirementsModelBase::RoleNames::NoteRole:
            m_note = text;
            break;

        case RequirementsModelBase::TagsRole:
            m_issue.mLabels;
            break;
    }
}

QString Requirement::parseReqIfId(const gitlab::Issue &issue)
{
    bool valid = false;

    for (const QString &line : issue.mDescription.split("|")) {
        if (valid) {
            QString id = line.trimmed();
            id = id.trimmed();
            if (id.startsWith(":")) {
                id = id.sliced(1).trimmed();
            }

            // Remove quotes if there
            if (id.startsWith("\"")) {
                id = id.sliced(1);
            }
            if (id.endsWith("\"")) {
                id.chop(1);
            }

            return id;
        }
        else
        {
            if (line.contains(startReqIfId)) {
                valid = true;
            }
        }
    }
    return QString::number(issue.mIssueIID);
}

QString Requirement::parseTextField(const gitlab::Issue &issue, const QString field)
{
    bool first = true;
    bool valid = false;

    QString text;
    for (QString &line : issue.mDescription.split("|")) {
        if (valid) {
            text = line.replace("<br>", "\n");
            valid = false;

            return text;
        }
        else {
            if (line.contains(field)) {
                valid = true;
            }
        }
    }
    return text;
}

QStringList Requirement::removeInternalRefs(const QStringList reqIfIDs)
{
    QStringList processed;

    for (QString reqIfID : reqIfIDs) {
        if (reqIfID.startsWith('[')) {
            int end = reqIfID.indexOf(']');

            if (end > 1) {
                processed << reqIfID.mid(1, end - 1);
            }
        }
        else {
            processed << reqIfID;
        }
    }

    processed.removeAll("");
    return processed;
}

QStringList Requirement::parseTextList(const gitlab::Issue &issue, const QString field)
{
    QStringList list = parseTextField(issue, field).split('\n');
    list.removeAll("");
    list.removeDuplicates();
    return list;
}

void Requirement::updateIssue(QList<Requirement> *requirements)
{
    QString mutableDescription(m_description);
    mutableDescription.replace("\n", "<br>");

    QString mutableJustification(m_justification);
    mutableJustification.replace("\n", "<br>");

    QString mutableValDescription(m_valDescription);
    mutableValDescription.replace("\n", "<br>");

    QString mutableValEvidence(m_valEvidence);
    mutableValEvidence.replace("\n", "<br>");

    QString mutableNotes(m_note);
    mutableNotes.replace("\n", "<br>");

    QString validation = RequirementsModelCommon::validationStringFromLabels(m_validation);

    QStringList parents = RequirementsModelBase::addInternalRefs(m_parents, requirements);
    QStringList childrenchildren = RequirementsModelBase::addInternalRefs(m_children, requirements);

    QString descr = QString("| Section | Data |\n");
    descr += QString("| :----------------- | :---------------------------------------- | \n");
    descr += QString ("| %1 |%2|\n").arg(startReqIfId).arg(m_id);
    descr += QString ("| %1 |%2|\n").arg(TypeField).arg(m_type);
    descr += QString ("| %1 |%2|\n").arg(DescriptionField).arg(mutableDescription);
    descr += QString ("| %1 |%2|\n").arg(PriorityField).arg(m_priority);
    descr += QString ("| %1 |%2|\n").arg(StatusField).arg(m_status);
    descr += QString ("| %1 |%2|\n").arg(ParentsField).arg(parents.join("<br>").trimmed());
    descr += QString ("| %1 |%2|\n").arg(ChildrenField).arg(childrenchildren.join("<br>").trimmed());
    descr += QString ("| %1 |%2|\n").arg(JustificationField).arg(mutableJustification);
    descr += QString ("| %1 |%2|\n").arg(ValidationField).arg(validation);
    descr += QString ("| %1 |%2|\n").arg(ValVersionField).arg(m_valVersion);
    descr += QString ("| %1 |%2|\n").arg(ValDescriptionField).arg(mutableValDescription);
    descr += QString ("| %1 |%2|\n").arg(ValStatusField).arg(m_valStatus);
    descr += QString ("| %1 |%2|\n").arg(ValEvidenceField).arg(mutableValEvidence);
    descr += QString ("| %1 |%2|\n").arg(ComplianceField).arg(m_compliance);
    descr += QString ("| %1 |%2|\n").arg(ComplianceStatusField).arg(m_complianceStatus);
    descr += QString ("| %1 |%2|\n").arg(NoteField).arg(mutableNotes);
    QStringList labels = m_validation << m_reqType << k_requirementsTypeLabel;
    labels.removeDuplicates();
    m_issue.mTitle = m_longName;
    m_issue.mDescription = descr;
    m_issue.mLabels = labels;
}

/*!
 * Returns the tags from the list of labels provided by the gitlab server
 */
QStringList Requirement::validationFromLabels(const QStringList &labels)
{
    QStringList val;
    for (const QString &label : labels) {
        if (label == k_validationTestLabel) {
            val.append(label);
        }

        if (label == k_validationAnalysisLabel) {
            val.append(label);
        }

        if (label == k_validationDesignLabel) {
            val.append(label);
        }

        if (label == k_validationInspectionLabel) {
            val.append(label);
        }
    }
    return val;
}

QString Requirement::reqTypeFromLabels(const QStringList &labels)
{
    for (const QString &label : labels) {
        if (label == k_SSSLabel) {
            return label;
        }
    }

    return k_SRSLabel;
}

bool Requirement::operator==(const Requirement &req) const
{
    return (req.m_id == this->m_id);
}

} // namespace requirement

QDebug operator<<(QDebug debug, const requirement::Requirement &r)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "Requirement(" << r.m_id << ", " << r.m_longName << ", issue:" << r.m_issueIID << ")";
    return debug;
}

