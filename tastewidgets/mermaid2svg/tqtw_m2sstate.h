/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// State diagrams: a C++ port of mermaid2svg's diagrams/state.py.

#include "tqtw_m2sbase.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tqtw_mermaid2svg {

struct State
{
    std::string id;
    std::string label;
    std::string kind = "normal"; // normal | start | end | fork | join | choice
    bool hasParent = false;
    std::string parent;
    bool composite = false;
    int line = 0;
};

struct Transition
{
    std::string source;
    std::string target;
    std::string label;
    int line = 0;
};

struct StateNote
{
    std::string target;
    std::string placement;
    std::string text;
    int line = 0;
};

class StateDiagram : public DiagramBase
{
public:
    StateDiagram(const Source &source, Diagnostics *diagnostics);

    std::string direction = "TB";
    std::map<std::string, State> states;
    //: State ids in declaration order (the reference iterates its dict in
    //: insertion order, which the layout depends on).
    std::vector<std::string> stateOrder;
    std::vector<Transition> transitions;
    std::vector<StateNote> notes;

    static std::shared_ptr<StateDiagram> parse(const Source &source, Diagnostics *diagnostics);
    RenderResult render(const RenderContext &ctx) override;
};

} // namespace tqtw_mermaid2svg
