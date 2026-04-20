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

#include <QString>
#include <QFile>
#include <QList>
#include <QPair>
#include <QFileInfo>
#include <QTextStream>
#include <QRandomGenerator>
#include <QtXml/QDomDocument>

#include "requirementsmodelbase.h"


namespace requirement {

class ReqIf
{
public:
    ReqIf(const QString fileName, RequirementsModelBase *model, const enum RequirementsModelBase::modelType type = RequirementsModelBase::SRS);

    void createReqIf();
    void readReqIfFile();
    void writeReqIfFile();
    bool importReqIf();
    static const qsizetype MAX_VALID_REQIFID_SIZE = 40;
private:
    enum entryType{
        String,
        State,
        List
    };

    typedef struct {
        entryType type;
        QString id;
        QString name;
        QString dataType;
        RequirementsModelBase::RoleNames role;
        QList<QPair<QString, QString>> states;
    }specAttrType;

    QDomElement createEnumeration(const QString identifier, const QString name, QList<QPair<QString, QString>> tupleList);
    QDomElement createSpecObjectType(const QString identifier, QString longName, QString description, const QList<specAttrType> attrList);
    QDomElement createSpecObjects(const QString identifier, QList<specAttrType> attrList);
    QDomElement createSpecification(const QString longName, const QString description);
    QDomElement makeTextNode(const QString tag, const QString node, const QString text);
    void findParents(QDomElement *element);
    void findChildren(QDomElement *element, QString childId, const QStringList parents);
    QDomElement createAttributeString(const QString stringRef, const QString string);
    QDomElement createAttributeEnum(const QString enumRef, const QString valueRef);
    QString tagFromType(const int type){return (type == String) ? "STRING" : "ENUMERATION";};
    QString multiFromType(const int type){return (type == List) ? "true" : "false";};
    QString randIdGenerator(QString prefix);
    QString enumRefFromName(QString name, QList<QPair<QString, QString>> tupleList);
    QString enumNameFromRef(QString name, QList<QPair<QString, QString>> tupleList);

    void reqFromSpec(const QDomElement &spec, Requirement *req);
    QString refFromTag(const QDomElement element, QString tag);
    void updateReqText(Requirement *req, QString textRef, QString text);
    void updateReqEnum(Requirement *req, QString enumRef, QString valueRef);

    QString m_fileName;
    QDomDocument m_document;
    QString m_timeStamp;
    RequirementsModelBase *m_model;
    enum RequirementsModelBase::modelType m_modelType;
    int m_rowCount;
    bool m_error;


    QList<QPair<QString, QString>> statusList {
        {"STATUS-NOT-STARTED", "Not Started"},
        {"STATUS-IN-WORK", "In Work"},
        {"STATUS-UNDER-REVIEW", "Under Review"},
        {"STATUS-APPROVED", "Approved"}
    };

    QList<QPair<QString, QString>> priorityList {
        {"PRIORITY-UNASSIGNED", "Unassigned"},
        {"PRIORITY-LOW", "Low"},
        {"PRIORITY-MEDIUM", "Medium"},
        {"PRIORITY-HIGH", "High"},
        {"PRIORITY-URGENT", "Urgent"}
    };

    QList<QPair<QString, QString>> complianceList {
        {"COMPLIANCE-NOT-COMPLIANT", "Not Compliant"},
        {"COMPLIANCE-PARTIALLY-COMPLIANT", "Partially Compliant"},
        {"COMPLIANCE-COMPLIANT", "Compliant"}
    };

    QList<QPair<QString, QString>> typeList = {
        {"TYPE-FUNCTIONAL", "Functional"},
        {"TYPE-PERFORMANCE", "Performance"},
        {"TYPE-INTERFACE", "Interface"},
        {"TYPE-OPERATIONAL", "Operational"},
        {"TYPE-RESOURCES", "Resources"},
        {"TYPE-DESIGN", "Design"},
        {"TYPE-IMPLEMENTATION", "Implementation"},
        {"TYPE-SECURITY", "Security"},
        {"TYPE-PORTABILITY", "Portability"},
        {"TYPE-QUALITY", "Quality"},
        {"TYPE-RELIABILITY", "Reliability"},
        {"TYPE-MAINTAINABILITY", "Maintainability"},
        {"TYPE-SAFETY", "Safety"},
        {"TYPE-CONFIGURATION", "Configuration"},
        {"TYPE-INSTALLATION", "Installation"},
        {"TYPE-DATA-DEFINITION", "Data Definition"},
        {"TYPE-HUMAN-FACTORS", "Human Factors"},
        {"TYPE-ADAPTION", "Adaption"}
    };

