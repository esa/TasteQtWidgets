/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2ssequence.h"

#include "tqtw_m2serrors.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>
#include <set>
#include <sstream>

namespace tqtw_mermaid2svg {

using Point = std::pair<double, double>;

namespace {

std::string trim(const std::string &s)
{
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool startsWith(const std::string &s, const std::string &p)
{
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

struct ArrowToken
{
    std::string token;
    std::string style;
    std::string startHead;
    std::string endHead;
};

const std::vector<ArrowToken> &arrowTokens()
{
    static const std::vector<ArrowToken> tokens = {
        { "<<-->>", "dotted", "arrow", "arrow" }, { "<<->>", "solid", "arrow", "arrow" },
        { "-->>", "dotted", "", "arrow" }, { "--x", "dotted", "", "cross" }, { "--)", "dotted", "", "open" },
        { "-->", "dotted", "", "" }, { "->>", "solid", "", "arrow" }, { "-x", "solid", "", "cross" },
        { "-)", "solid", "", "open" }, { "->", "solid", "", "" },
    };
    return tokens;
}

const std::map<std::string, std::string> &blockStarters()
{
    static const std::map<std::string, std::string> m = { { "loop", "loop" }, { "alt", "alt" }, { "opt", "opt" },
        { "par", "par" }, { "critical", "critical" }, { "break", "break" }, { "rect", "rect" }, { "par_over", "par" } };
    return m;
}

const std::map<std::string, std::string> &blockSections()
{
    static const std::map<std::string, std::string> m = { { "else", "alt" }, { "and", "par" }, { "option", "critical" } };
    return m;
}

std::pair<std::string, std::string> splitBoxHeader(const std::string &restIn)
{
    const std::string rest = trim(restIn);
    std::smatch match;
    const std::regex re(R"(^(rgba?\([^)]*\)|#[0-9a-fA-F]{3,8}|transparent|\w+)\s+(.*)$)");
    if (std::regex_match(rest, match, re) && !match[2].str().empty()) {
        return { match[1].str(), unquote(trim(match[2].str())) };
    }
    return { "transparent", unquote(rest) };
}

} // namespace

SequenceDiagram::SequenceDiagram(const Source &source, Diagnostics *diagnostics)
{
    this->source = source;
    this->diagnostics = diagnostics;
    diagramId = "sequence";
    role = "sequence";
    configKey = "sequence";
    hasTitle = source.hasTitle;
    title = source.title;
    hasAccDescr = source.hasAccDescr;
    accDescr = source.accDescr;
}

namespace {

SeqActor &actor(SequenceDiagram &d, const std::string &actorIdIn, int line, const std::string &label, bool hasLabel)
{
    const std::string actorId = unquote(trim(actorIdIn));
    auto it = d.actors.find(actorId);
    if (it == d.actors.end()) {
        SeqActor a;
        a.id = actorId;
        a.label = hasLabel ? label : actorId;
        a.line = line;
        a.order = static_cast<int>(d.actors.size());
        return d.actors.emplace(actorId, a).first->second;
    }
    return it->second;
}

} // namespace

std::shared_ptr<SequenceDiagram> SequenceDiagram::parse(const Source &source, Diagnostics *diagnostics)
{
    auto d = std::make_shared<SequenceDiagram>(source, diagnostics);
    auto &diag = *diagnostics;
    std::vector<int> blockStack;
    std::vector<int> boxStack;

    auto participant = [&](const SourceLine &line) {
        const std::string text = line.stripped();
        const std::size_t sp = text.find(' ');
        const std::string keyword = sp == std::string::npos ? text : text.substr(0, sp);
        std::string rest = sp == std::string::npos ? "" : trim(text.substr(sp));
        std::string actorId, label;
        std::smatch match;
        const std::regex re(R"(^(.+?)\s+as\s+(.+)$)", std::regex::icase);
        if (std::regex_match(rest, match, re)) {
            actorId = trim(match[1].str());
            label = unquote(trim(match[2].str()));
        } else {
            actorId = label = unquote(rest);
        }
        if (actorId.empty()) {
            diag.error("participant needs a name", line.number, 1, "participant");
            return;
        }
        SeqActor &a = actor(*d, actorId, line.number, label, true);
        a.label = label;
        a.kind = lower(keyword);
    };

    auto note = [&](const SourceLine &line) {
        const std::string text = line.stripped();
        std::smatch match;
        const std::regex re(R"(^note\s+(left of|right of|over)\s+([^:]+):?\s*(.*)$)", std::regex::icase);
        if (!std::regex_match(text, match, re)) {
            throw ParseError("could not parse note; expected 'Note left of/right of/over <actor>: text'",
                    line.number, 1);
        }
        std::string placement = lower(match[1].str());
        const std::size_t of = placement.find(" of");
        if (of != std::string::npos) placement = placement.substr(0, of);
        SeqEvent ev;
        ev.kind = SeqEvent::Note;
        ev.note.placement = placement;
        std::string actorsCsv = match[2].str();
        std::size_t start = 0;
        while (start <= actorsCsv.size()) {
            const std::size_t comma = actorsCsv.find(',', start);
            const std::string piece = actorsCsv.substr(start,
                    comma == std::string::npos ? std::string::npos : comma - start);
            ev.note.actors.push_back(actor(*d, piece, line.number, "", false).id);
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        ev.note.text = trim(match[3].str());
        ev.note.line = line.number;
        d->events.push_back(ev);
    };

    auto message = [&](const SourceLine &line) {
        const std::string text = line.stripped();
        const std::size_t colon = text.find(':');
        const std::string head = colon == std::string::npos ? text : text.substr(0, colon);
        for (const ArrowToken &tok : arrowTokens()) {
            const std::size_t index = head.find(tok.token);
            if (index == std::string::npos) continue;
            const std::string left = trim(text.substr(0, index));
            const std::string right = text.substr(index + tok.token.size());
            std::string body = right;
            std::string msgText;
            const std::size_t c2 = right.find(':');
            if (c2 != std::string::npos) {
                body = trim(right.substr(0, c2));
                msgText = trim(right.substr(c2 + 1));
            } else {
                body = trim(right);
            }
            if (left.empty() || body.empty()) continue;
            bool activate = false, deactivate = false;
            if (!body.empty() && body.front() == '+') {
                activate = true;
                body = trim(body.substr(1));
            } else if (!body.empty() && body.front() == '-') {
                deactivate = true;
                body = trim(body.substr(1));
            }
            const std::string sourceId = actor(*d, left, line.number, "", false).id;
            const std::string targetId = actor(*d, body, line.number, "", false).id;
            SeqEvent ev;
            ev.kind = SeqEvent::Message;
            ev.message.source = sourceId;
            ev.message.target = targetId;
            ev.message.text = msgText;
            ev.message.style = tok.style;
            ev.message.startHead = tok.startHead;
            ev.message.endHead = tok.endHead;
            ev.message.activate = activate;
            ev.message.deactivate = deactivate;
            ev.message.line = line.number;
            d->events.push_back(ev);
            return;
        }
        throw ParseError("could not parse statement; expected a message like 'A->>B: text'", line.number, 1);
    };

    for (const SourceLine &line : iterLines(source)) {
        const std::string text = line.stripped();
        if (text.empty()) continue;
        if (consumeCommon(*d, line)) continue;
        const std::string lowered = lower(text);

        if (lowered == "autonumber" || startsWith(lowered, "autonumber ")) {
            d->autonumber = true;
            continue;
        }
        if (lowered == "end") {
            if (!boxStack.empty() && (blockStack.empty() || boxStack.back() > blockStack.back())) {
                boxStack.pop_back();
                SeqEvent ev;
                ev.kind = SeqEvent::BoxEnd;
                ev.boxEnd.line = line.number;
                d->events.push_back(ev);
            } else if (!blockStack.empty()) {
                blockStack.pop_back();
                SeqEvent ev;
                ev.kind = SeqEvent::BlockEnd;
                ev.blockEnd.line = line.number;
                d->events.push_back(ev);
            } else {
                diag.error("'end' without a matching block", line.number, 1, "block");
            }
            continue;
        }
        if (startsWith(lowered, "box")) {
            const std::string rest = trim(text.substr(3));
            const auto header = splitBoxHeader(rest);
            SeqEvent ev;
            ev.kind = SeqEvent::BoxStart;
            ev.boxStart.colour = header.first;
            ev.boxStart.label = header.second;
            ev.boxStart.line = line.number;
            d->events.push_back(ev);
            boxStack.push_back(line.number);
            continue;
        }
        if (std::regex_search(text, std::regex(R"(^(participant|actor)\b)", std::regex::icase))) {
            participant(line);
            continue;
        }
        if (std::regex_search(text, std::regex(R"(^(activate|deactivate)\b)", std::regex::icase))) {
            const std::size_t sp = text.find(' ');
            const std::string keyword = sp == std::string::npos ? text : text.substr(0, sp);
            const std::string name = sp == std::string::npos ? "" : trim(text.substr(sp));
            SeqEvent ev;
            ev.kind = SeqEvent::Activation;
            ev.activation.actor = actor(*d, name, line.number, "", false).id;
            ev.activation.start = lower(keyword) == "activate";
            ev.activation.line = line.number;
            d->events.push_back(ev);
            continue;
        }
        if (startsWith(lowered, "note ")) {
            note(line);
            continue;
        }
        if (startsWith(lowered, "links ") || startsWith(lowered, "link ") || startsWith(lowered, "properties ")
                || startsWith(lowered, "details ")) {
            const std::string word = text.substr(0, text.find(' '));
            diag.info("ignoring unsupported directive '" + word + "'", line.number, 1, "unsupported");
            continue;
        }

        // block keyword
        {
            const std::size_t sp = text.find_first_of(" \t");
            std::string keyword = (sp == std::string::npos ? text : text.substr(0, sp));
            if (!keyword.empty() && keyword.back() == ':') keyword.pop_back();
            const std::string lkey = lower(keyword);
            if (blockSections().count(lkey)) {
                if (blockStack.empty()) {
                    diag.error("'" + lkey + "' outside of a block", line.number, 1, "block");
                    continue;
                }
                SeqEvent ev;
                ev.kind = SeqEvent::BlockSection;
                ev.blockSection.kind = blockSections().at(lkey);
                ev.blockSection.text = trim(text.substr(keyword.size()));
                ev.blockSection.line = line.number;
                d->events.push_back(ev);
                continue;
            }
            if (blockStarters().count(lkey)) {
                SeqEvent ev;
                ev.kind = SeqEvent::BlockStart;
                ev.blockStart.kind = blockStarters().at(lkey);
                ev.blockStart.text = trim(text.substr(keyword.size()));
                ev.blockStart.line = line.number;
                d->events.push_back(ev);
                blockStack.push_back(line.number);
                continue;
            }
        }
        message(line);
    }

    for (int pending : blockStack) {
        diag.error("block is never closed with 'end'", pending, 1, "block");
    }
    for (int pending : boxStack) {
        diag.error("box is never closed with 'end'", pending, 1, "block");
    }

    // validate
    if (d->actors.empty()) {
        diag.warn("sequence diagram has no participants", 0, 0, "empty");
    }
    std::map<std::string, int> depth;
    for (const auto &a : d->actors) depth[a.first] = 0;
    for (const SeqEvent &ev : d->events) {
        if (ev.kind == SeqEvent::Activation) {
            depth[ev.activation.actor] += ev.activation.start ? 1 : -1;
            if (depth[ev.activation.actor] < 0) {
                diag.error("deactivate '" + ev.activation.actor + "' without a matching activate", ev.activation.line,
                        1, "activation");
                depth[ev.activation.actor] = 0;
            }
        } else if (ev.kind == SeqEvent::Message) {
            if (ev.message.activate) depth[ev.message.target] += 1;
            if (ev.message.deactivate) {
                depth[ev.message.source] -= 1;
                if (depth[ev.message.source] < 0) {
                    diag.error("'-' deactivates '" + ev.message.source + "' which is not active", ev.message.line, 1,
                            "activation");
                    depth[ev.message.source] = 0;
                }
            }
        }
    }
    for (const auto &e : depth) {
        if (e.second > 0) {
            diag.warn("participant '" + e.first + "' is left activated " + std::to_string(e.second) + " time(s)", 0, 0,
                    "activation");
        }
    }
    return d;
}

} // namespace tqtw_mermaid2svg

namespace tqtw_mermaid2svg {

namespace {

constexpr double kActorHeight = 65.0;
constexpr double kActorMinWidth = 150.0;
constexpr double kActorMargin = 50.0;
constexpr double kMessageMargin = 35.0;
constexpr double kBoxMargin = 10.0;
constexpr double kNoteMargin = 25.0;
constexpr double kSelfLabelGap = 8.0;
constexpr double kActivationWidth = 10.0;
constexpr double kLabelPad = 10.0;
constexpr double kMessageLineRatio = 1.0625;
constexpr double kDiagramMarginX = 50.0;
constexpr double kDiagramMarginY = 10.0;

const std::map<std::string, std::string> &markerKinds()
{
    static const std::map<std::string, std::string> m = { { "arrow", "arrow" }, { "cross", "cross" },
        { "open", "point" }, { "", "arrow" } };
    return m;
}

double asNum(const JsonValue &v, double fallback)
{
    if (v.isNull()) return fallback;
    if (v.type() == JsonValue::Number) return v.toNumber();
    return asNumber(v.toString(), fallback);
}

} // namespace

RenderResult SequenceDiagram::render(const RenderContext &ctx)
{
    const Theme &theme = ctx.theme;
    const JsonValue block = ctx.sub("sequence");
    const double actorMargin = asNum(block["actorMargin"], kActorMargin);
    const double minWidth = asNum(block["width"], kActorMinWidth);
    const double actorHeight = asNum(block["height"], kActorHeight);
    const double messageMargin = asNum(block["messageMargin"], kMessageMargin);
    const double boxMargin = asNum(block["boxMargin"], kBoxMargin);
    const bool mirrorActors = block["mirrorActors"].isNull() ? true : block["mirrorActors"].toBool();
    const double marginX = asNum(block["diagramMarginX"], kDiagramMarginX);
    const double marginY = asNum(block["diagramMarginY"], kDiagramMarginY);
    const double fontSize = theme.fontSize();
    const double actorFontSize = asNum(block["actorFontSize"], fontSize);
    const double wrapWidth = asNum(block["wrap"], 0) != 0 ? asNum(block["wrap"], 0) : 300.0;
    std::shared_ptr<TextMeasurer> measurer = ctx.measurer;

    std::vector<SeqActor> order;
    for (const auto &e : actors) order.push_back(e.second);
    std::sort(order.begin(), order.end(),
            [](const SeqActor &a, const SeqActor &b) { return a.order < b.order; });

    double minX = 0, maxX = 0;
    auto seen = [&](double left, double right) {
        minX = std::min(minX, left);
        maxX = std::max(maxX, right);
    };

    auto textWidth = [&](const std::string &text, double size) {
        double w = 0;
        for (const std::string &ln : measurer->splitLines(text)) {
            w = std::max(w, measurer->width(ln, size > 0 ? size : fontSize));
        }
        return w;
    };
    auto textHeight = [&](const std::string &text) {
        const std::size_t lines = std::max<std::size_t>(1, measurer->splitLines(text).size());
        return static_cast<double>(lines) * fontSize * 1.25;
    };

    // -- horizontal pass --
    for (SeqActor &a : order) {
        TextBlock blk = measurer->block(a.label, actorFontSize, false, wrapWidth);
        a.width = std::max(minWidth, blk.width() + 2 * kLabelPad);
    }
    std::map<std::string, int> index;
    for (std::size_t i = 0; i < order.size(); ++i) index[order[i].id] = static_cast<int>(i);
    std::vector<double> spans(std::max<int>(0, static_cast<int>(order.size()) - 1), 0.0);
    auto require = [&](int gap, double width) {
        if (gap >= 0 && gap < static_cast<int>(spans.size())) spans[gap] = std::max(spans[gap], width);
    };
    for (const SeqEvent &ev : events) {
        if (ev.kind == SeqEvent::Message) {
            const int i = index[ev.message.source], j = index[ev.message.target];
            const double needed = textWidth(ev.message.text, fontSize) + 2 * kLabelPad;
            if (i == j) {
                require(i, needed / 2);
                require(i - 1, needed / 2);
            } else if (std::abs(i - j) == 1) {
                require(std::min(i, j), needed);
            }
        } else if (ev.kind == SeqEvent::Note) {
            std::vector<int> positions;
            for (const std::string &a : ev.note.actors) {
                if (index.count(a)) positions.push_back(index[a]);
            }
            if (positions.empty()) continue;
            const double needed = textWidth(ev.note.text, fontSize) + 2 * kNoteMargin;
            if (ev.note.placement == "over" && positions.size() > 1) {
                const int lo = *std::min_element(positions.begin(), positions.end());
                const int hi = *std::max_element(positions.begin(), positions.end());
                const double share = needed / (hi - lo);
                for (int gap = lo; gap < hi; ++gap) require(gap, share);
            } else if (ev.note.placement == "left") {
                require(*std::min_element(positions.begin(), positions.end()) - 1, needed);
            } else {
                require(*std::min_element(positions.begin(), positions.end()), needed);
            }
        }
    }
    double x = 0;
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i].x = x + order[i].width / 2;
        if (i < spans.size()) {
            const SeqActor &next = order[i + 1];
            const double centres = std::max(actorMargin + order[i].width / 2 + next.width / 2,
                    spans[i] + actorMargin);
            x = order[i].x + centres - next.width / 2;
        } else {
            x += order[i].width;
        }
    }
    const double totalWidth = x;

    std::map<std::string, SeqActor *> actorById;
    for (SeqActor &a : order) actorById[a.id] = &a;

    auto doc = newDocument(ctx);
    // css
    {
        std::ostringstream css;
        css << "#{id} .actor-shape { fill: " << theme.get("actorBkg", "#ECECFF") << "; stroke: "
            << theme.get("actorBorder", "#9370DB") << "; stroke-width: 1px; }\n";
        css << "#{id} .actor-label, #{id} .actor-label tspan { fill: " << theme.get("actorTextColor", "#333")
            << "; font-weight: bold; }\n";
        css << "#{id} .actor-line { stroke: " << theme.get("actorLineColor", "#999")
            << "; stroke-width: 0.5px; stroke-dasharray: 3 3; }\n";
        css << "#{id} .message-line { stroke: " << theme.get("signalColor", "#333")
            << "; stroke-width: 2px; fill: none; }\n";
        css << "#{id} .message-text, #{id} .message-text tspan { fill: " << theme.get("signalTextColor", "#333")
            << "; font-size: " << fmt(fontSize * 0.9) << "px; }\n";
        css << "#{id} .note-shape { fill: " << theme.get("noteBkgColor", "#fff5ad") << "; stroke: "
            << theme.get("noteBorderColor", "#aaaa33") << "; }\n";
        css << "#{id} .note-text, #{id} .note-text tspan { fill: " << theme.get("noteTextColor", "#333") << "; }\n";
        css << "#{id} .activation { fill: " << theme.get("activationBkgColor", "#f4f4f4") << "; stroke: "
            << theme.get("activationBorderColor", "#666") << "; stroke-width: 1px; }\n";
        css << "#{id} .block-rect { stroke: " << theme.get("labelBoxBorderColor", "#9370DB")
            << "; stroke-width: 2px; fill: none; }\n";
        css << "#{id} .block-divider { stroke: " << theme.get("labelBoxBorderColor", "#9370DB")
            << "; stroke-width: 1px; stroke-dasharray: 2 2; }\n";
        css << "#{id} .block-label-box { fill: " << theme.get("labelBoxBkgColor", "#ECECFF") << "; stroke: "
            << theme.get("labelBoxBorderColor", "#9370DB") << "; stroke-width: 1px; }\n";
        css << "#{id} .block-kind, #{id} .block-kind tspan { fill: " << theme.get("labelTextColor", "#333")
            << "; font-weight: bold; }\n";
        css << "#{id} .block-text, #{id} .block-text tspan { fill: " << theme.get("loopTextColor", "#333") << "; }\n";
        css << "#{id} .sequence-number-bkg { fill: " << theme.get("signalColor", "#333") << "; }\n";
        css << "#{id} .sequence-number, #{id} .sequence-number tspan { fill: "
            << theme.get("sequenceNumberColor", "#fff") << "; }\n";
        css << "#{id} .box-rect { opacity: 0.35; stroke: none; }";
        doc->addStyle(css.str());
    }
    if (order.empty()) {
        return finish(ctx, *doc, 1, 1);
    }

    const double top = hasTitle ? 34.0 : 0.0;
    const double lifelineTop = top + actorHeight;
    double y = lifelineTop;

    auto boxesGroup = std::make_shared<El>("g");
    boxesGroup->attr("class", "boxes");
    auto lifelinesGroup = std::make_shared<El>("g");
    lifelinesGroup->attr("class", "lifelines");
    auto activationsGroup = std::make_shared<El>("g");
    activationsGroup->attr("class", "activations");
    auto blocksGroup = std::make_shared<El>("g");
    blocksGroup->attr("class", "blocks");
    auto messagesGroup = std::make_shared<El>("g");
    messagesGroup->attr("class", "messages");
    auto notesGroup = std::make_shared<El>("g");
    notesGroup->attr("class", "notes");
    auto actorsGroup = std::make_shared<El>("g");
    actorsGroup->attr("class", "actors");
    doc->root()->add(boxesGroup);
    doc->root()->add(lifelinesGroup);
    doc->root()->add(activationsGroup);
    doc->root()->add(blocksGroup);
    doc->root()->add(messagesGroup);
    doc->root()->add(notesGroup);
    doc->root()->add(actorsGroup);

    struct Frame
    {
        std::string kind;
        std::string text;
        double top = 0;
        std::vector<std::pair<double, std::string>> sections;
    };
    std::vector<Frame> blockStack;
    std::map<std::string, std::vector<double>> activationStack;
    for (const auto &a : actors) activationStack[a.first] = {};
    struct Active
    {
        std::string actor;
        double start;
        double end;
        int depth;
    };
    std::vector<Active> activations;
    struct BoxFrame
    {
        std::string label;
        std::string colour;
    };
    std::vector<BoxFrame> boxStack;
    std::vector<BoxFrame> boxes;
    int counter = 0;

    for (const SeqEvent &ev : events) {
        switch (ev.kind) {
        case SeqEvent::Message: {
            ++counter;
            const SeqMessage &message = ev.message;
            const SeqActor *source = actorById[message.source];
            const SeqActor *target = actorById[message.target];
            const std::vector<std::string> lines = measurer->splitLines(message.text);
            const std::string colour = theme.get("signalColor", "#333");
            std::map<std::string, std::string> attrs;
            attrs["class"] = "message-line message-" + message.style;
            attrs["fill"] = "none";
            if (message.style == "dotted") attrs["stroke-dasharray"] = "2 2";
            if (!message.endHead.empty()) {
                attrs["marker-end"] = markerDefs(*doc, "sequence-" + message.endHead, colour,
                        markerKinds().count(message.endHead) ? markerKinds().at(message.endHead) : "arrow");
            }
            if (!message.startHead.empty()) {
                attrs["marker-start"] = markerDefs(*doc, "sequence-" + message.startHead + "-start", colour,
                        markerKinds().count(message.startHead) ? markerKinds().at(message.startHead) : "arrow");
            }
            if (source->id == target->id) {
                const double width = std::max(60.0, textWidth(message.text, fontSize) + 20);
                const double topY = y;
                const double bottomY = y + messageMargin;
                Path path;
                path.M(source->x, topY);
                path.L(source->x + width, topY);
                path.L(source->x + width, bottomY);
                path.L(source->x + 2, bottomY);
                attrs["d"] = path.str();
                messagesGroup->add([&] {
                    auto el = std::make_shared<El>("path");
                    for (const auto &a : attrs) el->attr(a.first, a.second);
                    return el;
                }());
                if (!lines.empty()) {
                    const double labelX = source->x + width + kSelfLabelGap;
                    messagesGroup->add(textElement(lines, labelX, (topY + bottomY) / 2, "start", fontSize,
                            1.25, "message-text"));
                    seen(source->x, labelX + textWidth(message.text, fontSize));
                }
                y = bottomY + messageMargin / 2;
            } else {
                const bool srcActive = !activationStack[message.source].empty();
                const bool tgtActive = !activationStack[message.target].empty();
                double left = srcActive ? kActivationWidth / 2 : 0.0;
                double right = tgtActive ? kActivationWidth / 2 : 0.0;
                const double direction = target->x > source->x ? 1 : -1;
                const double x1 = source->x + left * direction;
                const double x2 = target->x - right * direction;
                const double lineH = fontSize * kMessageLineRatio;
                const double total = std::max<int>(1, static_cast<int>(lines.size())) * lineH;
                y = y + lineH + total + boxMargin;
                Path path;
                path.M(x1, y);
                path.L(x2, y);
                attrs["d"] = path.str();
                messagesGroup->add([&] {
                    auto el = std::make_shared<El>("path");
                    for (const auto &a : attrs) el->attr(a.first, a.second);
                    return el;
                }());
                if (!lines.empty()) {
                    const double centre = (y - 13.0 - total / 2 + lineH / 2 - fontSize * 0.34);
                    messagesGroup->add(textElement(lines, (x1 + x2) / 2, centre, "middle", fontSize,
                            kMessageLineRatio, "message-text"));
                }
            }
            if (autonumber) {
                auto circle = std::make_shared<El>("circle");
                circle->attr("cx", source->x);
                circle->attr("cy", y);
                circle->attr("r", 9);
                circle->attr("class", "sequence-number-bkg");
                messagesGroup->add(circle);
                messagesGroup->add(textElement({ std::to_string(counter) }, source->x, y, "middle",
                        fontSize * 0.75, 1.25, "sequence-number"));
            }
            if (message.activate) activationStack[message.target].push_back(y - messageMargin / 2);
            if (message.deactivate && !activationStack[message.source].empty()) {
                const double start = activationStack[message.source].back();
                activationStack[message.source].pop_back();
                activations.push_back({ message.source, start, y,
                        static_cast<int>(activationStack[message.source].size()) });
            }
            break;
        }
        case SeqEvent::Activation: {
            if (ev.activation.start) {
                activationStack[ev.activation.actor].push_back(y);
            } else if (!activationStack[ev.activation.actor].empty()) {
                const double start = activationStack[ev.activation.actor].back();
                activationStack[ev.activation.actor].pop_back();
                activations.push_back({ ev.activation.actor, start, y,
                        static_cast<int>(activationStack[ev.activation.actor].size()) });
            }
            break;
        }
        case SeqEvent::Note: {
            const SeqNote &noteEv = ev.note;
            std::vector<const SeqActor *> noteActors;
            for (const std::string &a : noteEv.actors) {
                if (actorById.count(a)) noteActors.push_back(actorById[a]);
            }
            if (noteActors.empty()) break;
            const std::vector<std::string> lines = measurer->splitLines(noteEv.text);
            double textW = 0;
            for (const std::string &ln : lines) textW = std::max(textW, measurer->width(ln, fontSize));
            const double height = textHeight(noteEv.text) + 2 * kNoteMargin / 2;
            y += boxMargin;
            double nx, centre, width;
            if (noteEv.placement == "over") {
                double leftX = noteActors[0]->x - noteActors[0]->width / 2;
                double rightX = noteActors[0]->x + noteActors[0]->width / 2;
                for (const SeqActor *a : noteActors) {
                    leftX = std::min(leftX, a->x - a->width / 2);
                    rightX = std::max(rightX, a->x + a->width / 2);
                }
                centre = (leftX + rightX) / 2;
                width = std::max(textW + 2 * kNoteMargin, rightX - leftX);
                nx = centre - width / 2;
            } else if (noteEv.placement == "left") {
                width = textW + 2 * kNoteMargin;
                nx = noteActors[0]->x - noteActors[0]->width / 2 - width - 8;
            } else {
                width = textW + 2 * kNoteMargin;
                nx = noteActors[0]->x + noteActors[0]->width / 2 + 8;
            }
            seen(nx, nx + width);
            auto group = std::make_shared<El>("g");
            group->attr("class", "note");
            auto rect = std::make_shared<El>("rect");
            rect->attr("x", nx);
            rect->attr("y", y);
            rect->attr("width", width);
            rect->attr("height", height);
            rect->attr("rx", 2);
            rect->attr("ry", 2);
            rect->attr("class", "note-shape");
            group->add(rect);
            group->add(textElement(lines, nx + width / 2, y + height / 2, "middle", fontSize, 1.25, "note-text"));
            notesGroup->add(group);
            y = y + height + boxMargin;
            break;
        }
        case SeqEvent::BlockStart:
            y += boxMargin;
            blockStack.push_back({ ev.blockStart.kind, ev.blockStart.text, y, {} });
            y += fontSize * 1.6 + boxMargin;
            break;
        case SeqEvent::BlockSection:
            if (!blockStack.empty()) {
                y += boxMargin;
                blockStack.back().sections.push_back({ y, ev.blockSection.text });
                y += fontSize * 1.4;
            }
            break;
        case SeqEvent::BlockEnd:
            if (!blockStack.empty()) {
                Frame frame = blockStack.back();
                blockStack.pop_back();
                y += boxMargin;
                // draw block
                const double left = order.front().x - order.front().width / 2 - 8
                        - static_cast<double>(blockStack.size()) * 6;
                const double right = order.back().x + order.back().width / 2 + 8
                        + static_cast<double>(blockStack.size()) * 6;
                seen(left, right);
                auto group = std::make_shared<El>("g");
                group->attr("class", "block block-" + frame.kind);
                auto rect = std::make_shared<El>("rect");
                rect->attr("x", left);
                rect->attr("y", frame.top);
                rect->attr("width", right - left);
                rect->attr("height", y - frame.top);
                rect->attr("class", "block-rect");
                rect->attr("fill", "none");
                group->add(rect);
                const std::string label = frame.kind;
                const double labelW = measurer->width(label, fontSize * 0.8, true) + 16;
                auto poly = std::make_shared<El>("polygon");
                poly->attr("points", fmt(left) + "," + fmt(frame.top) + " " + fmt(left + labelW) + ","
                                + fmt(frame.top) + " " + fmt(left + labelW) + "," + fmt(frame.top + 18) + " "
                                + fmt(left + labelW - 8) + "," + fmt(frame.top + 24) + " " + fmt(left) + ","
                                + fmt(frame.top + 24));
                poly->attr("class", "block-label-box");
                group->add(poly);
                group->add(textElement({ label }, left + 8, frame.top + 12, "start", fontSize * 0.8, 1.25,
                        "block-kind"));
                if (!frame.text.empty()) {
                    group->add(textElement({ "[" + frame.text + "]" }, (left + right) / 2 + labelW / 2,
                            frame.top + 12, "middle", fontSize * 0.9, 1.25, "block-text"));
                }
                for (const auto &section : frame.sections) {
                    auto line = std::make_shared<El>("line");
                    line->attr("x1", left);
                    line->attr("y1", section.first - boxMargin);
                    line->attr("x2", right);
                    line->attr("y2", section.first - boxMargin);
                    line->attr("class", "block-divider");
                    group->add(line);
                    if (!section.second.empty()) {
                        group->add(textElement({ "[" + section.second + "]" }, (left + right) / 2, section.first,
                                "middle", fontSize * 0.9, 1.25, "block-text"));
                    }
                }
                blocksGroup->add(group);
                y += boxMargin;
            }
            break;
        case SeqEvent::BoxStart:
            boxStack.push_back({ ev.boxStart.label, ev.boxStart.colour });
            break;
        case SeqEvent::BoxEnd:
            if (!boxStack.empty()) {
                boxes.push_back(boxStack.back());
                boxStack.pop_back();
            }
            break;
        }
    }

    // Left-open blocks and activations still get drawn.
    for (auto it = blockStack.rbegin(); it != blockStack.rend(); ++it) {
        y += boxMargin;
        const double left = order.front().x - order.front().width / 2 - 8;
        const double right = order.back().x + order.back().width / 2 + 8;
        seen(left, right);
        auto group = std::make_shared<El>("g");
        group->attr("class", "block block-" + it->kind);
        auto rect = std::make_shared<El>("rect");
        rect->attr("x", left);
        rect->attr("y", it->top);
        rect->attr("width", right - left);
        rect->attr("height", y - it->top);
        rect->attr("class", "block-rect");
        rect->attr("fill", "none");
        group->add(rect);
        auto poly = std::make_shared<El>("polygon");
        const double labelW = measurer->width(it->kind, fontSize * 0.8, true) + 16;
        poly->attr("points", fmt(left) + "," + fmt(it->top) + " " + fmt(left + labelW) + "," + fmt(it->top) + " "
                        + fmt(left + labelW) + "," + fmt(it->top + 18) + " " + fmt(left + labelW - 8) + ","
                        + fmt(it->top + 24) + " " + fmt(left) + "," + fmt(it->top + 24));
        poly->attr("class", "block-label-box");
        group->add(poly);
        group->add(textElement({ it->kind }, left + 8, it->top + 12, "start", fontSize * 0.8, 1.25, "block-kind"));
        blocksGroup->add(group);
    }
    for (auto &entry : activationStack) {
        while (!entry.second.empty()) {
            const double start = entry.second.back();
            entry.second.pop_back();
            activations.push_back({ entry.first, start, y, static_cast<int>(entry.second.size()) });
        }
    }

    double bottom = y + boxMargin * 2;
    const double lifelineBottom = bottom;
    if (mirrorActors) bottom += actorHeight;

    for (const SeqActor &a : order) {
        auto line = std::make_shared<El>("line");
        line->attr("x1", a.x);
        line->attr("y1", lifelineTop);
        line->attr("x2", a.x);
        line->attr("y2", lifelineBottom);
        line->attr("class", "actor-line");
        lifelinesGroup->add(line);
    }
    for (const Active &act : activations) {
        const SeqActor *a = actorById[act.actor];
        auto rect = std::make_shared<El>("rect");
        rect->attr("x", a->x - kActivationWidth / 2 + act.depth * 4);
        rect->attr("y", act.start);
        rect->attr("width", kActivationWidth);
        rect->attr("height", std::max(act.end - act.start, 4.0));
        rect->attr("class", "activation");
        activationsGroup->add(rect);
    }
    auto drawActor = [&](const SeqActor &a, double yy) {
        auto group = std::make_shared<El>("g");
        group->attr("class", "actor");
        group->attr("id", "actor-" + a.id);
        const std::vector<std::string> lines = measurer->splitLines(a.label);
        if (a.kind == "actor") {
            const double cy = yy + 18;
            auto circle = std::make_shared<El>("circle");
            circle->attr("cx", a.x);
            circle->attr("cy", cy);
            circle->attr("r", 10);
            circle->attr("class", "actor-shape");
            group->add(circle);
            Path path;
            path.M(a.x, cy + 10).V(cy + 30);
            path.M(a.x - 14, cy + 18).L(a.x + 14, cy + 18);
            path.M(a.x, cy + 30).L(a.x - 12, cy + 46);
            path.M(a.x, cy + 30).L(a.x + 12, cy + 46);
            auto pel = std::make_shared<El>("path");
            pel->attr("d", path.str());
            pel->attr("class", "actor-shape");
            pel->attr("fill", "none");
            group->add(pel);
            group->add(textElement(lines, a.x, yy + actorHeight + 4, "middle", actorFontSize, 1.25, "actor-label"));
        } else {
            auto rect = std::make_shared<El>("rect");
            rect->attr("x", a.x - a.width / 2);
            rect->attr("y", yy);
            rect->attr("width", a.width);
            rect->attr("height", actorHeight);
            rect->attr("rx", 3);
            rect->attr("ry", 3);
            rect->attr("class", "actor-shape");
            group->add(rect);
            group->add(textElement(lines, a.x, yy + actorHeight / 2, "middle", actorFontSize, 1.25, "actor-label"));
        }
        actorsGroup->add(group);
    };
    for (const SeqActor &a : order) {
        drawActor(a, top);
        if (mirrorActors) drawActor(a, lifelineBottom);
    }
    for (const auto &box : boxes) {
        if (box.label.empty()) continue;
        auto rect = std::make_shared<El>("rect");
        rect->attr("x", 0);
        rect->attr("y", top - 8);
        rect->attr("width", totalWidth);
        rect->attr("height", lifelineBottom - top + 16);
        rect->attr("fill", box.colour);
        rect->attr("class", "box-rect");
        boxesGroup->add(rect);
    }

    seen(0.0, totalWidth);
    const double width = maxX - minX;
    if (hasTitle) {
        doc->root()->add(textElement({ title }, minX + width / 2, 16, "middle", 18, 1.25, "diagram-title",
                "font-size:18px;font-weight:bold;fill:" + theme.get("titleColor", "#333") + ";"));
    }
    if (minX < 0) {
        doc->root()->attr("transform", "translate(" + fmt(-minX) + ", 0)");
    }
    return finish(ctx, *doc, width, bottom, marginX, marginY);
}

} // namespace tqtw_mermaid2svg
