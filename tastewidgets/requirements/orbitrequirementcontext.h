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

#pragma once

#include "requirement.h"

#include <QList>
#include <QString>
#include <QTemporaryDir>

#include <memory>

namespace requirement {

/*!
 * Prepares what the Orbit chatbot needs to reason about a requirements set:
 * a working directory, a machine-readable copy of the requirements, and a
 * briefing that carries the ECSS requirements-engineering knowledge the old
 * Virtual Assistant used to get from its `vareq` server.
 *
 * The briefing is sent to the agent ahead of the user's first question, so
 * questions like "find duplicates" or "check consistency" are answered with
 * the actual specification in hand and held to ECSS-E-ST-10-06C.
 */
class OrbitRequirementContext
{
public:
    //: Everything the chat needs: where it works, and what to tell the agent.
    struct Prepared
    {
        QString directory;        //: the working directory handed to Orbit
        QString requirementsFile; //: <directory>/requirements.md, "" on failure
        QString briefing;         //: the text sent before the first question
        //: Owns the temporary working directory, removed when this is dropped.
        std::shared_ptr<QTemporaryDir> workspace;
    };

    //: Writes the requirements into a working directory and composes the briefing.
    //: `specType` is "SRS" or "SSS". When `projectDir` is non-empty it is used as
    //: the working directory (so Orbit can create files in the real project);
    //: otherwise a fresh temp directory is created and removed when the chat closes.
    static Prepared prepare(const QList<Requirement> &requirements, const QString &specType,
            const QString &projectDir = {});

    //: The condensed ECSS-E-ST-10-06C guidance embedded in every briefing.
    static QString ecssGuidance();

    //: The requirements rendered as Markdown, one section each.
    static QString renderRequirements(const QList<Requirement> &requirements);
};

} // namespace requirement
