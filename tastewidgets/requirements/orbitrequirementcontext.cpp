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

#include "orbitrequirementcontext.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

namespace requirement {

QString OrbitRequirementContext::ecssGuidance()
{
    // A condensed reading of ECSS-E-ST-10-06C (the same standard the old
    // Virtual Assistant's `vareq` queries quoted), small enough to sit in every
    // briefing while still saying what compliance means concretely.
    return QStringLiteral(
            "## ECSS-E-ST-10-06C - requirements engineering (condensed)\n"
            "\n"
            "You are an expert requirements engineer in the space industry. Judge every requirement "
            "against the standard below and say which clause a finding comes from.\n"
            "\n"
            "Characteristics each technical requirement should satisfy (SS8.2):\n"
            "- Performance (8.2.1): stated in quantifiable terms; state the method of measurement if "
            "needed to remove ambiguity.\n"
            "- Justification (8.2.2): each requirement should be justified and the responsible entity "
            "identified.\n"
            "- Configuration management and traceability (8.2.3): under configuration management, "
            "backwards- and forward-traceable.\n"
            "- Ambiguity (8.2.4): unambiguous.\n"
            "- Uniqueness (8.2.5): unique - no two requirements say the same thing.\n"
            "- Identifiability (8.2.6): tied to a function/product/system and given a unique "
            "identifier that reflects its type and life-profile situation.\n"
            "- Singularity (8.2.7): separately stated - not a combination of two or more requirements.\n"
            "- Completeness (8.2.8): self-contained; needs no further data to be understood.\n"
            "- Verification (8.2.9): verifiable by an approved method (test, analysis, inspection, "
            "design).\n"
            "- Tolerance (8.2.10): a tolerance is given for every parameter.\n"
            "\n"
            "Wording (SS8.3):\n"
            "- Stated in performance / \"what is necessary\" terms, positive, as a complete sentence "
            "with a verb and a noun.\n"
            "- Verbal form: \"shall\" for a requirement, \"should\" for a recommendation, \"may\" for a "
            "permission, \"can\" for a possibility.\n"
            "- Terms that shall NOT be used (8.3.3): and/or, etc., goal, \"shall be included but not "
            "limited to\", relevant, necessary, appropriate, \"as far as possible\", optimize, "
            "minimize, maximize, typical, rapid, user-friendly, easy, sufficient, enough, suitable, "
            "satisfactory, adequate, quick, \"first rate\", \"best possible\", great, small, large, "
            "\"state of the art\".\n"
            "\n"
            "Requirement types (SS6): functional, mission, interface, environmental, operational, "
            "human factor, (integrated) logistics support, physical, product assurance, "
            "configuration, design, verification.\n");
}

QString OrbitRequirementContext::renderRequirements(const QList<Requirement> &requirements)
{
    const auto field = [](const QString &label, const QString &value) {
        const QString v = value.trimmed();
        return v.isEmpty() ? QString() : QStringLiteral("- %1: %2\n").arg(label, v);
    };

    QString out = QStringLiteral("# Requirements\n\n");
    for (const Requirement &r : requirements) {
        out += QStringLiteral("## %1\n").arg(r.m_id.trimmed().isEmpty() ? QStringLiteral("(no id)") : r.m_id);
        out += field(QStringLiteral("Type"), r.m_type);
        const QString longName = r.m_longName.trimmed();
        const QString description = r.m_description.trimmed();
        if (!longName.isEmpty() && longName != description) {
            out += field(QStringLiteral("Title"), longName);
        }
        if (!description.isEmpty()) {
            out += field(QStringLiteral("Description"), description);
        }
        out += field(QStringLiteral("Note"), r.m_note);
        out += field(QStringLiteral("Justification"), r.m_justification);
        out += field(QStringLiteral("Priority"), r.m_priority);
        out += field(QStringLiteral("Status"), r.m_status);
        // Validation is deliberately not narrowed to one method: the field has
        // held both a single method and a list over time.
        out += field(QStringLiteral("Validation"), r.m_validation.join(QStringLiteral(", ")));
        out += field(QStringLiteral("Verification method"), r.m_valDescription);
        if (!r.m_parents.isEmpty()) {
            out += field(QStringLiteral("Traces (parents)"), r.m_parents.join(QStringLiteral(", ")));
        }
        if (!r.m_children.isEmpty()) {
            out += field(QStringLiteral("Traces (children)"), r.m_children.join(QStringLiteral(", ")));
        }
        out += QLatin1Char('\n');
    }
    return out;
}

OrbitRequirementContext::Prepared OrbitRequirementContext::prepare(
        const QList<Requirement> &requirements, const QString &specType)
{
    Prepared prepared;
    prepared.directory = QDir::currentPath(); // A real directory, never a URL.

    const QString spec = specType.trimmed().isEmpty() ? QStringLiteral("requirements") : specType.trimmed();
    prepared.briefing =
            QStringLiteral(
                    "# Context: you are analysing the %1 requirements of a TASTE project\n"
                    "\n"
                    "You are working in a directory that holds `requirements.md`, a Markdown copy of the "
                    "%1 requirements currently loaded in the requirements editor. Read it before "
                    "answering, and work only from it.\n"
                    "\n"
                    "%2"
                    "\n"
                    "When you are asked to find duplicates, check consistency or completeness, or review "
                    "a requirement, answer with:\n"
                    "- a short verdict first (how many issues, and how serious),\n"
                    "- then a table or list of each finding: requirement id(s), the problem, and the ECSS "
                    "clause it violates,\n"
                    "- then a concrete suggested fix for each.\n"
                    "Be specific and reference requirement ids. Do not invent requirements that are not in "
                    "the file.\n")
                    .arg(spec, ecssGuidance());

    if (requirements.isEmpty()) {
        return prepared;
    }

    // A fresh directory each time, so a previous run's file is never mistaken
    // for the current set. `directive.workspace` removes it when the chat closes.
    auto dir = std::make_shared<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/orbit-requirements-XXXXXX"));
    if (!dir->isValid()) {
        // Fall back to the briefing carrying the requirements inline, so the
        // agent is never left without the specification.
        prepared.directory = QDir::currentPath();
        prepared.briefing += QStringLiteral("\n## The %1 requirements\n\n%2").arg(spec, renderRequirements(requirements));
        return prepared;
    }
    prepared.workspace = dir;
    prepared.directory = dir->path();
    prepared.requirementsFile = QStringLiteral("%1/requirements.md").arg(dir->path());
    QFile file(prepared.requirementsFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream(&file) << renderRequirements(requirements);
        file.close();
        prepared.briefing +=
                QStringLiteral("\nThe file `requirements.md` in your working directory contains all %1 "
                               "requirements (%2 of them).\n")
                        .arg(spec)
                        .arg(requirements.size());
    } else {
        // No file: put the requirements in the briefing itself so the agent is
        // never left without the specification.
        prepared.requirementsFile.clear();
        prepared.briefing += QStringLiteral("\n## The %1 requirements\n\n%2").arg(spec, renderRequirements(requirements));
    }
    return prepared;
}

} // namespace requirement
