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

#include "reqif.h"

namespace requirement {

ReqIf::ReqIf(const QString fileName, RequirementsModelBase *model, const enum RequirementsModelBase::modelType type) :
        m_timeStamp(QDateTime::currentDateTimeUtc().toString(Qt::ISODate)),
        m_fileName(fileName),
        m_model(model),
        m_modelType(type),
        m_error(false)
{
}

void ReqIf::readReqIfFile()
{
    QFile file(m_fileName);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open ReqIf file";
        m_error = true;
    }
    else {
        if (!m_document.setContent(&file)) {
            qDebug() << "Failed to load ReqIf file";
            m_error = true;
        }
    }
}

bool ReqIf::importReqIf()
{
    QDomElement reqif = m_document.firstChildElement();

    if(refFromTag(reqif, "SPECIFICATION-TYPE-REF") == k_SSSLabel) 
    {
        qDebug() << "SSS Detected";
        m_modelType = RequirementsModelBase::SSS;
    } 
    else 
    {
        qDebug() << "SRS Detected";
        m_modelType = RequirementsModelBase::SRS;
    }

    QDomNodeList hierarchy = reqif.elementsByTagName("SPEC-HIERARCHY");

//    qDebug() << "Hierarchy count " << hierarchy.count();
    qDebug() << "DEBUG: Hierarchy count " << hierarchy.count();
    QList<QPair<QString, QStringList>> childMatrix;

    for (int i = 0; i< hierarchy.count(); i++) 
    {
        QDomNode hierarchyNode = hierarchy.at(i);

        QDomElement object = hierarchyNode.firstChildElement("OBJECT");
        QDomElement children = hierarchyNode.firstChildElement("CHILDREN");

        QString ReqIfId = refFromTag(object, "SPEC-OBJECT-REF");

        QStringList childIds;

        for (QDomNode child = children.firstChildElement("SPEC-HIERARCHY"); !child.isNull(); child = child.nextSibling()) 
        {
            QDomElement childObject = child.firstChildElement("OBJECT");
            childIds << refFromTag(childObject, "SPEC-OBJECT-REF");
        }
        qDebug() << " ChildIds List " << childIds;
        
        QPair<QString, QStringList> reqIfIdChildren;

        reqIfIdChildren.first = ReqIfId;
        reqIfIdChildren.second = childIds;

        childMatrix << reqIfIdChildren;
    }

    QDomNodeList specs = reqif.elementsByTagName("SPEC-OBJECT");
    qDebug() << "* SPEC-OBJECTS found = " << specs.count();
    QList<Requirement> reqList;

    for (int i = 0; i< specs.count(); i++) 
    {
        QDomNode specNode = specs.at(i);

        if(specNode.isElement()) 
        {
            qDebug() << "** Element " << i;
            QDomElement specification = specNode.toElement();

            QString reqIfId = specification.attribute("IDENTIFIER");

            if(reqIfId.size() < 25) 
            {
                Requirement requirement;
                reqFromSpec(specification, &requirement);
                qDebug() << "*** requirement " << requirement.m_id;
                if (m_modelType == RequirementsModelBase::SSS) 
                {
                    requirement.m_reqType = QString(k_SSSLabel);
                }
                else 
                {
                    requirement.m_reqType = QString(k_SRSLabel);
                }

                for (int i = 0; i < childMatrix.count(); i++) 
                {
                    QPair<QString, QStringList> entry = childMatrix.at(i);
                    qDebug() << "**** entry " << entry.first;
                    
                    if (entry.first.compare(requirement.m_id) == 0) 
                    {
                        qDebug() << "***** children " << entry.second;
                        requirement.m_children << entry.second;
                    }
                }

                requirement.m_children.removeDuplicates();

                qDebug() << "Add requirement " << requirement.m_id << " to reqList";
                reqList << requirement;
            }
            else 
            {
                qDebug() << "Invalid ReqIf " << reqIfId;
            }
        }
    }

    for (Requirement &parent : reqList) 
    {
        if (parent.m_children.count()) 
        {
            for (QString &childId : parent.m_children) 
            {
                for (Requirement &child : reqList) 
                {
                    if (child.m_id.compare(childId) == 0) 
                    {
                        child.m_parents << parent.m_id;
                    }
                }
            }
        }
    }

    qDebug() << "Check Requirements:";
    for (Requirement &requirement : reqList) 
    {
        qDebug() << "req = " << requirement.m_id;
        QDomNodeList relations = reqif.elementsByTagName("SPEC-RELATION");
        for (int i = 0; i< relations.count(); i++) 
        {
            QDomNode relNode = relations.at(i);

            if(relNode.isElement()) 
            {
                QDomElement relation = relNode.toElement();
     
                QDomNodeList sourceElements = relation.elementsByTagName("SOURCE");
   
                QDomNode sourceNode = sourceElements.at(0);
                QDomElement sourceElement = sourceNode.toElement();
                
                if(requirement.m_id.compare(sourceElement.text()) == 0)
                {
                    QDomNodeList targetElements = relation.elementsByTagName("TARGET");
                    QDomNode targetNode = targetElements.at(0);
                    QDomElement targetElement = targetNode.toElement();
                    qDebug() << " parent =" << targetElement.text();
                    requirement.m_parents << targetElement.text();
               }
           }
        }
    }
    qDebug() << "Requirements Checked";
    for (Requirement &requirement : reqList) 
    {
        requirement.updateIssue(m_model->getRequirements());
    }
    m_model->changeModelState(m_modelType);
    m_model->clearRequirements();
    m_model->addRequirements(reqList);
    m_model->syncRequirements();
   
    return false;
}

