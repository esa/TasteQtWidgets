/*
   Copyright (C) 2023 European Space Agency - <maxime.perrotin@esa.int>

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

#include "issue.h"
#include "label.h"
#include "requirement.h"

#include <QObject>

namespace requirement {

const static QString k_requirementsTypeLabel = "requirement";

class GitLabRequirements : public QObject
{
    Q_OBJECT
public:
    void listOfIssues(const QList<gitlab::Issue> &issues);

    static QStringList tagsFromLabels(const QList<gitlab::Label> &labels);

Q_SIGNALS:
    void listOfRequirements(const QList<Requirement> &);
};

}