    QList<specAttrType> srsAttributes{
        {String, "SRS-REQ-REQIFID-TXT", "Requirement Identifier", "STRING-TEXT", RequirementsModelBase::ReqIfIdRole, },
        {State, "SRS-REQ-TYPE-ENUM", "Requirement Type", "ENUM-TYPE", RequirementsModelBase::TypeRole, typeList},
        {String, "SRS-REQ-TITLE-TXT", "Title", "STRING-TEXT", RequirementsModelBase::TitleRole, },
        {String, "SRS-REQ-DESC-TXT", "Requirement", "STRING-TEXT", RequirementsModelBase::DetailDescriptionRole, },
        {State, "SRS-REQ-PRIORITY-ENUM", "Priority", "ENUM-PRIORITY", RequirementsModelBase::PriorityRole, priorityList},
        {State, "SRS-REQ-STATUS-ENUM", "Requirement Status", "ENUM-STATUS", RequirementsModelBase::StatusRole, statusList},
        {String, "SRS-REF-JUST-TXT", "Rationale/Justification", "STRING-TEXT", RequirementsModelBase::JustificationRole, },
        {String, "SRS-VAL-TXT", "Validation Method", "STRING-TEXT", RequirementsModelBase::ValidationRole, },
        {String, "SRS-VAL-DESC-TXT", "Validation Description", "STRING-TEXT", RequirementsModelBase::ValDescriptionRole, },
        {State, "SRS-VAL-STATUS-ENUM", "Validation Status", "ENUM-STATUS", RequirementsModelBase::ValStatusRole, statusList},
        {String, "SRS-VAL-EVID-TXT", "Validation Evidence", "STRING-TEXT", RequirementsModelBase::ValEvidenceRole, },
        {State, "SRS-COMP-ENUM", "Compliance", "ENUM-COMPLIANCE", RequirementsModelBase::ComplianceRole, complianceList},
        {State, "SRS-COMP-STATUS-ENUM", "Compliance Status", "ENUM-STATUS", RequirementsModelBase::ComplianceStatusRole, statusList},
        {String, "SRS-NOTE-TXT", "Note", "STRING-TEXT", RequirementsModelBase::NoteRole, }
    };

    QList<specAttrType> sssAttributes{
        {String, "SSS-REQ-REQIFID-TXT", "Requirement Identifier", "STRING-TEXT", RequirementsModelBase::ReqIfIdRole, },
        {State, "SSS-REQ-TYPE-ENUM", "Requirement Type", "ENUM-TYPE", RequirementsModelBase::TypeRole, typeList},
        {String, "SSS-REQ-TITLE-TXT", "Title", "STRING-TEXT", RequirementsModelBase::TitleRole, },
        {String, "SSS-REQ-DESC-TXT", "Requirement", "STRING-TEXT", RequirementsModelBase::DetailDescriptionRole, },
        {State, "SSS-REQ-STATUS-ENUM", "Requirement Status", "ENUM-STATUS", RequirementsModelBase::StatusRole, statusList},
        {String, "SSS-REF-JUST-TXT", "Rationale/Justification", "STRING-TEXT", RequirementsModelBase::JustificationRole, },
        {String, "SSS-VAL-TXT", "Validation Method", "STRING-TEXT", RequirementsModelBase::ValidationRole, },
        {String, "SSS-VAL-VER-TXT", "Validation Version", "STRING-TEXT", RequirementsModelBase::ValVersionRole, },
        {String, "SSS-VAL-DESC-TXT", "Validation Description", "STRING-TEXT", RequirementsModelBase::ValDescriptionRole, },
        {State, "SSS-VAL-STATUS-ENUM", "Validation Status", "ENUM-STATUS", RequirementsModelBase::ValStatusRole, statusList},
        {String, "SSS-VAL-EVID-TXT", "Validation Evidence", "STRING-TEXT", RequirementsModelBase::ValEvidenceRole, },
        {State, "SSS-COMP-ENUM", "Compliance", "ENUM-COMPLIANCE", RequirementsModelBase::ComplianceRole, complianceList},
        {State, "SSS-COMP-STATUS-ENUM", "Compliance Status", "ENUM-STATUS", RequirementsModelBase::ComplianceStatusRole, statusList},
        {String, "SSS-NOTE-TXT", "Note", "STRING-TEXT", RequirementsModelBase::NoteRole, }
    };

};

}
