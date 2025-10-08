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


#include "excel.h"
#include "defaultsss.h"
#include "defaultsrs.h"
#include <iostream>
#include <fstream>
namespace requirement {

Excel::Excel(const QString fileName, RequirementsModelBase *model, const enum RequirementsModelBase::modelType type) :
    m_fileName(fileName),
    m_xlsx(fileName),
    m_model(model),
    m_modelType(type),
    m_rowCount(0),
    m_error(false)
{
    qDebug() << "Excel Constructor ";
    if (!m_xlsx.load()) {
        m_error = true;
        qDebug() << "ERROR - Xlsx load failed";
    }
}

void Excel::createDefault(const QString fileName, const enum RequirementsModelBase::modelType type)
{
    if (type == RequirementsModelBase::SSS) {
        if (!fileName.isEmpty()) {

            qDebug() << fileName;

            std::ofstream fileStream(fileName.toStdString());
            fileStream.write((const char *)defaultSSS, sizeof(defaultSSS));
            fileStream.flush();
            fileStream.close();
        }
    } else {
        if (!fileName.isEmpty()) {

            qDebug() << fileName;

            std::ofstream fileStream(fileName.toStdString());
            fileStream.write((const char *)defaultSRS, sizeof(defaultSRS));
            fileStream.flush();
            fileStream.close();
        }
    }
}

void Excel::parseExcel()
{
    if (!m_error) {
        m_xlsx.selectSheet(6);
        int rowCount = m_xlsx.dimension().rowCount();
        qDebug() << rowCount << " excel rows";

        if (rowCount > 1) {
            QStringList duplicates;
            QStringList checkIds;
            int blank = 0;

            for (int r = 3; r <= rowCount; r++) {
                QString id(m_xlsx.read(r, 1).toString().trimmed());
                if (id.size()) {
                    if (checkIds.contains(id)) {
                        duplicates << id;
                    }
                    else {
                        checkIds << id;
                    }
                    blank = 0;
                }
                else {
                    if(++blank > 6) {
                        rowCount = r - blank;
                    }
                }
            }

            if(duplicates.count()) {
                duplicates.removeDuplicates();

                QMessageBox::warning(nullptr, QObject::tr("Duplicate ReqIfIds Found"),
                        QObject::tr("Please edit the Excel file to remove the following\n duplicated ReqIfId ID(s):\n\n%1 ")
                        .arg(duplicates.join('\n')));

                m_rowCount = 0;
                m_error = true;
            }
            else {
                m_rowCount = rowCount;
            }
        }
        else {
            m_rowCount = 0;
        }
    }
}

void Excel::importExcel()
{
    QList<Requirement> reqList;

    parseExcel();

    if (!m_error) {
        for (int r = 3; r <= m_rowCount; r++) {
            Requirement requirement;

            if (m_modelType == RequirementsModelBase::SSS) {
                requirement.m_reqType = k_SSSLabel;

                requirement.m_id = m_xlsx.read(r, 1).toString().trimmed();
                requirement.m_type = m_xlsx.read(r, 2).toString();
                requirement.m_longName = m_xlsx.read(r, 3).toString();
                requirement.m_description = m_xlsx.read(r, 4).toString();
//              requirement.m_children = m_xlsx.read(r, 5).toString().trimmed().split('\n');
                requirement.m_justification = m_xlsx.read(r, 5).toString();
                requirement.m_note = m_xlsx.read(r, 6).toString();
                requirement.m_status = m_xlsx.read(r, 7).toString();
                requirement.m_validation = RequirementsModelCommon::validationLabelsFromString(m_xlsx.read(r, 8).toString().trimmed());
                requirement.m_valVersion = m_xlsx.read(r, 9).toString();
                requirement.m_valDescription = m_xlsx.read(r, 10).toString();
                requirement.m_valStatus = m_xlsx.read(r, 11).toString();
                requirement.m_valEvidence = m_xlsx.read(r, 12).toString();
                requirement.m_compliance = m_xlsx.read(r, 13).toString();
                requirement.m_complianceStatus = m_xlsx.read(r, 14).toString();
            }
            else {
                requirement.m_reqType = k_SRSLabel;

                requirement.m_id = m_xlsx.read(r, 1).toString().trimmed();
                requirement.m_type = m_xlsx.read(r, 2).toString();
                requirement.m_longName = m_xlsx.read(r, 3).toString();
                requirement.m_description = m_xlsx.read(r, 4).toString();
                requirement.m_parents = m_xlsx.read(r, 5).toString().trimmed().split('\n');
//              requirement.m_children = m_xlsx.read(r, 6).toString().trimmed().split('\n');
                requirement.m_justification = m_xlsx.read(r, 6).toString();
                requirement.m_priority = m_xlsx.read(r, 7).toString();
                requirement.m_note = m_xlsx.read(r, 8).toString();
                requirement.m_status = m_xlsx.read(r, 9).toString();
                requirement.m_validation = RequirementsModelCommon::validationLabelsFromString(m_xlsx.read(r, 10).toString().trimmed());
                requirement.m_valDescription = m_xlsx.read(r, 11).toString();
                requirement.m_valStatus = m_xlsx.read(r, 12).toString();
                requirement.m_valEvidence = m_xlsx.read(r, 13).toString();
                requirement.m_compliance = m_xlsx.read(r, 14).toString();
                requirement.m_complianceStatus = m_xlsx.read(r, 15).toString();
            }

            requirement.updateIssue(m_model->getRequirements());
            reqList << requirement;
        }
        m_model->changeModelState(m_modelType);
        m_model->clearRequirements();
        m_model->addRequirements(reqList);
        m_model->syncRequirements();
    } else {
        qDebug() << "ERROR - Excel import failed";
    }
}


void Excel::exportExcel()
{
    int rowCount = m_model->rowCount(QModelIndex());

    if (rowCount && !m_error) {
        QXlsx::Format formatColumnText;
        QXlsx::Format formatColumnId;
        QXlsx::Format formatHeaders;
        QXlsx::Format formatTopper;

        formatTopper.setVerticalAlignment(QXlsx::Format::VerticalAlignment::AlignVCenter);
        formatTopper.setHorizontalAlignment(QXlsx::Format::HorizontalAlignment::AlignLeft);
        formatTopper.setFontBold(true);
        formatTopper.setFontSize(12);
        QRgb colourTop = 0xFF96DCF8;
        formatTopper.setPatternBackgroundColor(QColor(colourTop));

        formatHeaders.setVerticalAlignment(QXlsx::Format::VerticalAlignment::AlignVCenter);
        formatHeaders.setHorizontalAlignment(QXlsx::Format::HorizontalAlignment::AlignHCenter);
        formatHeaders.setTextWrap(true);
        formatHeaders.setFontBold(true);
        formatHeaders.setBottomBorderStyle(QXlsx::Format::BorderStyle::BorderThick);
        formatHeaders.setTopBorderStyle(QXlsx::Format::BorderStyle::BorderHair);
        formatHeaders.setRightBorderStyle(QXlsx::Format::BorderStyle::BorderHair);
        QRgb colourHead = 0xFFCAEEFB;
        formatHeaders.setPatternBackgroundColor(QColor(colourHead));
        formatHeaders.setFontSize(14);

        formatColumnText.setVerticalAlignment(QXlsx::Format::VerticalAlignment::AlignTop);
        formatColumnText.setHorizontalAlignment(QXlsx::Format::HorizontalAlignment::AlignLeft);
        formatColumnText.setFontSize(11);
        formatColumnText.setTextWrap(true);
        formatColumnText.setIndent(1);


        formatColumnId.setVerticalAlignment(QXlsx::Format::VerticalAlignment::AlignTop);
        formatColumnId.setHorizontalAlignment(QXlsx::Format::HorizontalAlignment::AlignLeft);
        formatColumnId.setTextWrap(true);
        formatColumnId.setIndent(1);
        formatColumnId.setFontBold(true);
        formatColumnId.setFontSize(12);

        if (m_modelType == RequirementsModelBase::SSS) {
            m_xlsx.deleteSheet("5. & 6. Requirements");
            m_xlsx.insertSheet(6, "5. & 6. Requirements", QXlsx::AbstractSheet::ST_WorkSheet);
            m_xlsx.write("A1", "5. & 6. Requirements");
            m_xlsx.mergeCells("A1:N1", formatTopper);

            m_xlsx.setColumnFormat(1, formatColumnId);
            m_xlsx.setColumnFormat(2, 15, formatColumnText);
            m_xlsx.setRowHeight(2, 45.0);

            int column = 0;
            for(QString header: xlsxSSSHeaders) {
                column++;
                m_xlsx.setColumnWidth(column, xlsxSSSColumnWidths[column-1]);
                m_xlsx.write(2,column, header, formatHeaders);
            }

            int line = 3;
            for(int r = 3; r < rowCount + 3; ++r) {
                QModelIndex index = m_model->index(r-3, 0);

                QStringList labels = m_model->data(index, RequirementsModelBase::RoleNames::TagsRole).toStringList();

                if (labels.contains("SSS")) {
                    m_xlsx.write(line, 1, m_model->data(index, RequirementsModelBase::RoleNames::ReqIfIdRole));
                    m_xlsx.write(line, 2, m_model->data(index, RequirementsModelBase::RoleNames::TypeRole));
                    m_xlsx.write(line, 3, m_model->data(index, RequirementsModelBase::RoleNames::TitleRole));
                    m_xlsx.write(line, 4, m_model->data(index, RequirementsModelBase::RoleNames::DetailDescriptionRole));
                    m_xlsx.write(line, 5, m_model->data(index, RequirementsModelBase::RoleNames::JustificationRole));
                    m_xlsx.write(line, 6, m_model->data(index, RequirementsModelBase::RoleNames::NoteRole));
                    m_xlsx.write(line, 7, m_model->data(index, RequirementsModelBase::RoleNames::StatusRole));
                    m_xlsx.write(line, 8, m_model->data(index, RequirementsModelBase::RoleNames::ValidationRole));
                    m_xlsx.write(line, 9, m_model->data(index, RequirementsModelBase::RoleNames::ValVersionRole));
                    m_xlsx.write(line, 10, m_model->data(index, RequirementsModelBase::RoleNames::ValDescriptionRole));
                    m_xlsx.write(line, 11, m_model->data(index, RequirementsModelBase::RoleNames::ValStatusRole));
                    m_xlsx.write(line, 12, m_model->data(index, RequirementsModelBase::RoleNames::ValEvidenceRole));
                    m_xlsx.write(line, 13, m_model->data(index, RequirementsModelBase::RoleNames::ComplianceRole));
                    m_xlsx.write(line, 14, m_model->data(index, RequirementsModelBase::RoleNames::ComplianceStatusRole));
                    line++;
                }
            }
        } else {

            m_xlsx.deleteSheet("5. Requirements");
            m_xlsx.insertSheet(6, "5. Requirements", QXlsx::AbstractSheet::ST_WorkSheet);
            m_xlsx.write("A1", "5. Requirements");
            m_xlsx.mergeCells("A1:O1", formatTopper);

            m_xlsx.setColumnFormat(1, formatColumnId);
            m_xlsx.setColumnFormat(2, 15, formatColumnText);
            m_xlsx.setRowHeight(2, 45.0);

            int column = 0;
            for(QString header: xlsxSRSHeaders) {
                column++;
                m_xlsx.setColumnWidth(column, xlsxSRSColumnWidths[column-1]);
                m_xlsx.write(2,column, header, formatHeaders);
            }

            int line = 3;
            for(int r = 3; r < rowCount + 3; ++r) {
                QModelIndex index = m_model->index(r-3, 0);

                QStringList labels = m_model->data(index, RequirementsModelBase::RoleNames::TagsRole).toStringList();

                if (labels.contains("SRS")) {
                    m_xlsx.write(line, 1, m_model->data(index, RequirementsModelBase::RoleNames::ReqIfIdRole));
                    m_xlsx.write(line, 2, m_model->data(index, RequirementsModelBase::RoleNames::TypeRole));
                    m_xlsx.write(line, 3, m_model->data(index, RequirementsModelBase::RoleNames::TitleRole));
                    m_xlsx.write(line, 4, m_model->data(index, RequirementsModelBase::RoleNames::DetailDescriptionRole));
                    m_xlsx.write(line, 5, m_model->data(index, RequirementsModelBase::RoleNames::ParentsRole));
                    m_xlsx.write(line, 6, m_model->data(index, RequirementsModelBase::RoleNames::JustificationRole));
                    m_xlsx.write(line, 7, m_model->data(index, RequirementsModelBase::RoleNames::PriorityRole));
                    m_xlsx.write(line, 8, m_model->data(index, RequirementsModelBase::RoleNames::NoteRole));
                    m_xlsx.write(line, 9, m_model->data(index, RequirementsModelBase::RoleNames::StatusRole));
                    m_xlsx.write(line, 10, m_model->data(index, RequirementsModelBase::RoleNames::ValidationRole));
                    m_xlsx.write(line, 11, m_model->data(index, RequirementsModelBase::RoleNames::ValDescriptionRole));
                    m_xlsx.write(line, 12, m_model->data(index, RequirementsModelBase::RoleNames::ValStatusRole));
                    m_xlsx.write(line, 13, m_model->data(index, RequirementsModelBase::RoleNames::ValEvidenceRole));
                    m_xlsx.write(line, 14, m_model->data(index, RequirementsModelBase::RoleNames::ComplianceRole));
                    m_xlsx.write(line, 15, m_model->data(index, RequirementsModelBase::RoleNames::ComplianceStatusRole));
                    line++;
                }
            }

            m_xlsx.deleteSheet("6. Validation Matrix");
            m_xlsx.insertSheet(8, "6. Validation Matrix", QXlsx::AbstractSheet::ST_WorkSheet);
            m_xlsx.write("A1", "Validation Matrix");
            m_xlsx.mergeCells("A1:B1", formatTopper);

            m_xlsx.setColumnFormat(1, 2, formatColumnText);
            m_xlsx.setColumnWidth(1, 25.0);
            m_xlsx.setColumnWidth(2, 20.0);
            m_xlsx.write(2, 1, "Requirement Identifier", formatHeaders);
            m_xlsx.write(2, 2, "Validation Approach", formatHeaders);

            line = 3;
            for(int r = 3; r < rowCount + 3; ++r) {
#if 0
                QVariant IdRef = QString("=('5. Requirements\'!A%1)").arg(r);
                QVariant ValidationRef = QString("=(\'5. Requirements\'!J%1)").arg(r);
                m_xlsx.write(r, 1, IdRef, formatColumnText);
                m_xlsx.write(r, 2, ValidationRef, formatColumnText);
#endif

                QModelIndex index = m_model->index(r-3, 0);

                m_xlsx.write(r, 1, m_model->data(index, RequirementsModelBase::RoleNames::ReqIfIdRole));
                m_xlsx.write(r, 2, m_model->data(index, RequirementsModelBase::RoleNames::ValidationRole).toString());

            }

            m_xlsx.deleteSheet("7a. Traceability Matrix");
            m_xlsx.insertSheet(9, "7a. Traceability Matrix", QXlsx::AbstractSheet::ST_WorkSheet);
            m_xlsx.write("A1", "7a. Traceability Matrix (High level requirements to SRS)");
            m_xlsx.mergeCells("A1:B1", formatTopper);

            m_xlsx.setColumnFormat(1, 2, formatColumnText);
            m_xlsx.setColumnWidth(1, 40.0);
            m_xlsx.setColumnWidth(2, 40.0);
            m_xlsx.write(2, 1, "Parent Requirement Identifier", formatHeaders);
            m_xlsx.write(2, 2, "SRS Requirement Identifier", formatHeaders);

            line = 3;
            for(int r = 3; r < rowCount + 3; ++r) {
                QModelIndex index = m_model->index(r-3, 0);

                QString children = m_model->data(index, RequirementsModelBase::RoleNames::ChildrenRole).toString();

                qDebug() << "Children " << children;

                if (!children.isEmpty()) {
                    m_xlsx.write(line, 1, m_model->data(index, RequirementsModelBase::RoleNames::ReqIfIdRole));
                    m_xlsx.write(line, 2, children);
                    line++;
                }
            }

            m_xlsx.deleteSheet("7b. Traceability Matrix");
            m_xlsx.insertSheet(9, "7b. Traceability Matrix", QXlsx::AbstractSheet::ST_WorkSheet);
            m_xlsx.write("A1", "7b. Traceability Matrix (SRS to High level requirements)");
            m_xlsx.mergeCells("A1:B1", formatTopper);

            m_xlsx.setColumnFormat(1, 2, formatColumnText);
            m_xlsx.setColumnWidth(1, 40.0);
            m_xlsx.setColumnWidth(2, 40.0);
            m_xlsx.write(2, 1, "SRS Requirement Identifier", formatHeaders);
            m_xlsx.write(2, 2, "Parent Requirement Identifier", formatHeaders);

            line = 3;
            for(int r = 3; r < rowCount + 3; ++r) {
                QModelIndex index = m_model->index(r-3, 0);

                QString parents = m_model->data(index, RequirementsModelBase::RoleNames::ParentsRole).toString();

                qDebug() << "Parents" << parents;

                if (!parents.isEmpty()) {
                    m_xlsx.write(line, 1, m_model->data(index, RequirementsModelBase::RoleNames::ReqIfIdRole));
                    m_xlsx.write(line, 2, parents);
                    line++;
                }
            }
        }

        m_xlsx.selectSheet(0);
        m_xlsx.saveAs(m_fileName);
    } else {
        qDebug() << "Excel export failed";
    }
}

}