QString ReqIf::refFromTag(const QDomElement element, QString tag)
{
    QString ref;
    QDomNodeList refs = element.elementsByTagName(tag);

    if (refs.count()) {
        QDomNode refNode = refs.at(0);

        if(refNode.isElement()) {
            QDomElement refElement = refNode.toElement();
            ref = QString("%1").arg(refElement.firstChild().toText().data());
        }
    }

    return ref;
}

void ReqIf::updateReqText(Requirement *req, QString textRef, QString text)
{
    QList<specAttrType> attributes;

    if (m_modelType == RequirementsModelBase::SSS) {
        attributes = sssAttributes;
    } else {
        attributes = srsAttributes;
    }


    for(specAttrType attribute: attributes) {
        if (attribute.id.compare(textRef) == 0) {
            req->updateFromRole(attribute.role, text);
            return;
        }
    }
}

void ReqIf::updateReqEnum(Requirement *req, QString enumRef, QString valueRef)
{
    QList<specAttrType> attributes;

    if (m_modelType == RequirementsModelBase::SSS) {
        attributes = sssAttributes;
    } else {
        attributes = srsAttributes;
    }
    for(specAttrType attribute: attributes) {
        if (attribute.id.compare(enumRef) == 0) {
            QString text = enumNameFromRef(valueRef, attribute.states);
            req->updateFromRole(attribute.role, text);
            return;
        }
    }
}

void ReqIf::reqFromSpec(const QDomElement &spec, Requirement *req)
{
    req->m_id = spec.attribute("IDENTIFIER");
    QDomNodeList textElements = spec.elementsByTagName("ATTRIBUTE-VALUE-STRING");

    for (int i = 0; i< textElements.count(); i++) {
        QDomNode textNode = textElements.at(i);

        if(textNode.isElement()) {
            QDomElement textElement = textNode.toElement();
            QString text(textElement.attribute("THE-VALUE"));
            QString ref(refFromTag(textElement, "ATTRIBUTE-DEFINITION-STRING-REF"));
            updateReqText(req, ref, text);
            qDebug() << req->m_id << ref << text;
        }
    }

    QDomNodeList enumElements = spec.elementsByTagName("ATTRIBUTE-VALUE-ENUMERATION");

    for (int i = 0; i< enumElements.count(); i++) {
        QDomNode enumNode = enumElements.at(i);

        if(enumNode.isElement()) {
            QDomElement enumElement = enumNode.toElement();
            QString enumerationRef(refFromTag(enumElement, "ATTRIBUTE-DEFINITION-ENUMERATION-REF"));
            QString valueRef(refFromTag(enumElement, "ENUM-VALUE-REF"));
            updateReqEnum(req, enumerationRef, valueRef);
        }
    }
}

void ReqIf::writeReqIfFile()
{
    if (!m_fileName.isEmpty()) {
        QFileInfo fileinfo(m_fileName);
        QString suffix = fileinfo.completeSuffix();

        if (suffix.isEmpty()) {
            m_fileName.append(".reqif");
        }

        QFile reqIfFile(m_fileName);
        if (!reqIfFile.open(QFile::WriteOnly | QFile::Text )) {
            qDebug() << "ReqIf file already opened or there is another issue";
            reqIfFile.close();
        }

        QTextStream reqIfContent(&reqIfFile);

        reqIfContent << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        reqIfContent << m_document.toString();

        reqIfFile.close();
    }
}

