/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2sstate.h"

#include "tqtw_m2sdagre.h"
#include "tqtw_m2sdagrecommon.h"
#include "tqtw_m2serrors.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>
#include <set>
#include <sstream>

namespace tqtw_mermaid2svg {

namespace {

std::string trim(const std::string &s);
std::string lower(std::string s);
bool startsWith(const std::string &s, const std::string &p);
double asNum(const JsonValue &v, double fallback);

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
double asNum(const JsonValue &v, double fallback)
{
    if (v.isNull()) return fallback;
    if (v.type() == JsonValue::Number) return v.toNumber();
    return asNumber(v.toString(), fallback);
}

const std::map<std::string, std::string> &specials()
{
    static const std::map<std::string, std::string> m = { { "fork", "fork" }, { "join", "join" }, { "choice", "choice" } };
    return m;
}

} // namespace

StateDiagram::StateDiagram(const Source &source, Diagnostics *diagnostics)
{
    this->source = source;
    this->diagnostics = diagnostics;
    diagramId = "state";
    role = "stateDiagram";
    configKey = "state";
    hasTitle = source.hasTitle;
    title = source.title;
    hasAccDescr = source.hasAccDescr;
    accDescr = source.accDescr;
}

namespace {

struct ParseState
{
    std::vector<std::string> stack; // a "" entry means an ignored scope
    int pseudo = 0;
};

std::string parentOf(const ParseState &ps)
{
    if (ps.stack.empty()) return std::string();
    return ps.stack.back();
}

std::string stateName(StateDiagram &d, ParseState &ps, const std::string &nameIn, int line, Diagnostics &)
{
    const std::string name = unquote(trim(nameIn));
    if (name.empty()) {
        throw ParseError("expected a state name", line, 1);
    }
    if (name == "[*]") {
        throw ParseError("pseudo-state [*] must appear in a transition", line, 1);
    }
    auto it = d.states.find(name);
    if (it == d.states.end()) {
        State s;
        s.id = name;
        s.label = name;
        s.parent = parentOf(ps);
        s.hasParent = !ps.stack.empty();
        s.line = line;
        d.stateOrder.push_back(name);
        return d.states.emplace(name, s).first->first;
    }
    return it->first;
}

std::string endpoint(StateDiagram &d, ParseState &ps, const std::string &nameIn, int line, bool isTarget)
{
    if (trim(nameIn) != "[*]") {
        return stateName(d, ps, nameIn, line, *d.diagnostics);
    }
    const std::string scope = ps.stack.empty() ? "root" : ps.stack.back();
    const std::string kind = isTarget ? "end" : "start";
    const std::string stateId = "__" + kind + "_" + scope;
    if (!d.states.count(stateId)) {
        State s;
        s.id = stateId;
        s.kind = kind;
        s.parent = parentOf(ps);
        s.hasParent = !ps.stack.empty();
        s.line = line;
        d.stateOrder.push_back(stateId);
        d.states[stateId] = s;
    }
    return stateId;
}

} // namespace

std::shared_ptr<StateDiagram> StateDiagram::parse(const Source &source, Diagnostics *diagnostics)
{
    auto d = std::make_shared<StateDiagram>(source, diagnostics);
    auto &diag = *diagnostics;
    const auto header = firstLine(source);
    {
        std::smatch match;
        const std::regex re(R"(\b(TB|TD|BT|LR|RL)\b)");
        if (std::regex_search(header.first, match, re)) {
            const std::string dir = match[1].str();
            d->direction = dir == "TD" ? "TB" : dir;
        }
    }

    ParseState ps;
    bool noteBuffering = false;
    std::vector<std::string> noteBuffer;
    std::string noteTarget, notePlacement;
    int noteLine = 0;

    for (const SourceLine &line : iterLines(source)) {
        const std::string text = line.stripped();
        if (noteBuffering) {
            if (lower(text) == "end note") {
                StateNote n;
                n.target = noteTarget;
                n.placement = notePlacement;
                std::string joined;
                for (std::size_t i = 0; i < noteBuffer.size(); ++i) {
                    if (i) joined += "\n";
                    joined += noteBuffer[i];
                }
                n.text = joined;
                n.line = noteLine;
                d->notes.push_back(n);
                noteBuffering = false;
                noteBuffer.clear();
            } else {
                noteBuffer.push_back(text);
            }
            continue;
        }
        if (consumeCommon(*d, line)) continue;
        if (text == "}") {
            if (!ps.stack.empty()) ps.stack.pop_back();
            else diag.error("'}' without a matching '{'", line.number, 1, "block");
            continue;
        }
        if (startsWith(lower(text), "direction ")) {
            std::string dir = trim(text.substr(std::string("direction").size()));
            std::transform(dir.begin(), dir.end(), dir.begin(), [](unsigned char c) { return std::toupper(c); });
            d->direction = dir == "TD" ? "TB" : dir;
            continue;
        }
        if (startsWith(lower(text), "note")) {
            std::smatch match;
            const std::regex re(R"(^note\s+(left of|right of|over)?\s*([^:]*?)\s*(?::\s*(.*))?$)", std::regex::icase);
            if (!std::regex_match(text, match, re)) {
                throw ParseError("could not parse note", line.number, 1);
            }
            std::string placement = match[1].matched ? lower(match[1].str()) : "right of";
            const std::size_t of = placement.find(" of");
            if (of != std::string::npos) placement = placement.substr(0, of);
            const std::string target = trim(match[2].str());
            const std::string body = match[3].matched ? match[3].str() : "";
            if (!body.empty()) {
                StateNote n;
                n.target = target;
                n.placement = placement;
                n.text = trim(body);
                n.line = line.number;
                d->notes.push_back(n);
            } else {
                noteBuffering = true;
                noteBuffer.clear();
                noteTarget = target;
                notePlacement = placement;
                noteLine = line.number;
            }
            continue;
        }
        if (text == "--") {
            continue;
        }
        if (text.find("-->") != std::string::npos) {
            // transition
            std::string body = text;
            std::string label;
            const std::size_t arrowEnd = body.rfind("-->");
            if (arrowEnd != std::string::npos) {
                const std::size_t tailAt = body.find(':', arrowEnd);
                if (tailAt != std::string::npos) {
                    label = trim(body.substr(tailAt + 1));
                    body = trim(body.substr(0, tailAt));
                }
            } else if (body.find(':') != std::string::npos) {
                label = trim(body.substr(body.find(':') + 1));
                body = trim(body.substr(0, body.find(':')));
            }
            // split on -->
            std::vector<std::string> parts;
            std::size_t start = 0;
            while (true) {
                const std::size_t pos = body.find("-->", start);
                if (pos == std::string::npos) {
                    parts.push_back(body.substr(start));
                    break;
                }
                parts.push_back(body.substr(start, pos - start));
                start = pos + 3;
            }
            if (parts.size() < 2) {
                throw ParseError("could not parse transition", line.number, 1);
            }
            std::string previous = endpoint(*d, ps, parts[0], line.number, false);
            for (std::size_t index = 1; index < parts.size(); ++index) {
                const bool isLast = index == parts.size() - 1;
                const std::string target = endpoint(*d, ps, parts[index], line.number, true);
                Transition t;
                t.source = previous;
                t.target = target;
                t.label = isLast ? label : "";
                t.line = line.number;
                d->transitions.push_back(t);
                previous = target;
            }
            continue;
        }
        if (startsWith(lower(text), "state ")) {
            std::string rest = trim(text.substr(std::string("state").size()));
            const bool opens = !rest.empty() && rest.back() == '{';
            if (opens) rest = trim(rest.substr(0, rest.size() - 1));
            bool declared = false;
            {
                std::smatch match;
                const std::regex re(R"RE(^"(.*)"\s+as\s+(\S+)$)RE");
                if (std::regex_match(rest, match, re)) {
                    const std::string id = stateName(*d, ps, match[2].str(), line.number, diag);
                    d->states[id].label = match[1].str();
                    declared = true;
                }
            }
            if (!declared) {
                std::smatch match;
                const std::regex re(R"(^(\S+)\s*<<\s*(\w+)\s*>>$)");
                if (std::regex_match(rest, match, re)) {
                    const std::string id = stateName(*d, ps, match[1].str(), line.number, diag);
                    const std::string kind = lower(match[2].str());
                    if (specials().count(kind)) d->states[id].kind = specials().at(kind);
                    else diag.warn("unknown state stereotype <<" + match[2].str() + ">>", line.number, 1,
                            "stereotype");
                    declared = true;
                }
            }
            if (!declared && rest.find(':') != std::string::npos) {
                const std::string name = trim(rest.substr(0, rest.find(':')));
                const std::string id = stateName(*d, ps, name, line.number, diag);
                d->states[id].label = trim(rest.substr(rest.find(':') + 1));
                declared = true;
            }
            if (!declared) {
                const std::string id = stateName(*d, ps, rest, line.number, diag);
                (void)id;
                declared = true;
            }
            if (opens) {
                // Mark composite only when a real state was declared.
                if (!rest.empty() && trim(rest) != "[*]") {
                    // find the state we just made
                    // simplest: the last state whose id matches the parsed name
                }
                // Re-derive the id to mark composite: parse again is awkward, so
                // recompute from the first token.
                std::string name;
                {
                    std::smatch match;
                    const std::regex re(R"RE(^"?(.*?)"?\s+as\s+(\S+)$)RE");
                    if (std::regex_match(rest, match, re)) name = match[2].str();
                    else {
                        const std::size_t sp = rest.find_first_of(" <:");
                        name = sp == std::string::npos ? rest : rest.substr(0, sp);
                    }
                }
                const std::string id = stateName(*d, ps, name, line.number, diag);
                d->states[id].composite = true;
                ps.stack.push_back(id);
            }
            continue;
        }
        if (text.find(':') != std::string::npos) {
            const std::string name = trim(text.substr(0, text.find(':')));
            const std::string label = trim(text.substr(text.find(':') + 1));
            if (unquote(name) == "[*]") {
                diag.info("ignoring a bare '[*]' declaration", line.number, 1, "pseudo-state");
            } else {
                try {
                    const std::string id = stateName(*d, ps, name, line.number, diag);
                    d->states[id].label = label;
                } catch (const ParseError &) {
                    // ignored
                }
            }
            continue;
        }
        if (!text.empty()) {
            try {
                if (unquote(trim(text)) == "[*]") {
                    diag.info("ignoring a bare '[*]' declaration", line.number, 1, "pseudo-state");
                } else {
                    stateName(*d, ps, text, line.number, diag);
                }
            } catch (const ParseError &) {
                // ignored
            }
        }
    }

    // validate
    if (d->states.empty()) {
        diag.warn("state diagram has no states", 0, 0, "empty");
    }
    for (const StateNote &note : d->notes) {
        if (!note.target.empty() && !d->states.count(note.target)) {
            diag.warn("note refers to unknown state '" + note.target + "'", note.line, 1, "unknown-state");
        }
    }
    std::set<std::string> reachable;
    for (const Transition &t : d->transitions) reachable.insert(t.target);
    for (const auto &e : d->states) {
        const State &s = e.second;
        if (s.kind == "normal" && !s.composite && !reachable.count(s.id)) {
            bool anySource = false;
            for (const Transition &t : d->transitions) {
                if (t.source == s.id) { anySource = true; break; }
            }
            if (!anySource) {
                diag.info("state '" + s.id + "' has no transitions", s.line, 1, "isolated-state");
            }
        }
    }
    return d;
}

namespace {

constexpr double kPadding = 8.0;
constexpr double kMarkerRadius = 7.0;
constexpr double kForkWidth = 70.0;
constexpr double kForkHeight = 10.0;
constexpr double kChoiceSize = 40.0;
constexpr double kNotePadding = 15.0;
constexpr double kCompositeTitleHeight = 30.0;
constexpr double kCompositeTitlePad = 12.0;

std::string shapeFor(const State *state)
{
    if (state == nullptr) return "rect";
    if (state->kind == "start" || state->kind == "end") return "circle";
    if (state->kind == "choice") return "diamond";
    return "rect";
}

} // namespace

RenderResult StateDiagram::render(const RenderContext &ctx)
{
    const Theme &theme = ctx.theme;
    const JsonValue block = ctx.sub("state");
    const double fontSize = theme.fontSize();
    std::shared_ptr<TextMeasurer> measurer = ctx.measurer;
    const double padding = asNum(block["padding"], kPadding);
    const double nodeSpacing = asNum(block["nodeSpacing"], 50);
    const double rankSpacing = asNum(block["rankSpacing"], 50);
    const double diagramPadding = asNum(block["diagramPadding"], 8);
    const double wrappingWidth = ctx.num("flowchart", "wrappingWidth", 200);

    auto doc = newDocument(ctx);
    // css
    {
        std::ostringstream css;
        const std::string line = theme.get("transitionColor", theme.get("lineColor", "#333"));
        css << "#{id} .state-rect { fill: " << theme.get("stateBkg", "#ECECFF") << "; stroke: "
            << theme.get("nodeBorder", "#9370DB") << "; stroke-width: 1px; }\n";
        css << "#{id} .state-label, #{id} .state-label tspan { fill: " << theme.get("stateLabelColor", "#333")
            << "; }\n";
        css << "#{id} .note-box { fill: " << theme.get("noteBkgColor", "#fff5ad") << "; stroke: "
            << theme.get("noteBorderColor", "#aaaa33") << "; stroke-width: 1px; }\n";
        css << "#{id} .note-label, #{id} .note-label tspan { fill: " << theme.get("noteTextColor", "#333") << "; }\n";
        css << "#{id} .state-start, #{id} .state-end-inner, #{id} .state-bar { fill: "
            << theme.get("specialStateColor", "#333") << "; stroke: none; }\n";
        css << "#{id} .state-end-outer { fill: none; stroke: " << theme.get("specialStateColor", "#333")
            << "; stroke-width: 2px; }\n";
        css << "#{id} .state-choice { fill: " << theme.get("mainBkg", "#ECECFF") << "; stroke: "
            << theme.get("nodeBorder", "#9370DB") << "; }\n";
        css << "#{id} .transition { stroke: " << line << "; stroke-width: 1.5px; fill: none; }\n";
        css << "#{id} .transition-label, #{id} .transition-label tspan { fill: "
            << theme.get("transitionLabelColor", "#333") << "; }\n";
        css << "#{id} .transition-label-bkg { fill: " << theme.get("edgeLabelBackground", "#e8e8e8")
            << "; stroke: none; }\n";
        css << "#{id} .composite-rect { fill: " << theme.get("compositeBackground", "#ECECFF") << "; stroke: "
            << theme.get("compositeBorder", "#9370DB") << "; stroke-width: 1px; }\n";
        css << "#{id} .composite-divider { stroke: " << theme.get("compositeBorder", "#9370DB") << "; }\n";
        css << "#{id} .composite-label, #{id} .composite-label tspan { fill: "
            << theme.get("primaryTextColor", "#333") << "; font-weight: bold; }";
        doc->addStyle(css.str());
    }

    DagreGraph g;
    std::map<std::string, std::vector<std::string>> textLines;
    auto sizeOf = [&](const State &state, std::vector<std::string> *lines) -> std::pair<double, double> {
        if (state.kind == "start" || state.kind == "end") {
            *lines = {};
            return { kMarkerRadius * 2, kMarkerRadius * 2 };
        }
        if (state.kind == "fork" || state.kind == "join") {
            *lines = {};
            const bool horizontal = direction == "LR" || direction == "RL";
            return horizontal ? std::make_pair(kForkHeight, kForkWidth) : std::make_pair(kForkWidth, kForkHeight);
        }
        if (state.kind == "choice") {
            *lines = {};
            return { kChoiceSize, kChoiceSize };
        }
        TextBlock blk = measurer->block(state.label.empty() ? state.id : state.label, fontSize, false, wrappingWidth,
                HTML_LABEL_LINE_HEIGHT);
        *lines = blk.lines;
        return { blk.width() + 2 * padding, blk.height() + 2 * padding };
    };

    for (const std::string &sid : stateOrder) {
        const State &state = states.at(sid);
        if (state.composite) continue;
        std::vector<std::string> lines;
        const auto sz = sizeOf(state, &lines);
        textLines[state.id] = lines;
        DagreNode &n = g.addNode(state.id, sz.first, sz.second);
        if (state.hasParent) { n.parent = state.parent; n.hasParent = true; }
    }
    for (const std::string &sid : stateOrder) {
        const State &state = states.at(sid);
        if (!state.composite) continue;
        const std::string label = state.label.empty() ? state.id : state.label;
        DagreCluster &c = g.addCluster(state.id, label);
        if (state.hasParent) { c.parent = state.parent; c.hasParent = true; }
        c.labelHeight = kCompositeTitleHeight;
        c.minWidth = measurer->width(label, fontSize, true) + 2 * kCompositeTitlePad;
    }
    std::map<std::string, std::vector<std::string>> noteLines;
    for (std::size_t index = 0; index < notes.size(); ++index) {
        const StateNote &note = notes[index];
        if (note.target.empty() || !states.count(note.target)) continue;
        const std::string noteId = "__note" + std::to_string(index);
        TextBlock blk = measurer->block(note.text, fontSize, false, wrappingWidth, HTML_LABEL_LINE_HEIGHT);
        noteLines[noteId] = blk.lines;
        std::string anchor = note.target;
        std::string parent = states[note.target].hasParent ? states[note.target].parent : std::string();
        bool hasParent = states[note.target].hasParent;
        if (g.clusters.count(anchor)) {
            for (const auto &n : g.nodes) {
                const std::vector<std::string> chain = g.clusterChain(n.first);
                if (std::find(chain.begin(), chain.end(), anchor) != chain.end()) { anchor = n.first; break; }
            }
            parent = note.target;
            hasParent = true;
        }
        if (!g.nodes.count(anchor)) continue;
        DagreNode &n = g.addNode(noteId, blk.width() + 2 * kNotePadding, blk.height() + 2 * kNotePadding);
        if (hasParent) { n.parent = parent; n.hasParent = true; }
        DagreEdge &edge = g.addEdge(anchor, noteId);
        (void)edge;
    }
    for (const Transition &transition : transitions) {
        std::string source = transition.source;
        std::string target = transition.target;
        if (g.clusters.count(source)) {
            for (const auto &n : g.nodes) {
                const std::vector<std::string> chain = g.clusterChain(n.first);
                if (std::find(chain.begin(), chain.end(), source) != chain.end()) { source = n.first; break; }
            }
        }
        if (g.clusters.count(target)) {
            for (const auto &n : g.nodes) {
                const std::vector<std::string> chain = g.clusterChain(n.first);
                if (std::find(chain.begin(), chain.end(), target) != chain.end()) { target = n.first; break; }
            }
        }
        if (!g.nodes.count(source) || !g.nodes.count(target)) continue;
        DagreEdge &edge = g.addEdge(source, target);
        edge.labelWidth = transition.label.empty() ? 0 : measurer->width(transition.label, fontSize);
        edge.labelHeight = transition.label.empty() ? 0 : fontSize * HTML_LABEL_LINE_HEIGHT;
    }

    runLayout(g, direction, nodeSpacing, rankSpacing, 0.0);

    auto clustersGroup = std::make_shared<El>("g");
    clustersGroup->attr("class", "clusters");
    auto edgesGroup = std::make_shared<El>("g");
    edgesGroup->attr("class", "transitions");
    auto nodesGroup = std::make_shared<El>("g");
    nodesGroup->attr("class", "states");
    doc->root()->add(clustersGroup);
    doc->root()->add(edgesGroup);
    doc->root()->add(nodesGroup);

    for (const auto &e : noteLines) {
        auto it = g.nodes.find(e.first);
        if (it == g.nodes.end()) continue;
        const DagreNode &laid = it->second;
        auto group = std::make_shared<El>("g");
        group->attr("class", "state-note");
        const double left = laid.x - laid.width / 2;
        const double top = laid.y - laid.height / 2;
        const double fold = 12.0;
        auto path = std::make_shared<El>("path");
        path->attr("d", "M" + fmt(left) + "," + fmt(top) + " H" + fmt(left + laid.width - fold) + " L"
                        + fmt(left + laid.width) + "," + fmt(top + fold) + " V" + fmt(top + laid.height) + " H"
                        + fmt(left) + " Z");
        path->attr("class", "note-box");
        group->add(path);
        group->add(textElement(e.second, laid.x, laid.y, "middle", fontSize, HTML_LABEL_LINE_HEIGHT, "note-label"));
        nodesGroup->add(group);
    }

    std::vector<DagreCluster> clusters;
    for (const auto &e : g.clusters) clusters.push_back(e.second);
    std::sort(clusters.begin(), clusters.end(),
            [](const DagreCluster &a, const DagreCluster &b) { return a.width > b.width; });
    for (const DagreCluster &cluster : clusters) {
        if (!cluster.width) continue;
        auto group = std::make_shared<El>("g");
        group->attr("class", "cluster");
        auto rect = std::make_shared<El>("rect");
        rect->attr("x", cluster.x - cluster.width / 2);
        rect->attr("y", cluster.y - cluster.height / 2);
        rect->attr("width", cluster.width);
        rect->attr("height", cluster.height);
        rect->attr("rx", 8); rect->attr("ry", 8); rect->attr("class", "composite-rect");
        group->add(rect);
        group->add(textElement({ cluster.label }, cluster.x, cluster.y - cluster.height / 2 + fontSize, "middle",
                fontSize, 1.25, "composite-label"));
        auto line = std::make_shared<El>("line");
        line->attr("x1", cluster.x - cluster.width / 2);
        line->attr("y1", cluster.y - cluster.height / 2 + fontSize * 1.8);
        line->attr("x2", cluster.x + cluster.width / 2);
        line->attr("y2", cluster.y - cluster.height / 2 + fontSize * 1.8);
        line->attr("class", "composite-divider");
        group->add(line);
        clustersGroup->add(group);
    }

    struct LabelBox
    {
        double x, y, w, h;
    };
    std::vector<LabelBox> labelBoxes;
    auto freeLabelSlot = [&](double x, double y, double width, double height) {
        const double step = height + 4;
        for (int attempt = 0; attempt < 24; ++attempt) {
            bool clash = false;
            for (const LabelBox &b : labelBoxes) {
                if (std::fabs(x - b.x) < (width + b.w) / 2 && std::fabs(y - b.y) < (height + b.h) / 2) {
                    clash = true;
                    break;
                }
            }
            if (!clash) break;
            y += step;
        }
        labelBoxes.push_back({ x, y, width, height });
        return y;
    };

    for (const DagreEdge &edge : g.edges) {
        // find matching transition (edge.data holds the index we did not set; match by endpoints/label)
        const Transition *transition = nullptr;
        for (const Transition &t : transitions) {
            if ((t.source == edge.v || t.target == edge.w) && !t.label.empty()) { transition = &t; break; }
        }
        const State *sourceState = states.count(edge.v) ? &states.at(edge.v) : nullptr;
        const State *targetState = states.count(edge.w) ? &states.at(edge.w) : nullptr;
        const std::vector<Point> points = clipPoints(g, edge, shapeFor(sourceState), shapeFor(targetState));
        if (points.empty()) continue;
        if (edge.v.rfind("__note", 0) == 0 || edge.w.rfind("__note", 0) == 0) continue;
        const std::string colour = theme.get("transitionColor", theme.get("lineColor", "#333"));
        std::map<std::string, std::string> attrs;
        attrs["class"] = "transition";
        attrs["marker-end"] = markerDefs(*doc, "state-arrow", colour, "arrow");
        edgesGroup->add(edgePath(points, "basis", attrs));
        if (transition != nullptr && !transition->label.empty()) {
            const std::vector<std::string> lines = measurer->splitLines(transition->label);
            double width = 0;
            for (const std::string &ln : lines) width = std::max(width, measurer->width(ln, fontSize * 0.9));
            const double height = static_cast<double>(lines.size()) * fontSize * 1.2;
            const double labelY = freeLabelSlot(edge.labelX, edge.labelY, width + 8, height + 4);
            auto rect = std::make_shared<El>("rect");
            rect->attr("x", edge.labelX - width / 2 - 4);
            rect->attr("y", labelY - height / 2 - 2);
            rect->attr("width", width + 8);
            rect->attr("height", height + 4);
            rect->attr("class", "transition-label-bkg");
            rect->attr("rx", 2); rect->attr("ry", 2);
            edgesGroup->add(rect);
            edgesGroup->add(textElement(lines, edge.labelX, labelY, "middle", fontSize * 0.9, 1.25,
                    "transition-label"));
        }
    }

    for (const auto &e : states) {
        auto it = g.nodes.find(e.first);
        if (it == g.nodes.end()) continue;
        const DagreNode &laid = it->second;
        const State &state = e.second;
        auto group = std::make_shared<El>("g");
        group->attr("class", "state state-" + state.kind);
        group->attr("id", "state-" + state.id);
        if (state.kind == "start") {
            auto c = std::make_shared<El>("circle");
            c->attr("cx", laid.x); c->attr("cy", laid.y); c->attr("r", 8); c->attr("class", "state-start");
            group->add(c);
        } else if (state.kind == "end") {
            auto outer = std::make_shared<El>("circle");
            outer->attr("cx", laid.x); outer->attr("cy", laid.y); outer->attr("r", 8);
            outer->attr("class", "state-end-outer");
            group->add(outer);
            auto inner = std::make_shared<El>("circle");
            inner->attr("cx", laid.x); inner->attr("cy", laid.y); inner->attr("r", 5);
            inner->attr("class", "state-end-inner");
            group->add(inner);
        } else if (state.kind == "fork" || state.kind == "join") {
            auto rect = std::make_shared<El>("rect");
            rect->attr("x", laid.x - laid.width / 2);
            rect->attr("y", laid.y - laid.height / 2);
            rect->attr("width", laid.width);
            rect->attr("height", laid.height);
            rect->attr("rx", 3); rect->attr("ry", 3); rect->attr("class", "state-bar");
            group->add(rect);
        } else if (state.kind == "choice") {
            const double hw = laid.width / 2, hh = laid.height / 2;
            auto poly = std::make_shared<El>("polygon");
            poly->attr("points", fmt(laid.x) + "," + fmt(laid.y - hh) + " " + fmt(laid.x + hw) + "," + fmt(laid.y)
                            + " " + fmt(laid.x) + "," + fmt(laid.y + hh) + " " + fmt(laid.x - hw) + ","
                            + fmt(laid.y));
            poly->attr("class", "state-choice");
            group->add(poly);
        } else {
            auto rect = std::make_shared<El>("rect");
            rect->attr("x", laid.x - laid.width / 2);
            rect->attr("y", laid.y - laid.height / 2);
            rect->attr("width", laid.width);
            rect->attr("height", laid.height);
            rect->attr("rx", 8); rect->attr("ry", 8); rect->attr("class", "state-rect");
            group->add(rect);
            group->add(textElement(textLines[e.first], laid.x, laid.y, "middle", fontSize, 1.25, "state-label"));
        }
        nodesGroup->add(group);
    }

    double spill = 0;
    for (const LabelBox &b : labelBoxes) {
        spill = std::max(spill, b.y + b.h / 2 - g.height);
    }
    return finish(ctx, *doc, g.width, g.height + std::max(0.0, spill), diagramPadding);
}

} // namespace tqtw_mermaid2svg
