/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Error and diagnostic types, a C++ port of mermaid2svg's errors.py.

#include <stdexcept>
#include <string>
#include <vector>

namespace tqtw_mermaid2svg {

enum class Severity { Error, Warning, Info };

std::string severityName(Severity severity);

//: A single lint/parse message anchored at a source position.
struct Diagnostic
{
    Severity severity = Severity::Error;
    std::string message;
    int line = 0;
    int column = 0;
    std::string rule = "syntax";
    bool hasSourceLine = false;
    std::string sourceLine;

    std::string formatText(const std::string &filename = "-") const;
};

//: Base class for all mermaid2svg failures.
class MermaidError : public std::runtime_error
{
public:
    explicit MermaidError(const std::string &message)
        : std::runtime_error(message)
    {
    }
};

//: A diagram could not be parsed.
class ParseError : public MermaidError
{
public:
    explicit ParseError(const std::string &message, int line = 0, int column = 0)
        : MermaidError(message)
        , line(line)
        , column(column)
    {
    }
    int line = 0;
    int column = 0;
};

//: The text does not start with a recognised diagram keyword.
class UnknownDiagramError : public ParseError
{
public:
    using ParseError::ParseError;
};

//: A recognised diagram type that is deliberately not implemented.
class UnsupportedDiagramError : public MermaidError
{
public:
    explicit UnsupportedDiagramError(const std::string &kind, const std::string &reason = std::string())
        : MermaidError("Diagram type '" + kind + "' is not supported by mermaid2svg"
                  + (reason.empty() ? std::string() : ": " + reason))
        , kind(kind)
    {
    }
    std::string kind;
};

//: Accumulator a parser reports non-fatal issues to.
class Diagnostics
{
public:
    void add(Severity severity, const std::string &message, int line = 0, int column = 0,
            const std::string &rule = "semantic");
    void error(const std::string &message, int line = 0, int column = 0, const std::string &rule = "semantic");
    void warn(const std::string &message, int line = 0, int column = 0, const std::string &rule = "semantic");
    void info(const std::string &message, int line = 0, int column = 0, const std::string &rule = "semantic");
    bool hasErrors() const;
    std::vector<Diagnostic> sorted() const;

    std::vector<Diagnostic> items;
    std::vector<std::string> lines;

private:
    std::string sourceOf(int line) const;
};

} // namespace tqtw_mermaid2svg