/*!
 * \brief ReqIf random Id generator based on 128bits (2x64) random bits
 * formated in bit pattern 32-16-16-16-48 an converted to QString hex (base 16).
 */

QString ReqIf::randIdGenerator(QString prefix)
{
    quint64 upper = QRandomGenerator::global()->generate64();
    quint64 lower = QRandomGenerator::global()->generate64();

    auto r3 = upper & 0xFFFF;
    auto r2 = (upper >> 16) & 0xFFFF;
    auto r1 = upper >> 32;
    auto r5 = lower & 0xFFFFFFFFFFFF;
    auto r4 = lower >> 48;

    QString s1 = QString().number(r1, 16);
    QString s2 = QString().number(r2, 16);
    QString s3 = QString().number(r3, 16);
    QString s4 = QString().number(r4, 16);
    QString s5 = QString().number(r5, 16);

    return prefix + QString("%1-%2-%3-%4-%5").arg(s1).arg(s2).arg(s3).arg(s4).arg(s5);
}

QDomElement ReqIf::createEnumeration(const QString identifier, QString name, QList<QPair<QString, QString>> tupleList)
{
    QDomElement enumDef = m_document.createElement("DATATYPE-DEFINITION-ENUMERATION");
    enumDef.setAttribute("IDENTIFIER", identifier);
    enumDef.setAttribute("LONG-NAME", name);
    enumDef.setAttribute("LAST-CHANGE", m_timeStamp);

    QDomElement specifiedValues = m_document.createElement("SPECIFIED-VALUES");

    int index = 0;

    for(QPair<QString, QString> entry: tupleList) {
        QDomElement enumValue = m_document.createElement("ENUM-VALUE");
        enumValue.setAttribute("IDENTIFIER", entry.first);
        enumValue.setAttribute("LONG-NAME", entry.second);
        enumValue.setAttribute("LAST-CHANGE", m_timeStamp);

        QDomElement properties = m_document.createElement("PROPERTIES");
        QDomElement embeddedValue = m_document.createElement("EMBEDDED-VALUE");
        embeddedValue.setAttribute("KEY", QString("%1").arg(++index));
        embeddedValue.setAttribute("OTHER-CONTENT", entry.second);

        properties.appendChild(embeddedValue);
        enumValue.appendChild(properties);
        specifiedValues.appendChild(enumValue);
    }

    enumDef.appendChild(specifiedValues);

    return enumDef;
}

QString ReqIf::enumRefFromName(QString name, QList<QPair<QString, QString>> tupleList)
{
    for(QPair<QString, QString> entry: tupleList) {
        if (entry.second.compare(name) == 0) {
            return entry.first;
        }
    }
    qDebug() << "Name Not Found - " << name;

    return QString();
}

QString ReqIf::enumNameFromRef(QString ref, QList<QPair<QString, QString>> tupleList)
{
    for(QPair<QString, QString> entry: tupleList) {
        if (entry.first.compare(ref) == 0) {
            return entry.second;
        }
    }
    qDebug() << "Ref Not Found - " << ref;

    return QString();
}


QDomElement ReqIf::createSpecObjectType(const QString identifier, QString longName, QString Description, const QList<specAttrType> attrList)
{
    QDomElement specObjType = m_document.createElement("SPEC-OBJECT-TYPE");
    specObjType.setAttribute("LONG-NAME", longName);
    specObjType.setAttribute("DESC", Description);
    specObjType.setAttribute("IDENTIFIER", identifier);
    specObjType.setAttribute("LAST-CHANGE", m_timeStamp);

    QDomElement specAttributes = m_document.createElement("SPEC-ATTRIBUTES");

    for(specAttrType attribute: attrList) {
        QDomElement attributeDef = m_document.createElement(QString("ATTRIBUTE-DEFINITION-%1").arg(tagFromType(attribute.type)));
        attributeDef.setAttribute("IDENTIFIER", attribute.id);
        attributeDef.setAttribute("LONG-NAME", attribute.name);
        attributeDef.setAttribute("LAST-CHANGE", m_timeStamp);
        if (attribute.type != String) attributeDef.setAttribute("MULTI-VALUED", multiFromType(attribute.type));

        QDomElement type = m_document.createElement("TYPE");
        QDomElement typeDefinition = m_document.createElement(QString("DATATYPE-DEFINITION-%1-REF").arg(tagFromType(attribute.type)));
        QDomText typeText = m_document.createTextNode(attribute.dataType);

        typeDefinition.appendChild(typeText);
        type.appendChild(typeDefinition);
        attributeDef.appendChild(type);
        specAttributes.appendChild(attributeDef);

    }

    specObjType.appendChild(specAttributes);

    return specObjType;
}

