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

#include <QDebug>
#include <QString>
#include <QUrl>

#include "../qgitlabapi/issue.h"
#include "../qgitlabapi/label.h"

using namespace gitlab;

namespace requirement {
class RequirementsModelBase;

static const QString k_validationTestLabel = "test";
static const QString k_validationAnalysisLabel = "analysis";
static const QString k_validationDesignLabel = "design";
static const QString k_validationInspectionLabel = "inspection";
static const QString k_SSSLabel = "SSS";
static const QString k_SRSLabel = "SRS";

static const QString startReqIfId("**Requirement Identifier**");
static const QString TypeField("**Requirement Type**");
static const QString DescriptionField("**Requirement**");
static const QString PriorityField("**Priority**");
static const QString StatusField("**Requirement Status**");
static const QString ParentsField("**Parents**");
static const QString ChildrenField("**Children**");
static const QString JustificationField("**Rationale / Justification**");
static const QString ValidationField("**Validation Method**");
static const QString ValVersionField("**Validation Version**");
static const QString ValDescriptionField("**Validation Description**");
static const QString ValStatusField("**Validation Status**");
static const QString ValEvidenceField("**Validation Evidence**");
static const QString ComplianceField("**Compliance**");
static const QString ComplianceStatusField("**Compliance Status**");
static const QString NoteField("**Note**");

static const QStringList PriorityList = {"Unassigned", "Low", "Medium", "High", "Urgent"};
static const QStringList StatusList = {"Not Started","In Work", "Under Review", "Approved"};
static const QStringList ComplianceList = {"Not Compliant", "Partially Compliant", "Compliant"};

// These requirement types can appear in an SRS.
static const QStringList TypeListSRS = {
        "Functional",
        "Performance",
        "Interface",
        "Operational",
        "Resources",
        "Design Req",
        "Security",
        "Portability",
        "Quality",
        "Reliability",
        "Maintainability",
        "Safety",
        "Config",
        "Data Def",
        "Human Factors",
        "Adaptation"
};
// These requirement types can appear in an SSS.
static const QStringList TypeListSSS = {
  "Capabilities",
  "Interface",
  "Adaptation",
  "Resources",
  "Security",
  "Safety",
  "Reliability",
  "Design",
  "Operations",
  "Maintenance",
  "Observability",
  "V&V Process",
  "Validation Approach",
  "Validation",
   "Verification"
};


class Requirement
{
public:
    Requirement();
    Requirement(const gitlab::Issue &issue);
    int m_issueIID;
    QString m_reqType;
    QString m_id;
    QString m_type;
    QString m_longName;
    QString m_description;
    QString m_priority;
    QString m_status;
    QStringList m_parents;
    QStringList m_children;
    QString m_justification;
    QStringList m_validation;
    QString m_valVersion;
    QString m_valDescription;
    QString m_valStatus;
    QString m_valEvidence;
    QString m_compliance;
    QString m_complianceStatus;
    QString m_note;
    QUrl m_link;
    gitlab::Issue m_issue;

    void updateIssue(QList<Requirement> *requirements);
    bool isValid() const;
    bool operator ==(const Requirement& req) const;

    void updateFromRole(int role, QString text);

    static QStringList validationFromLabels(const QStringList &labels);
    static QString reqTypeFromLabels(const QStringList &labels);

    enum Providence
    {
        GitLab,
        Excel,
        ReqIf,
        Edited,
        Created,
        Deleted
    };

    enum Providence m_providence;

private:
    QString parseReqIfId(const gitlab::Issue &issue);
    QString parseTextField(const gitlab::Issue &issue, const QString field);
    QStringList parseTextList(const gitlab::Issue &issue, const QString field);
    QStringList removeInternalRefs(const QStringList reqIfIDs);
//    QString parseDescription(const gitlab::Issue &issue);
//    QString parseNote(const gitlab::Issue &issue);
};

} // namespace requirement

QDebug operator<<(QDebug debug, const requirement::Requirement &r);
