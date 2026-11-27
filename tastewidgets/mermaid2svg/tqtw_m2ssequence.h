/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Sequence diagrams: a C++ port of mermaid2svg's diagrams/sequence.py. No graph
// layout: a vertical cursor walking a list of events, plus a horizontal pass
// that widens actor gaps until every message label fits.

#include "tqtw_m2sbase.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tqtw_mermaid2svg {

struct SeqActor
{
    std::string id;
    std::string label;
    std::string kind = "participant"; // participant | actor
    bool hasBox = false;
    std::string box;
    int line = 0;
    int order = 0;
    double x = 0;
    double width = 150.0;
};

struct SeqMessage
{
    std::string source;
    std::string target;
    std::string text;
    std::string style = "solid";
    std::string startHead;
    std::string endHead = "arrow";
    bool activate = false;
    bool deactivate = false;
    int line = 0;
};

struct SeqNote
{
    std::string placement; // left | right | over
    std::vector<std::string> actors;
    std::string text;
    int line = 0;
};

struct SeqBlockStart
{
    std::string kind;
    std::string text;
    int line = 0;
};
struct SeqBlockSection
{
    std::string kind;
    std::string text;
    int line = 0;
};
struct SeqBlockEnd
{
    int line = 0;
};
struct SeqActivation
{
    std::string actor;
    bool start = false;
    int line = 0;
};
struct SeqBoxStart
{
    std::string label;
    std::string colour;
    int line = 0;
};
struct SeqBoxEnd
{
    int line = 0;
};

//: One event, as a tagged union (the parser appends one per statement).
struct SeqEvent
{
    enum Kind { Message, Note, BlockStart, BlockSection, BlockEnd, Activation, BoxStart, BoxEnd } kind = Message;
    SeqMessage message;
    SeqNote note;
    SeqBlockStart blockStart;
    SeqBlockSection blockSection;
    SeqBlockEnd blockEnd;
    SeqActivation activation;
    SeqBoxStart boxStart;
    SeqBoxEnd boxEnd;
};

class SequenceDiagram : public DiagramBase
{
public:
    SequenceDiagram(const Source &source, Diagnostics *diagnostics);

    std::map<std::string, SeqActor> actors;
    std::vector<SeqEvent> events;
    bool autonumber = false;

    static std::shared_ptr<SequenceDiagram> parse(const Source &source, Diagnostics *diagnostics);
    RenderResult render(const RenderContext &ctx) override;
};

} // namespace tqtw_mermaid2svg
