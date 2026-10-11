/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Flowchart / graph diagrams, laid out by the layered graph layout: a C++ port
// of mermaid2svg's diagrams/flowchart.py.

#include "tqtw_m2sbase.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tqtw_mermaid2svg {

struct FlowNode
{
    std::string id;
    std::string label;
    std::string shape = "rect";
    std::vector<std::string> classes;
    std::string style;
    bool hasParent = false;
    std::string parent;
    int line = 0;
    bool hasLink = false;
    std::string link;
    bool labelled = false;
};

struct FlowEdge
{
    std::string source;
    std::string target;
    std::string label;
    std::string kind = "normal"; // normal | thick | dotted | invisible
    std::string startHead;       // "" | arrow | circle | cross
    std::string endHead = "arrow";
    int minlen = 1;
    std::string style;
    int line = 0;
    int index = 0;
};

struct Subgraph
{
    std::string id;
    std::string label;
    bool hasParent = false;
    std::string parent;
    bool hasDirection = false;
    std::string direction;
    int line = 0;
};

//: The flowchart diagram: parse then render.
class Flowchart : public DiagramBase
{
public:
    Flowchart(const Source &source, Diagnostics *diagnostics);

    std::string direction = "TB";
    std::map<std::string, FlowNode> nodes;
    std::vector<FlowEdge> edges;
    std::map<std::string, Subgraph> subgraphs;
    std::map<std::string, std::string> classDefs;
    std::map<int, std::string> linkStyles;
    std::string linkStyleDefault;

    static std::shared_ptr<Flowchart> parse(const Source &source, Diagnostics *diagnostics);
    RenderResult render(const RenderContext &ctx) override;
};

} // namespace tqtw_mermaid2svg