QDomElement ReqIf::createAttributeString(const QString stringRef, const QString string)
{
    QDomElement attributeString = m_document.createElement("ATTRIBUTE-VALUE-STRING");
    QDomElement definition = m_document.createElement("DEFINITION");
    QDomElement attributeStringRef = m_document.createElement("ATTRIBUTE-VALUE-STRING-REF");

    QDomText stringRefText = m_document.createTextNode(stringRef);

    attributeString.setAttribute("THE-VALUE", string);

    attributeStringRef.appendChild(stringRefText);
    definition.appendChild(attributeStringRef);
    attributeString.appendChild(definition);

    return attributeString;
}

QDomElement ReqIf::createAttributeEnum(const QString enumRef, const QString valueRef)
{
    QDomElement attributeEnum = m_document.createElement("ATTRIBUTE-VALUE-ENUMERATION");
    QDomElement definition = m_document.createElement("DEFINITION");
    QDomElement attributeEnumRef = m_document.createElement("ATTRIBUTE-VALUE-ENUMERATION-REF");
    QDomElement values = m_document.createElement("VALUES");
    QDomElement enumValueRef = m_document.createElement("ENUM-VALUE-REF");

    QDomText enumRefText = m_document.createTextNode(enumRef);
    QDomText valueRefText = m_document.createTextNode(valueRef);

    attributeEnumRef.appendChild(enumRefText);
    definition.appendChild(attributeEnumRef);
    attributeEnum.appendChild(definition);

    enumValueRef.appendChild(valueRefText);
    values.appendChild(enumValueRef);
    attributeEnum.appendChild(values);

    return attributeEnum;
}


QDomElement ReqIf::createSpecObjects(const QString identifier, QList<specAttrType> attrList)
{
    QDomElement specObjects = m_document.createElement("SPEC-OBJECTS");

    int rowCount = m_model->rowCount(QModelIndex());

    if (rowCount) {
        for(int r = 0; r < rowCount; ++r) {
            QModelIndex index = m_model->index(r, 0);

            QStringList labels = m_model->data(index, RequirementsModelCommon::TagsRole).toStringList();

            if ((m_modelType == RequirementsModelBase::SSS)
            && (labels.contains(k_SRSLabel))) {
                continue;
            }

            if ((m_modelType == RequirementsModelBase::SRS)
            && (labels.contains(k_SSSLabel))) {
                continue;
            }

            QDomElement specObject = m_document.createElement("SPEC-OBJECT");
            specObject.setAttribute("IDENTIFIER", m_model->data(index, RequirementsModelCommon::ReqIfIdRole).toString());
            specObject.setAttribute("LONG-NAME", m_model->data(index, RequirementsModelCommon::TitleRole).toString());
            specObject.setAttribute("DESC", m_model->data(index, RequirementsModelCommon::DetailDescriptionRole).toString());
            specObject.setAttribute("LAST-CHANGE", m_timeStamp);

            QDomElement type = m_document.createElement("TYPE");
            QDomElement typeRef = m_document.createElement("SPEC-OBJECT-TYPE-REF");
            QDomText specObjectType = m_document.createTextNode(identifier);

            typeRef.appendChild(specObjectType);
            type.appendChild(typeRef);

            QDomElement values = m_document.createElement("VALUES");

            for(specAttrType attribute: attrList) {
                QDomElement attributeValue = m_document.createElement(QString("ATTRIBUTE-VALUE-%1").arg(tagFromType(attribute.type)));
                if (attribute.type == String) {
                    attributeValue.setAttribute("THE-VALUE", m_model->data(index, attribute.role).toString());
                }

                QDomElement definition = m_document.createElement("DEFINITION");
                QDomElement attributeDefinition = m_document.createElement(QString("ATTRIBUTE-DEFINITION-%1-REF").arg(tagFromType(attribute.type)));

                QDomText attributeRef = m_document.createTextNode(attribute.id);

                attributeDefinition.appendChild(attributeRef);
                definition.appendChild(attributeDefinition);
                attributeValue.appendChild(definition);

                if (attribute.type == State) {
                    QDomElement vals = m_document.createElement("VALUES");
                    QDomElement valRef = m_document.createElement("ENUM-VALUE-REF");

                    QDomText valRefText = m_document.createTextNode(enumRefFromName(m_model->data(index, attribute.role).toString(), attribute.states));

                    valRef.appendChild(valRefText);
                    vals.appendChild(valRef);
                    attributeValue.appendChild(vals);
                }

                values.appendChild(attributeValue);
            }

            specObject.appendChild(type);
            specObject.appendChild(values);
            specObjects.appendChild(specObject);
        }
    }

    return specObjects;
}

