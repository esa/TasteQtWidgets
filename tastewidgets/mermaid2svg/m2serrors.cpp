/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2serrors.h"

#include <algorithm>

namespace tqtw_mermaid2svg {

std::string severityName(Severity severity)
{
    switch (severity) {
    case Severity::Error: return "error";
    case Severity::Warning: return "warning";
    case Severity::Info: return "info";
    }
    return "error";
}

std::string Diagnostic::formatText(const std::string &filename) const
{
    std::string loc = filename;
    if (line) {
        loc += ":" + std::to_string(line);
        if (column) {
            loc += ":" + std::to_string(column);
        }
    }
    std::string out = loc + ": " + severityName(severity) + ": " + message + " [" + rule + "]";
    if (hasSourceLine) {
        std::string stripped = sourceLine;
        while (!stripped.empty() && (stripped.back() == '\n' || stripped.back() == '\r')) {
            stripped.pop_back();
        }
        out += "\n  " + stripped;
        if (column) {
            std::string pad;
            for (int i = 0; i < column - 1 && i < static_cast<int>(stripped.size()); ++i) {
                pad.push_back(stripped[i] == '\t' ? '\t' : ' ');
            }
            out += "\n  " + pad + "^";
        }
    }
    return out;
}

std::string Diagnostics::sourceOf(int line) const
{
    if (line >= 1 && line <= static_cast<int>(lines.size())) {
        return lines[line - 1];
    }
    return std::string();
}

void Diagnostics::add(Severity severity, const std::string &message, int line, int column, const std::string &rule)
{
    Diagnostic d;
    d.severity = severity;
    d.message = message;
    d.line = line;
    d.column = column;
    d.rule = rule;
    const std::string source = sourceOf(line);
    if (!source.empty()) {
        d.hasSourceLine = true;
        d.sourceLine = source;
    }
    items.push_back(std::move(d));
}

void Diagnostics::error(const std::string &message, int line, int column, const std::string &rule)
{
    add(Severity::Error, message, line, column, rule);
}

void Diagnostics::warn(const std::string &message, int line, int column, const std::string &rule)
{
    add(Severity::Warning, message, line, column, rule);
}

void Diagnostics::info(const std::string &message, int line, int column, const std::string &rule)
{
    add(Severity::Info, message, line, column, rule);
}

bool Diagnostics::hasErrors() const
{
    for (const Diagnostic &d : items) {
        if (d.severity == Severity::Error) {
            return true;
        }
    }
    return false;
}

std::vector<Diagnostic> Diagnostics::sorted() const
{
    std::vector<Diagnostic> out = items;
    std::sort(out.begin(), out.end(), [](const Diagnostic &a, const Diagnostic &b) {
        return a.line != b.line ? a.line < b.line : a.column < b.column;
    });
    return out;
}

} // namespace tqtw_mermaid2svg
