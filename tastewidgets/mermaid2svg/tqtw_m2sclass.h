/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Class diagrams: a C++ port of mermaid2svg's diagrams/classdiag.py.

#include "tqtw_m2sbase.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tqtw_mermaid2svg {

struct ClassDef
{
    std::string id;
    std::string label;
    std::string annotation;
    std::vector<std::string> attributes;
    std::vector<std::string> methods;
    bool hasNamespace = false;
    std::string nameSpace;
    std::string cssClass;
    int line = 0;
};

struct Relation
{
    std::string source;
    std::string target;
    std::string kind;
    std::string arrow;
    bool dotted = false;
    std::string label;
    std::string sourceCard;
    std::string targetCard;
    int line = 0;
};

struct ClassNote
{
    std::string text;
    bool hasTarget = false;
    std::string target;
    int line = 0;
};

class ClassDiagram : public DiagramBase
{
public:
    ClassDiagram(const Source &source, Diagnostics *diagnostics);

    std::string direction = "TB";
    std::map<std::string, ClassDef> classes;
    std::vector<std::string> classOrder;
    std::vector<Relation> relations;
    std::vector<ClassNote> notes;
    std::map<std::string, std::string> namespaces; // name -> parent ("" root)
    std::vector<std::string> namespaceOrder;

    static std::shared_ptr<ClassDiagram> parse(const Source &source, Diagnostics *diagnostics);
    RenderResult render(const RenderContext &ctx) override;
};

} // namespace tqtw_mermaid2svg
