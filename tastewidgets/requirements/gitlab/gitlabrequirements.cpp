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

#include "gitlabrequirements.h"

#include <issue.h>

using namespace gitlab;

namespace requirement {

void GitLabRequirements::listOfIssues(const QList<gitlab::Issue> &issues)
{
    QList<Requirement> requirements;
    requirements.reserve(issues.size());
    for (auto &issue : issues) {
        requirements.append(Requirement(issue));
    }
    Q_EMIT listOfRequirements(requirements);
}

/*!
 * Returns the tags from the list of labels provided by the gitlab server
 */
QStringList GitLabRequirements::tagsFromLabels(const QList<gitlab::Label> &labels)
{
    QStringList tags;
    for (const gitlab::Label &label : labels) {
        if (label.mName != k_requirementsTypeLabel) {
            tags.append(label.mName);
        }
    }
    return tags;
}
} // namespace requirement