QDomElement ReqIf::makeTextNode(const QString tag, const QString node, const QString text)
{
    QDomElement tagElement = m_document.createElement(tag);
    QDomElement nodeElement = m_document.createElement(node);
    QDomText textElement = m_document.createTextNode(text);
    nodeElement.appendChild(textElement);
    tagElement.appendChild(nodeElement);

    return tagElement;
}

void ReqIf::findParents(QDomElement *element)
{
    int rowCount = m_model->rowCount(QModelIndex());

    if (rowCount) {
        for(int r = 0; r < rowCount; ++r) {
            QModelIndex index = m_model->index(r, 0);
            Requirement requirement = m_model->requirementFromIndex(index);
            if (requirement.m_id.isEmpty()) {
                qDebug() << "Empty requirement found";
            }
            else {
                for (QString &parent : requirement.m_parents) {
                    QDomElement specRelation = m_document.createElement("SPEC-RELATION");
                    specRelation.setAttribute("LONG-NAME", "Link to parent");
                    specRelation.setAttribute("DESC", "Link to parent requirement.");
                    specRelation.setAttribute("IDENTIFIER", randIdGenerator("_"));
                    specRelation.setAttribute("LAST-CHANGE", m_timeStamp);

                    specRelation.appendChild(makeTextNode("SOURCE", "SPEC-OBJECT-REF", requirement.m_id));
                    specRelation.appendChild(makeTextNode("TARGET", "SPEC-OBJECT-REF", parent));
                    specRelation.appendChild(makeTextNode("TYPE", "SPEC-RELATION-TYPE-REF", "parentReq"));

                    element->appendChild(specRelation);
                }
            }
        }
    }
}

void ReqIf::findChildren(QDomElement *element, QString childId, const QStringList parents)
{
    if (!m_model->reqIfIDExists(childId)) {
        return;
    }

    if (parents.contains(childId)) {
        return;
    }

    QDomElement hierarchy = m_document.createElement("SPEC-HIERARCHY");
    QDomElement object = m_document.createElement("OBJECT");
    QDomElement specObjectRef = m_document.createElement("SPEC-OBJECT-REF");

    QDomText refText = m_document.createTextNode(childId);
    specObjectRef.appendChild(refText);

    hierarchy.setAttribute("IDENTIFIER", randIdGenerator("rmf-"));
    hierarchy.setAttribute("LAST-CHANGE", m_timeStamp);

    object.appendChild(specObjectRef);
    hierarchy.appendChild(object);

    Requirement requirement = m_model->requirementFromId(childId);

    if (m_modelType == RequirementsModelBase::SRS) {
        if (requirement.m_children.count()) {
            QDomElement children = m_document.createElement("CHILDREN");

            for (const QString child : requirement.m_children) {
                QStringList grandparents(parents);
                grandparents << childId;
                findChildren(&children, child, grandparents);
            }
            hierarchy.appendChild(children);
        }
    }

    element->appendChild(hierarchy);
}

