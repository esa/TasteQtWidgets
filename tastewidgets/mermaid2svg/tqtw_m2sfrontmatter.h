/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Source preprocessing: frontmatter, init directives, comments, type sniffing.
// A C++ port of mermaid2svg's frontmatter.py.

#include "tqtw_m2sjson.h"

#include <string>
#include <utility>
#include <vector>

namespace tqtw_mermaid2svg {

class Diagnostics;

//: A mermaid definition split into metadata and body.
struct Source
{
    std::string text; // body with frontmatter and directives removed
    int lineOffset = 0;
    bool hasTitle = false;
    std::string title;
    bool hasAccDescr = false;
    std::string accDescr;
    JsonValue config;
    bool hasDisplayMode = false;
    std::string displayMode;

    std::vector<std::string> lines() const;
    int absLine(int bodyLine) const { return bodyLine + lineOffset; }
};

//: Strip frontmatter/directives/comments and collect their metadata.
Source preprocess(const std::string &text, Diagnostics *diagnostics = nullptr);

//: The diagram id and keyword for a preprocessed source. Throws
//: UnknownDiagramError or UnsupportedDiagramError.
std::pair<std::string, std::string> detectType(const Source &source);

} // namespace tqtw_mermaid2svg
