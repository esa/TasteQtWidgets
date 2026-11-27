/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// The programmatic entry point: text in, SVG out. A C++ port of mermaid2svg's
// api.py. The diagram types implemented are flowchart, sequence, state, class
// and er; a recognised-but-unported type raises UnsupportedDiagramError.

#include "tqtw_m2serrors.h"

#include <map>
#include <string>

namespace tqtw_mermaid2svg {

//: Rendering options, one per command-line flag.
struct Options
{
    std::string theme = "default";
    std::string backgroundColor = "white";
    std::map<std::string, std::string> config;
    std::string css;
    std::string svgId = "my-svg";
    std::string font; // empty: the embedded metrics
};

//: A finished render.
struct Result
{
    std::string svg;
    double width = 0;
    double height = 0;
    bool hasTitle = false;
    std::string title;
    bool hasDesc = false;
    std::string desc;
    std::string diagramType;
};

//: Render one mermaid definition to SVG. Throws MermaidError (or a subtype) on
//: a parse failure.
Result render(const std::string &text, const Options &options = Options());
Result render(const std::string &text, const Options &options, Diagnostics *diagnostics);

//: The diagram type of a definition, without rendering.
std::string detectDiagramType(const std::string &text);

} // namespace tqtw_mermaid2svg