QDomElement ReqIf::createSpecification(const QString longName, const QString description)
{
    QDomElement specSRSType = m_document.createElement("SPECIFICATION");
    specSRSType.setAttribute("LONG-NAME", longName);
    specSRSType.setAttribute("DESC", description);
    specSRSType.setAttribute("IDENTIFIER", randIdGenerator("_"));
    specSRSType.setAttribute("LAST-CHANGE", m_timeStamp);

    QString specTypeId;
    if (m_modelType == RequirementsModelBase::SSS) {
        specTypeId = QString(k_SSSLabel);
    } else {
        specTypeId = QString(k_SRSLabel);
    }
    QDomElement type = m_document.createElement("TYPE");
    QDomElement typeRef = m_document.createElement("SPECIFICATION-TYPE-REF");
    QDomText specObjectType = m_document.createTextNode(specTypeId);
    typeRef.appendChild(specObjectType);
    type.appendChild(typeRef);
    specSRSType.appendChild(type);

    int rowCount = m_model->rowCount(QModelIndex());

    qDebug() << "ReqIf create Specification rows " << rowCount;

    if (rowCount) {
        for(int r = 0; r < rowCount; ++r) {
            QModelIndex index = m_model->index(r, 0);
            Requirement requirement = m_model->requirementFromIndex(index);

            if (requirement.m_id.isEmpty()) {
                qDebug() << "Empty req found";
            } else if ((m_modelType == RequirementsModelBase::SSS)
                && (requirement.m_reqType.compare(k_SRSLabel)==0)) {
                    continue;
            } else if ((m_modelType == RequirementsModelBase::SRS)
                && (requirement.m_reqType.compare(k_SSSLabel)==0)) {
                    continue;
            } else {
                QStringList parents;
                if (requirement.m_parents.isEmpty()) {
                    qDebug() << "Reqif child created";
                    QDomElement children = m_document.createElement("CHILDREN");
                    findChildren(&children, requirement.m_id, parents);
                    specSRSType.appendChild(children);
                } else {
                    if (m_modelType == RequirementsModelBase::SRS) {
                        bool exists = false;
                        qDebug() << "Requirement " << requirement.m_id; 
                        for (const QString parent : requirement.m_parents) {
                            for(int i = 0; i < rowCount; ++i) {
                                qDebug() << " parent " << parent;
                                QModelIndex index = m_model->index(i, 0);
                                Requirement requirement = m_model->requirementFromIndex(index);
                                if (parent.compare(requirement.m_id) == 0) {
                                    exists = true;
                                }
                            }
                        }

                        if (!exists) {
                            qDebug() << "Reqif orphan created";
                            QDomElement orphan = m_document.createElement("CHILDREN");
                            findChildren(&orphan, requirement.m_id, parents);
                            specSRSType.appendChild(orphan);
                        }
                    }
                }
            }
        }
    }

    return specSRSType;
}

