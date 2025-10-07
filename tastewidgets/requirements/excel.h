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

#include <QObject>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QMessageBox>

#include "xlsxdocument.h"
#include "xlsxrichstring.h"
#include "xlsxworkbook.h"
#include "xlsxformat.h"

#include "requirementsmodelbase.h"

namespace requirement {
class RequirementsModelBase;

class Excel
{
public:
    Excel(const QString fileName, RequirementsModelBase *model, const enum RequirementsModelBase::modelType type);

    static void createDefault(const QString fileName, const enum RequirementsModelBase::modelType type);

    void exportExcel();
    void importExcel();

private:
    void parseExcel();

    QString m_fileName;
    QXlsx::Document m_xlsx;
    RequirementsModelBase *m_model;
    enum RequirementsModelBase::modelType m_modelType;
    int m_rowCount;
    bool m_error;


    const QStringList xlsxSSSHeaders = {
        "Requirement\nIdentifier",
        "Requirement\nType",
        "Title",
        "Details",
//        "Children",
        "Justification",
        "Note",
        "Status",
        "Validation\nApproach",
        "Validation\nVersion",
        "Validation\nDescription",
        "Validation\nStatus",
        "Validation\nEvidence",
        "Compliance",
        "Compliance\nStatus"};

    const QStringList xlsxSRSHeaders = {
        "Requirement\nIdentifier",
        "Requirement\nType",
        "Title",
        "Details",
        "Parents",
//        "Children",
        "Justification",
        "Priority",
        "Note",
        "Status",
        "Validation\nApproach",
        "Validation\nDescription",
        "Validation\nStatus",
        "Validation\nEvidence",
        "Compliance",
        "Compliance\nStatus"};

    const double xlsxSSSColumnWidths[14] = {
        25.0,
        20.0,
        35.0,
        60.0,
//        25.0,
        40.0,
        40.0,
        16.0,
        16.0,
        20.0,
        40.0,
        16.0,
        40.0,
        16.0,
        16.0
    };

    const double xlsxSRSColumnWidths[15] = {
        25.0,
        20.0,
        35.0,
        60.0,
        25.0,
//        25.0,
        40.0,
        16.0,
        40.0,
        16.0,
        16.0,
        40.0,
        16.0,
        40.0,
        16.0,
        16.0
    };
};

}
