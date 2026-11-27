/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Shared plumbing for diagram parsers and renderers: a C++ port of
// mermaid2svg's diagrams/base.py.

#include "tqtw_m2sfrontmatter.h"
#include "tqtw_m2sjson.h"
#include "tqtw_m2ssvg.h"
#include "tqtw_m2stext.h"
#include "tqtw_m2stheme.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tqtw_mermaid2svg {

class Diagnostics;

//: `float(value)` with a fallback; inf and nan fall back too.
double asNumber(const std::string &value, double fallback);

//: Everything a renderer needs that is not the diagram itself.
class RenderContext
{
public:
    Theme theme;
    std::shared_ptr<TextMeasurer> measurer;
    JsonValue config;
    std::string svgId = "my-svg";
    std::string background = "white";
    std::string extraCss;

    //: Per-diagram config block, e.g. config["flowchart"].
    JsonValue sub(const std::string &key) const;
    //: A numeric config value, coerced (never throws).
    double num(const std::string &key, const std::string &name, double fallback) const;
    bool useMaxWidth(const std::string &key) const;
};

//: A finished SVG, its size, and the accessibility text inside it.
struct RenderResult
{
    std::string svg;
    double width = 0;
    double height = 0;
    bool hasTitle = false;
    std::string title;
    bool hasDesc = false;
    std::string desc;
};

//: One logical source line with its 1-based position in the document.
struct SourceLine
{
    std::string text;
    int number = 0;
    int indent = 0;
    std::string stripped() const;
};

//: Non-blank lines of the body, tagged with absolute line numbers.
std::vector<SourceLine> iterLines(const Source &source, bool skipFirst = true);
//: The diagram declaration line and its absolute number.
std::pair<std::string, int> firstLine(const Source &source);
//: Strip one layer of matching quotes.
std::string unquote(const std::string &text);

class DiagramBase;

//: Handle title:/accTitle:/accDescr:. True if consumed.
bool consumeCommon(DiagramBase &diagram, const SourceLine &line);

//: The font-family rule for the measured font (empty when it is the built-in).
std::vector<std::string> fontCss(const RenderContext &ctx);

//: Register an arrowhead marker once; returns its url(#...) value.
std::string markerDefs(SvgDocument &doc, const std::string &name, const std::string &color,
        const std::string &kind = "arrow");

//: Base class: subclasses implement parse() and render().
class DiagramBase
{
public:
    virtual ~DiagramBase() = default;

    std::string diagramId;
    std::string role = "diagram";
    std::string configKey;

    Source source;
    Diagnostics *diagnostics = nullptr;
    bool hasTitle = false;
    std::string title;
    bool hasAccTitle = false;
    std::string accTitle;
    bool hasAccDescr = false;
    std::string accDescr;

    virtual RenderResult render(const RenderContext &ctx) = 0;

    //: A fresh document wired to this diagram's role and accessibility text.
    std::shared_ptr<SvgDocument> newDocument(const RenderContext &ctx) const;
    //: Finish: prepend the font CSS, pad the viewBox, produce the SVG.
    RenderResult finish(const RenderContext &ctx, SvgDocument &doc, double width, double height,
            double pad = 8.0, double padY = -1.0) const;
};

} // namespace tqtw_mermaid2svg