void ReqIf::createReqIf()
{
    m_document = QDomDocument();

    QDomElement reqIf = m_document.createElement("REQ-IF");
    reqIf.setAttribute("xml:lang", "en");
    reqIf.setAttribute("xmlns:xhtml", "http://www.w3.org/1999/xhtml");
    reqIf.setAttribute("xmlns", "http://www.omg.org/spec/ReqIF/20110401/reqif.xsd");
    m_document.appendChild(reqIf);

    QDomElement header = m_document.createElement("THE-HEADER");

    QDomElement reqifHeader = m_document.createElement("REQ-IF-HEADER");
    reqifHeader.setAttribute("IDENTIFIER", randIdGenerator("rmf-"));

    QDomElement creationTime = m_document.createElement("CREATION-TIME");
    QDomText creationTimeText = m_document.createTextNode(m_timeStamp);
    creationTime.appendChild(creationTimeText);

    QDomElement reqIfTool = m_document.createElement("REQ-IF-TOOL-ID");
    QDomText reqIfToolText = m_document.createTextNode("KISPE Requirements Manager");
    reqIfTool.appendChild(reqIfToolText);

    QDomElement version = m_document.createElement("REQ-IF-VERSION");
    QDomText versionText = m_document.createTextNode("1.0");
    version.appendChild(versionText);

    QDomElement sourceTool = m_document.createElement("SOURCE-TOOL-ID");
    QDomText sourceToolText = m_document.createTextNode("KISPE Requirements Manager");
    sourceTool.appendChild(sourceToolText);

    QDomElement title = m_document.createElement("TITLE");
    QDomText titleText = m_document.createTextNode(m_fileName);
    title.appendChild(titleText);

    reqifHeader.appendChild(creationTime);
    reqifHeader.appendChild(reqIfTool);
    reqifHeader.appendChild(version);
    reqifHeader.appendChild(sourceTool);
    reqifHeader.appendChild(title);

    header.appendChild(reqifHeader);

    QDomElement core = m_document.createElement("CORE-CONTENT");
    QDomElement reqifContent = m_document.createElement("REQ-IF-CONTENT");

    QDomElement dataTypes = m_document.createElement("DATATYPES");

    QDomElement textType = m_document.createElement("DATATYPE-DEFINITION-STRING");
    textType.setAttribute("IDENTIFIER", "STRING-TEXT");
    textType.setAttribute("LONG-NAME", "Text");
    textType.setAttribute("MAX-LENGTH", "10000");
    textType.setAttribute("LAST-CHANGE", m_timeStamp);

    QDomElement typeEnum = createEnumeration("ENUM-TYPE", "Type", typeList);
    QDomElement priorityEnum = createEnumeration("ENUM-PRIORITY", "Priority", priorityList);
    QDomElement statusEnum = createEnumeration("ENUM-STATUS", "Status", statusList);
    QDomElement complianceEnum = createEnumeration("ENUM-COMPLIANCE", "Compliance", complianceList);

    dataTypes.appendChild(textType);
    dataTypes.appendChild(typeEnum);
    dataTypes.appendChild(priorityEnum);
    dataTypes.appendChild(statusEnum);
    dataTypes.appendChild(complianceEnum);

    reqifContent.appendChild(dataTypes);

    QDomElement specTypes = m_document.createElement("SPEC-TYPES");

    QDomElement specificationType = m_document.createElement("SPECIFICATION-TYPE");

    if (m_modelType == RequirementsModelBase::SSS) {
        specificationType.setAttribute("DESC", "A Software System Specification contains the high level system requirements for the system development.");
        specificationType.setAttribute("LONG-NAME", "Software System Specification");
        specificationType.setAttribute("IDENTIFIER", k_SSSLabel);
    } else {
        specificationType.setAttribute("DESC", "A Software Requirements Specification contains a set of precise technical requirements for the software development.");
        specificationType.setAttribute("LONG-NAME", "Software Requirements Specification");
        specificationType.setAttribute("IDENTIFIER", k_SRSLabel);
    }
    specificationType.setAttribute("LAST-CHANGE", m_timeStamp);

    QDomElement specParentReletionType = m_document.createElement("SPEC-RELATION-TYPE");
    specParentReletionType.setAttribute("DESC","This links to the parent from which this requirement was derived.");
    specParentReletionType.setAttribute("LONG-NAME","Parent Requirement");
    specParentReletionType.setAttribute("IDENTIFIER","parentReq");
    specParentReletionType.setAttribute("LAST-CHANGE",m_timeStamp);
    specTypes.appendChild(specificationType);

    if (m_modelType == RequirementsModelBase::SSS) {
        QDomElement specSSSRequirementType = createSpecObjectType("GITLAB-SSS-REQ-TYPE", "Requirement", "Standard GitLab SSS Requirement", sssAttributes);
        specTypes.appendChild(specSSSRequirementType);
//        specTypes.appendChild(specParentReletionType);
        reqifContent.appendChild(specTypes);
        QDomElement specObjects = createSpecObjects("GITLAB-SSS-REQ-TYPE", sssAttributes);
        reqifContent.appendChild(specObjects);
    } else {
        QDomElement specSRSRequirementType = createSpecObjectType("GITLAB-SRS-REQ-TYPE", "Requirement", "Standard GitLab SRS Requirement", srsAttributes);
        specTypes.appendChild(specSRSRequirementType);
        specTypes.appendChild(specParentReletionType);
        reqifContent.appendChild(specTypes);
        QDomElement specObjects = createSpecObjects("GITLAB-SRS-REQ-TYPE", srsAttributes);
        reqifContent.appendChild(specObjects);
    }

    QDomElement specRelations = m_document.createElement("SPEC-RELATIONS");

    if (m_modelType == RequirementsModelBase::SRS) {
        findParents(&specRelations);
    }

    reqifContent.appendChild(specRelations);


    QString specTitle;
    QString specDescription;

    if (m_modelType == RequirementsModelBase::SSS) {
        specTitle = QString("Software System Specification");
        specDescription = QString("Specify the software system's requirements here.");
    } else {
        specTitle = QString("Software Requirements Specification");
        specDescription = QString("Specify the software's requirements here.");
    }

    QDomElement specifications = m_document.createElement("SPECIFICATIONS");
    QDomElement specification = createSpecification(specTitle, specDescription);
    specifications.appendChild(specification);
    reqifContent.appendChild(specifications);

    core.appendChild(reqifContent);

    reqIf.appendChild(header);
    reqIf.appendChild(core);

}

} // namespace requirement
