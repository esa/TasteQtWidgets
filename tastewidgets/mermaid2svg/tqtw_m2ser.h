/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Entity-relationship diagrams: a C++ port of mermaid2svg's diagrams/er.py.

#include "tqtw_m2sbase.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace tqtw_mermaid2svg {

struct ErAttribute
{
    std::string type;
    std::string name;
    std::string keys;
    std::string comment;
};

struct Entity
{
    std::string id;
    std::string label;
    std::vector<ErAttribute> attributes;
    int line = 0;
};

struct ErRelation
{
    std::string source;
    std::string target;
    std::string leftCard;
    std::string rightCard;
    bool identifying = false;
    std::string label;
    int line = 0;
};

class ErDiagram : public DiagramBase
{
public:
    ErDiagram(const Source &source, Diagnostics *diagnostics);

    std::string direction = "TB";
    std::map<std::string, Entity> entities;
    std::vector<std::string> entityOrder;
    std::vector<ErRelation> relations;

    static std::shared_ptr<ErDiagram> parse(const Source &source, Diagnostics *diagnostics);
    RenderResult render(const RenderContext &ctx) override;
};

} // namespace tqtw_mermaid2svg
