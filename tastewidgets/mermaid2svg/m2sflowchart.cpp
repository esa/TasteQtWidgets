/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2sflowchart.h"

#include "tqtw_m2sdagre.h"
#include "tqtw_m2serrors.h"
#include "tqtw_m2sintersect.h"
#include "tqtw_m2scurves.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <regex>
#include <set>

namespace tqtw_mermaid2svg {

using Point = std::pair<double, double>;

namespace {

std::string trim(const std::string &s);
std::string lower(std::string s);
bool startsWith(const std::string &s, const std::string &prefix);

const std::set<char> &idStop()
{
    static const std::set<char> stops = { ' ', '\t', '\n', '[', ']', '(', ')', '{', '}', '|', '<', '>', '=', '~', '&',
        ';', '"', '@', '^' };
    return stops;
}

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

bool startsWith(const std::string &s, const std::string &prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// --------------------------------------------------------------------------
// Link tokenizer (hand-rolled; the Python uses regexes with named groups)
// --------------------------------------------------------------------------

struct LinkInfo
{
    std::string kind = "normal";
    std::string startHead;
    std::string endHead = "arrow";
    int minlen = 1;
    std::string label;
};

//: Read a left head: `<`, or `o`/`x` when followed by `-`, `=`, `.`.
int readLeftHead(const std::string &t, std::size_t pos, std::string *head)
{
    if (pos >= t.size()) return 0;
    if (t[pos] == '<') { *head = "arrow"; return 1; }
    if ((t[pos] == 'o' || t[pos] == 'x') && pos + 1 < t.size()
            && (t[pos + 1] == '-' || t[pos + 1] == '=' || t[pos + 1] == '.')) {
        *head = t[pos] == 'o' ? "circle" : "cross";
        return 1;
    }
    return 0;
}

int readRightHead(const std::string &t, std::size_t pos, std::string *head)
{
    if (pos >= t.size()) return 0;
    if (t[pos] == '>') { *head = "arrow"; return 1; }
    if (t[pos] == 'o') { *head = "circle"; return 1; }
    if (t[pos] == 'x') { *head = "cross"; return 1; }
    return 0;
}

//: Try to match the full link form at pos. Returns the end index, or npos.
std::size_t matchArrowFull(const std::string &t, std::size_t pos, LinkInfo *info)
{
    const std::size_t start = pos;
    std::string lhead;
    const int lh = readLeftHead(t, pos, &lhead);
    pos += lh;

    std::string kind, body;
    if (t.compare(pos, 3, "-.-") == 0 || (t.compare(pos, 2, "-.") == 0 && t.size() > pos + 2)) {
        // dotted: -\.+-
        if (t[pos] == '-' && pos + 1 < t.size() && t[pos + 1] == '.') {
            std::size_t j = pos + 1;
            while (j < t.size() && t[j] == '.') ++j;
            if (j < t.size() && t[j] == '-') {
                kind = "dotted";
                body = t.substr(pos, j + 1 - pos);
                pos = j + 1;
            }
        }
    }
    if (kind.empty()) {
        if (t.compare(pos, 2, "==") == 0) {
            std::size_t j = pos;
            while (j < t.size() && t[j] == '=') ++j;
            kind = "thick";
            body = t.substr(pos, j - pos);
            pos = j;
        } else if (t.compare(pos, 3, "~~~") == 0) {
            std::size_t j = pos;
            while (j < t.size() && t[j] == '~') ++j;
            kind = "invisible";
            body = t.substr(pos, j - pos);
            pos = j;
        } else if (t.compare(pos, 2, "--") == 0) {
            std::size_t j = pos;
            while (j < t.size() && t[j] == '-') ++j;
            kind = "normal";
            body = t.substr(pos, j - pos);
            pos = j;
        }
    }
    if (kind.empty()) {
        return std::string::npos;
    }

    std::string rhead;
    const int rh = readRightHead(t, pos, &rhead);
    pos += rh;

    // _is_complete: rhead present, or dot/invis, or thick/dash body len >=3
    bool complete = !rhead.empty();
    if (!complete && (kind == "dotted" || kind == "invisible")) {
        complete = true;
    }
    if (!complete) {
        const std::string b = kind == "thick" ? body : body;
        complete = b.size() >= 3;
    }
    if (!complete) {
        return std::string::npos;
    }
    info->kind = kind;
    info->startHead = lhead;
    info->endHead = rhead;
    if (kind == "dotted") {
        info->minlen = std::max(1, static_cast<int>(std::count(body.begin(), body.end(), '.')));
    } else if (!rhead.empty()) {
        info->minlen = std::max(1, static_cast<int>(body.size()) - 1);
    } else {
        info->minlen = std::max(1, static_cast<int>(body.size()) - 2);
    }
    (void)start;
    return pos;
}

//: Try to match the opening half of an inline-label link: `--`, `==`, `-.`.
std::size_t matchArrowOpen(const std::string &t, std::size_t pos, std::string *kind, std::string *lhead)
{
    std::string head;
    const int lh = readLeftHead(t, pos, &head);
    std::size_t p = pos + lh;
    // not followed by [-=.>ox~]
    auto bad = [&](std::size_t q) {
        return q < t.size() && (t[q] == '-' || t[q] == '=' || t[q] == '.' || t[q] == '>' || t[q] == 'o' || t[q] == 'x'
                || t[q] == '~');
    };
    if (t.compare(p, 2, "-.") == 0 && !bad(p + 2)) {
        *kind = "dot";
        *lhead = head;
        return p + 2;
    }
    if (t.compare(p, 2, "==") == 0 && !bad(p + 2)) {
        *kind = "thick";
        *lhead = head;
        return p + 2;
    }
    if (t.compare(p, 2, "--") == 0 && !bad(p + 2)) {
        *kind = "dash";
        *lhead = head;
        return p + 2;
    }
    return std::string::npos;
}

struct CloseMatch
{
    std::size_t start = 0;
    std::size_t end = 0;
    std::string body;
    std::string rhead;
};

//: The closer of `kind` after pos, ignoring quotes.
bool findClose(const std::string &t, std::size_t pos, const std::string &kind, CloseMatch *out)
{
    while (pos < t.size()) {
        if (t[pos] == '"') {
            // find closing quote
            std::size_t j = pos + 1;
            bool found = false;
            while (j < t.size()) {
                if (t[j] == '\\' && j + 1 < t.size()) { j += 2; continue; }
                if (t[j] == '"') { found = true; break; }
                ++j;
            }
            if (!found) return false;
            pos = j + 1;
            continue;
        }
        if (kind == "dash" && t.compare(pos, 2, "--") == 0) {
            std::size_t j = pos;
            while (j < t.size() && t[j] == '-') ++j;
            std::string rhead;
            std::size_t e = j;
            if (e < t.size() && (t[e] == '>' || t[e] == 'o' || t[e] == 'x')) {
                rhead = t[e] == '>' ? "arrow" : (t[e] == 'o' ? "circle" : "cross");
                ++e;
            }
            if (j - pos >= 2 && (e > j)) {
                out->start = pos; out->end = e; out->body = t.substr(pos, j - pos); out->rhead = rhead;
                return true;
            }
        }
        if (kind == "thick" && t.compare(pos, 2, "==") == 0) {
            std::size_t j = pos;
            while (j < t.size() && t[j] == '=') ++j;
            std::string rhead;
            std::size_t e = j;
            if (e < t.size() && (t[e] == '>' || t[e] == 'o' || t[e] == 'x')) {
                rhead = t[e] == '>' ? "arrow" : (t[e] == 'o' ? "circle" : "cross");
                ++e;
            }
            if (j - pos >= 2 && e > j) {
                out->start = pos; out->end = e; out->body = t.substr(pos, j - pos); out->rhead = rhead;
                return true;
            }
        }
        if (kind == "dot") {
            // -?\.+-
            std::size_t j = pos;
            if (j < t.size() && t[j] == '-') ++j;
            if (j < t.size() && t[j] == '.') {
                while (j < t.size() && t[j] == '.') ++j;
                if (j < t.size() && t[j] == '-') {
                    std::string rhead;
                    std::size_t e = j + 1;
                    if (e < t.size() && (t[e] == '>' || t[e] == 'o' || t[e] == 'x')) {
                        rhead = t[e] == '>' ? "arrow" : (t[e] == 'o' ? "circle" : "cross");
                        ++e;
                    }
                    out->start = pos; out->end = e; out->body = t.substr(pos, j + 1 - pos); out->rhead = rhead;
                    return true;
                }
            }
        }
        ++pos;
    }
    return false;
}

//: The `|label|` after a link; returns the new pos and sets label.
std::size_t readPipeLabel(const std::string &t, std::size_t pos, std::string *label)
{
    while (pos < t.size() && (t[pos] == ' ' || t[pos] == '\t')) ++pos;
    if (pos < t.size() && t[pos] == '|') {
        if (pos + 1 < t.size() && t[pos + 1] == '"') {
            std::size_t j = pos + 2;
            while (j < t.size() && t[j] != '"') ++j;
            if (j < t.size() && j + 1 < t.size() && t[j + 1] == '|') {
                *label = t.substr(pos + 2, j - (pos + 2));
                return j + 2;
            }
        }
        const std::size_t close = t.find('|', pos + 1);
        if (close != std::string::npos) {
            *label = unquote(trim(t.substr(pos + 1, close - pos - 1)));
            return close + 1;
        }
    }
    return pos;
}

bool readLink(const std::string &t, std::size_t pos, LinkInfo *info, std::size_t *end)
{
    LinkInfo full;
    const std::size_t fullEnd = matchArrowFull(t, pos, &full);
    if (fullEnd != std::string::npos) {
        std::size_t p = fullEnd;
        std::string label;
        p = readPipeLabel(t, p, &label);
        if (!label.empty()) {
            full.label = label;
        }
        *info = full;
        *end = p;
        return true;
    }
    std::string kind, lhead;
    const std::size_t openEnd = matchArrowOpen(t, pos, &kind, &lhead);
    if (openEnd != std::string::npos) {
        CloseMatch close;
        if (findClose(t, openEnd, kind, &close)) {
            LinkInfo info2;
            info2.kind = kind == "dot" ? "dotted" : (kind == "thick" ? "thick" : "normal");
            info2.startHead = lhead;
            info2.endHead = close.rhead;
            const std::string body = close.body;
            if (kind == "dot") {
                info2.minlen = std::max(1, static_cast<int>(std::count(body.begin(), body.end(), '.')));
            } else if (!close.rhead.empty()) {
                info2.minlen = std::max(1, static_cast<int>(body.size()) - 1);
            } else {
                info2.minlen = std::max(1, static_cast<int>(body.size()) - 2);
            }
            info2.label = unquote(trim(t.substr(openEnd, close.start - openEnd)));
            std::size_t p = close.end;
            std::string pipe;
            p = readPipeLabel(t, p, &pipe);
            if (!pipe.empty()) {
                info2.label = pipe;
            }
            *info = info2;
            *end = p;
            return true;
        }
    }
    return false;
}

// --------------------------------------------------------------------------
// Shape tables
// --------------------------------------------------------------------------

struct ShapeToken
{
    std::string opener;
    std::vector<std::string> closers;
    std::string shape;
};

const std::vector<ShapeToken> &shapeTokens()
{
    static const std::vector<ShapeToken> tokens = {
        { "(((", { ")))" }, "doublecircle" },
        { "((", { "))" }, "circle" },
        { "([", { "])" }, "stadium" },
        { "[[", { "]]" }, "subroutine" },
        { "[(", { ")]" }, "cylinder" },
        { "[/", { "/]", "\\]" }, "lean_right" },
        { "[\\", { "\\]", "/]" }, "lean_left" },
        { "{{", { "}}" }, "hexagon" },
        { "[", { "]" }, "rect" },
        { "(", { ")" }, "round" },
        { "{", { "}" }, "diamond" },
        { ">", { "]" }, "odd" },
    };
    return tokens;
}

std::string slantedShape(const std::string &shape, const std::string &text, std::size_t end,
        const std::vector<std::string> &closers)
{
    for (const std::string &closer : closers) {
        if (end >= closer.size() && text.compare(end - closer.size(), closer.size(), closer) == 0) {
            if (shape == "lean_right" && closer == "\\]") return "trapezoid";
            if (shape == "lean_left" && closer == "/]") return "inv_trapezoid";
            return shape;
        }
    }
    return shape;
}

std::size_t closingQuote(const std::string &t, std::size_t pos)
{
    std::size_t i = pos + 1;
    while (i < t.size()) {
        if (t[i] == '\\' && i + 1 < t.size()) { i += 2; continue; }
        if (t[i] == '"') return i;
        ++i;
    }
    return std::string::npos;
}

bool readLabel(const std::string &t, std::size_t pos, const std::vector<std::string> &closers,
        std::string *label, std::size_t *end)
{
    if (pos < t.size() && t[pos] == '"') {
        const std::size_t e = closingQuote(t, pos);
        if (e == std::string::npos) return false;
        *label = t.substr(pos + 1, e - pos - 1);
        std::size_t after = e + 1;
        while (after < t.size() && (t[after] == ' ' || t[after] == '\t')) ++after;
        for (const std::string &closer : closers) {
            if (t.compare(after, closer.size(), closer) == 0) {
                *end = after + closer.size();
                return true;
            }
        }
        return false;
    }
    std::size_t best = std::string::npos;
    std::size_t bestLen = 0;
    for (const std::string &closer : closers) {
        const std::size_t found = t.find(closer, pos);
        if (found != std::string::npos && (best == std::string::npos || found < best)) {
            best = found;
            bestLen = closer.size();
        }
    }
    if (best == std::string::npos) return false;
    *label = unquote(trim(t.substr(pos, best - pos)));
    *end = best + bestLen;
    return true;
}

std::string normaliseStyle(const std::string &styles)
{
    std::string out;
    std::size_t start = 0;
    while (start <= styles.size()) {
        const std::size_t comma = styles.find(',', start);
        std::string part = trim(styles.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (!part.empty()) {
            if (!out.empty()) out += ";";
            out += part;
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return out;
}

std::vector<std::string> splitUnquoted(const std::string &text, char sep)
{
    std::vector<std::string> out;
    std::string current;
    char quote = 0;
    int depth = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        const char ch = text[i];
        if (quote) {
            if (ch == '\\' && i + 1 < text.size()) {
                current.push_back(ch);
                ++i;
                current.push_back(text[i]);
                ++i;
                continue;
            }
            if (ch == quote) quote = 0;
        } else if (ch == '"') {
            quote = ch;
        } else if (ch == '[' || ch == '(' || ch == '{') {
            ++depth;
        } else if (ch == ']' || ch == ')' || ch == '}') {
            depth = std::max(0, depth - 1);
        } else if (ch == sep && depth == 0) {
            out.push_back(current);
            current.clear();
            ++i;
            continue;
        }
        current.push_back(ch);
        ++i;
    }
    out.push_back(current);
    return out;
}

} // namespace

// --------------------------------------------------------------------------
// Flowchart parse
// --------------------------------------------------------------------------

Flowchart::Flowchart(const Source &source, Diagnostics *diagnostics)
{
    this->source = source;
    this->diagnostics = diagnostics;
    diagramId = "flowchart";
    role = "flowchart-v2";
    configKey = "flowchart";
    hasTitle = source.hasTitle;
    title = source.title;
    hasAccDescr = source.hasAccDescr;
    accDescr = source.accDescr;
}

// parse implementation
namespace {

void addEdge(Flowchart &d, const std::string &source, const std::string &target, const LinkInfo &info, int line)
{
    FlowEdge e;
    e.source = source;
    e.target = target;
    e.label = info.label;
    e.kind = info.kind;
    e.startHead = info.startHead;
    e.endHead = info.endHead;
    e.minlen = info.minlen;
    e.line = line;
    e.index = static_cast<int>(d.edges.size());
    d.edges.push_back(e);
}

FlowNode &ensureNode(Flowchart &d, const std::string &id, int line, const std::string &parent, bool hasParent)
{
    auto it = d.nodes.find(id);
    if (it == d.nodes.end()) {
        FlowNode n;
        n.id = id;
        n.label = id;
        n.line = line;
        n.parent = parent;
        n.hasParent = hasParent;
        return d.nodes.emplace(id, n).first->second;
    }
    if (!it->second.hasParent && hasParent) {
        it->second.parent = parent;
        it->second.hasParent = true;
    }
    return it->second;
}

} // namespace

std::shared_ptr<Flowchart> Flowchart::parse(const Source &source, Diagnostics *diagnostics)
{
    auto d = std::make_shared<Flowchart>(source, diagnostics);
    auto &diag = *diagnostics;

    const auto header = firstLine(source);
    const std::string headerText = header.first;
    const int headerLine = header.second;
    std::string trailing;

    {
        std::smatch match;
        const std::regex re(R"(^\s*(?:flowchart(?:-elk|-v2)?|graph)\b[ \t]*([^\s;]+)?[ \t]*(.*)$)",
                std::regex::icase);
        if (std::regex_match(headerText, match, re)) {
            const std::string dir = match[1].str();
            if (!dir.empty()) {
                std::string cleaned = trim(dir);
                while (!cleaned.empty() && (cleaned.front() == ':' || cleaned.front() == ' '))
                    cleaned.erase(cleaned.begin());
                const std::string upper = lower(cleaned);
                std::string up;
                for (char c : upper) up.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                static const std::set<std::string> dirs = { "TB", "TD", "BT", "LR", "RL" };
                if (dirs.count(up)) {
                    d->direction = up == "TD" ? "TB" : up;
                } else {
                    diag.warn("unknown flowchart direction '" + up + "'; using TB", headerLine, 1, "direction");
                }
            }
            std::string rest = trim(match[2].str());
            if (!rest.empty() && rest.front() == ';') {
                trailing = rest.substr(1);
            }
        }
    }

    std::vector<SourceLine> lines;
    if (!trim(trailing).empty()) {
        lines.push_back({ trim(trailing), headerLine, 0 });
    }
    for (const SourceLine &sl : iterLines(source)) {
        lines.push_back(sl);
    }

    // statement dispatch: define a local lambda that operates on the diagram.
    std::vector<std::string> subgraphStack;
    auto anonRef = std::make_shared<int>(0);

    std::function<void(const SourceLine &)> statement = [&](const SourceLine &line) {
        const std::string text = line.stripped();
        if (text.empty()) return;
        if (consumeCommon(*d, line)) return;
        const std::string lowered = lower(text);

        if (lowered == "end") {
            if (!subgraphStack.empty()) {
                subgraphStack.pop_back();
            } else {
                diag.error("'end' without a matching 'subgraph'", line.number, 1, "subgraph");
            }
            return;
        }
        if (startsWith(lowered, "subgraph")) {
            std::string rest = trim(text.substr(std::string("subgraph").size()));
            const std::string parent = subgraphStack.empty() ? std::string() : subgraphStack.back();
            std::string subId, label;
            if (rest.empty()) {
                subId = "subGraph" + std::to_string(++(*anonRef));
            } else {
                std::smatch match;
                const std::regex re(R"(^([^\s\[("]+)\s*[\[\(]\s*(.*?)\s*[\]\)]\s*$)");
                if (std::regex_match(rest, match, re)) {
                    subId = match[1].str();
                    label = unquote(match[2].str());
                } else {
                    subId = label = unquote(rest);
                }
            }
            if (d->subgraphs.count(subId)) {
                diag.warn("duplicate subgraph id '" + subId + "'", line.number, 1, "duplicate-id");
            }
            Subgraph sub;
            sub.id = subId;
            sub.label = label.empty() ? subId : label;
            sub.parent = parent;
            sub.hasParent = !parent.empty();
            sub.line = line.number;
            d->subgraphs[subId] = sub;
            subgraphStack.push_back(subId);
            return;
        }
        if (startsWith(lowered, "direction ")) {
            std::string dir = text.substr(std::string("direction").size());
            dir = lower(trim(dir));
            static const std::set<std::string> dirs = { "tb", "td", "bt", "lr", "rl" };
            if (!dirs.count(dir)) {
                diag.warn("unknown direction '" + dir + "'", line.number, 1, "direction");
            } else {
                std::string up;
                for (char c : dir) up.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                if (up == "TD") up = "TB";
                if (!subgraphStack.empty()) {
                    d->subgraphs[subgraphStack.back()].direction = up;
                    d->subgraphs[subgraphStack.back()].hasDirection = true;
                } else {
                    d->direction = up;
                }
            }
            return;
        }
        if (startsWith(lowered, "classdef")) {
            const std::string rest = trim(text.substr(std::string("classDef").size()));
            const std::size_t space = rest.find_first_of(" \t");
            if (rest.empty()) {
                diag.error("classDef needs a name", line.number, 1, "classdef");
                return;
            }
            const std::string names = space == std::string::npos ? rest : rest.substr(0, space);
            const std::string styles = space == std::string::npos ? "" : trim(rest.substr(space));
            for (const std::string &name : splitUnquoted(names, ',')) {
                const std::string n = trim(name);
                if (!n.empty()) d->classDefs[n] = normaliseStyle(styles);
            }
            return;
        }
        if (startsWith(lowered, "class ")) {
            const std::string rest = trim(text.substr(std::string("class").size()));
            const std::size_t space = rest.find_last_of(" \t");
            if (space == std::string::npos) {
                diag.error("class needs node ids and a class name", line.number, 1, "class");
                return;
            }
            const std::string ids = rest.substr(0, space);
            const std::string name = trim(rest.substr(space));
            for (const std::string &nodeIdRaw : splitUnquoted(ids, ',')) {
                const std::string nodeId = trim(nodeIdRaw);
                if (nodeId.empty()) continue;
                auto it = d->nodes.find(nodeId);
                if (it == d->nodes.end()) {
                    diag.warn("class applied to unknown node '" + nodeId + "'", line.number, 1, "unknown-node");
                    FlowNode &n = ensureNode(*d, nodeId, line.number, "", false);
                    n.classes.push_back(name);
                } else {
                    it->second.classes.push_back(name);
                }
            }
            return;
        }
        if (startsWith(lowered, "style ")) {
            const std::string rest = trim(text.substr(std::string("style").size()));
            const std::size_t space = rest.find_first_of(" \t");
            if (space == std::string::npos) {
                diag.error("style needs a node id and properties", line.number, 1, "style");
                return;
            }
            const std::string nodeId = trim(rest.substr(0, space));
            const std::string styles = trim(rest.substr(space));
            auto it = d->nodes.find(nodeId);
            if (it == d->nodes.end()) {
                diag.warn("style for unknown node '" + nodeId + "'", line.number, 1, "unknown-node");
                return;
            }
            it->second.style = normaliseStyle(styles);
            return;
        }
        if (startsWith(lowered, "linkstyle")) {
            const std::string rest = trim(text.substr(std::string("linkStyle").size()));
            const std::size_t space = rest.find_first_of(" \t");
            if (space == std::string::npos) {
                diag.error("linkStyle needs an index and properties", line.number, 1, "linkstyle");
                return;
            }
            const std::string selector = trim(rest.substr(0, space));
            const std::string styles = normaliseStyle(trim(rest.substr(space)));
            for (const std::string &tokenRaw : splitUnquoted(selector, ',')) {
                const std::string token = trim(tokenRaw);
                if (token == "default") {
                    d->linkStyleDefault = styles;
                } else if (!token.empty() && std::all_of(token.begin(), token.end(),
                                   [](unsigned char c) { return std::isdigit(c); })) {
                    d->linkStyles[std::stoi(token)] = styles;
                } else {
                    diag.warn("unusable linkStyle selector '" + token + "'", line.number, 1, "linkstyle");
                }
            }
            return;
        }
        if (startsWith(lowered, "click ")) {
            // click <id> [href] <url>  -- hand-parsed to avoid a regex.
            const std::string rest = trim(text.substr(6));
            std::size_t sp = rest.find_first_of(" \t");
            if (sp != std::string::npos) {
                const std::string nodeId = rest.substr(0, sp);
                std::string target = trim(rest.substr(sp));
                if (startsWith(lower(target), "href ")) {
                    target = trim(target.substr(5));
                }
                target = unquote(target);
                auto it = d->nodes.find(nodeId);
                if (it != d->nodes.end()) {
                    it->second.hasLink = true;
                    it->second.link = target;
                }
            }
            return;
        }
        if (startsWith(lowered, "%%")) {
            return;
        }

        // A node/link chain.
        std::size_t pos = 0;
        std::vector<std::string> previous;
        bool hasPrevious = false;
        bool hasPending = false;
        LinkInfo pending;
        int guard = 0;
        auto skipSpace = [&](std::size_t p) {
            while (p < text.size() && (text[p] == ' ' || text[p] == '\t')) ++p;
            return p;
        };
        auto readNode = [&](std::size_t p, std::string *outId) -> std::size_t {
            const std::size_t start = p;
            while (p < text.size() && !idStop().count(text[p])) {
                if (text.compare(p, 3, ":::") == 0) break;
                LinkInfo probe;
                if (matchArrowFull(text, p, &probe) != std::string::npos) break;
                std::string openKind, openHead;
                if (matchArrowOpen(text, p, &openKind, &openHead) != std::string::npos) break;
                ++p;
            }
            const std::string nodeId = text.substr(start, p - start);
            if (nodeId.empty()) {
                *outId = "";
                return start;
            }
            FlowNode &node = ensureNode(*d, nodeId, line.number,
                    subgraphStack.empty() ? "" : subgraphStack.back(), !subgraphStack.empty());
            // shape + label
            for (const ShapeToken &tok : shapeTokens()) {
                if (text.compare(p, tok.opener.size(), tok.opener) != 0) continue;
                std::string label;
                std::size_t endPos = 0;
                if (!readLabel(text, p + tok.opener.size(), tok.closers, &label, &endPos)) continue;
                if (node.labelled && node.label != label) {
                    diag.warn("node '" + nodeId + "' redefined with a different label", line.number,
                            static_cast<int>(p) + 1, "duplicate-label");
                }
                node.label = label;
                node.shape = slantedShape(tok.shape, text, endPos, tok.closers);
                node.labelled = true;
                p = endPos;
                break;
            }
            // :::className
            while (text.compare(p, 3, ":::") == 0) {
                std::size_t q = p + 3;
                const std::size_t start = q;
                while (q < text.size()
                        && (std::isalnum(static_cast<unsigned char>(text[q])) || text[q] == '_' || text[q] == '-')) {
                    ++q;
                }
                if (q == start) break;
                node.classes.push_back(text.substr(start, q - start));
                p = q;
            }
            *outId = nodeId;
            return p;
        };

        while (pos < text.size() && guard < 500) {
            ++guard;
            pos = skipSpace(pos);
            if (pos >= text.size()) break;
            // read node group A & B
            std::vector<std::string> group;
            while (true) {
                pos = skipSpace(pos);
                std::string nodeId;
                const std::size_t next = readNode(pos, &nodeId);
                if (nodeId.empty()) break;
                group.push_back(nodeId);
                pos = skipSpace(next);
                if (pos < text.size() && text[pos] == '&') {
                    ++pos;
                    continue;
                }
                break;
            }
            if (group.empty()) {
                diag.error("could not parse '" + text.substr(pos, std::min<std::size_t>(20, text.size() - pos))
                                + "' as a node",
                        line.number, static_cast<int>(pos) + 1, "syntax");
                return;
            }
            if (hasPending && hasPrevious) {
                for (const std::string &src : previous) {
                    for (const std::string &tgt : group) {
                        addEdge(*d, src, tgt, pending, line.number);
                    }
                }
                hasPending = false;
            }
            previous = group;
            hasPrevious = true;
            pos = skipSpace(pos);
            if (pos >= text.size()) break;
            LinkInfo info;
            std::size_t linkEnd = 0;
            if (!readLink(text, pos, &info, &linkEnd)) {
                diag.error("expected a link but found '"
                                + text.substr(pos, std::min<std::size_t>(20, text.size() - pos)) + "'",
                        line.number, static_cast<int>(pos) + 1, "syntax");
                return;
            }
            pending = info;
            hasPending = true;
            pos = linkEnd;
        }
        if (hasPending) {
            diag.error("link has no target node", line.number, static_cast<int>(text.size()), "syntax");
        }
    };

    for (const SourceLine &sl : lines) {
        // split on unquoted ';'
        if (sl.text.find(';') == std::string::npos) {
            statement(sl);
        } else {
            for (const std::string &part : splitUnquoted(sl.text, ';')) {
                if (!trim(part).empty()) {
                    statement({ trim(part), sl.number, sl.indent });
                }
            }
        }
    }
    if (!subgraphStack.empty()) {
        diag.error("unterminated subgraph '" + subgraphStack.back() + "'; missing 'end'", 0, 0, "subgraph");
    }

    // validate
    const bool multi = d->nodes.size() > 1;
    std::set<std::string> connected;
    for (const FlowEdge &e : d->edges) {
        connected.insert(e.source);
        connected.insert(e.target);
    }
    for (const FlowEdge &e : d->edges) {
        for (const std::string &endpoint : { e.source, e.target }) {
            if (!d->nodes.count(endpoint) && !d->subgraphs.count(endpoint)) {
                diag.warn("edge references unknown node '" + endpoint + "'", e.line, 1, "unknown-node");
            }
        }
    }
    for (const auto &entry : d->nodes) {
        for (const std::string &clsName : entry.second.classes) {
            if (!d->classDefs.count(clsName) && clsName != "default") {
                diag.warn("node '" + entry.first + "' uses undefined class '" + clsName + "'", entry.second.line, 1,
                        "unknown-class");
            }
        }
        if (!connected.count(entry.first) && multi) {
            diag.info("node '" + entry.first + "' is not connected to anything", entry.second.line, 1,
                    "isolated-node");
        }
    }
    return d;
}


namespace {

constexpr double kMinHeight = 36.0;
constexpr double kSubroutineBar = 8.0;
constexpr double kClusterCrossPadding = 10.0;
constexpr double kNestedRankExtra = 25.0;
constexpr double kClusterLabelPadding = 8.0;

std::string perpendicular(const std::string &direction)
{
    return (direction == "TB" || direction == "BT") ? "LR" : "TB";
}

double cylinderRy(double width)
{
    const double radius = width / 2;
    return radius / (2.5 + width / 50);
}

//: shape-affecting declarations only (fill, stroke, ...)
std::string styleToCss(const std::string &style)
{
    std::string out;
    std::size_t start = 0;
    while (start <= style.size()) {
        const std::size_t semi = style.find(';', start);
        std::string decl = trim(style.substr(start, semi == std::string::npos ? std::string::npos : semi - start));
        if (!decl.empty()) {
            const std::size_t colon = decl.find(':');
            std::string name = lower(trim(decl.substr(0, colon)));
            if (!name.empty() && name.rfind("color", 0) != 0 && name != "font-size") {
                if (!out.empty()) out += ";";
                out += decl;
            }
        }
        if (semi == std::string::npos) break;
        start = semi + 1;
    }
    return out;
}

std::string extractLabelStyle(const std::string &style)
{
    static const std::set<std::string> keep = { "color", "font-size", "font-weight", "font-style" };
    std::string out;
    std::size_t start = 0;
    while (start <= style.size()) {
        const std::size_t semi = style.find(';', start);
        std::string decl = trim(style.substr(start, semi == std::string::npos ? std::string::npos : semi - start));
        if (!decl.empty()) {
            const std::size_t colon = decl.find(':');
            const std::string name = lower(trim(decl.substr(0, colon)));
            if (keep.count(name)) {
                if (!out.empty()) out += ";";
                out += decl;
            }
        }
        if (semi == std::string::npos) break;
        start = semi + 1;
    }
    return out;
}

std::string pointsCsv(const std::vector<std::pair<double, double>> &pts)
{
    std::string out;
    for (const auto &p : pts) {
        if (!out.empty()) out += " ";
        out += fmt(p.first) + "," + fmt(p.second);
    }
    return out;
}

std::shared_ptr<El> rectEl(double w, double h, const std::map<std::string, std::string> &attrs, bool rounded,
        double rx = 0, double ry = 0)
{
    auto el = std::make_shared<El>("rect");
    el->attr("x", -w / 2);
    el->attr("y", -h / 2);
    el->attr("width", w);
    el->attr("height", h);
    if (rounded) {
        el->attr("rx", rx);
        el->attr("ry", ry);
    }
    for (const auto &a : attrs) {
        el->attr(a.first, a.second);
    }
    return el;
}

std::shared_ptr<El> polygonEl(const std::vector<std::pair<double, double>> &points,
        const std::map<std::string, std::string> &attrs)
{
    auto el = std::make_shared<El>("polygon");
    el->attr("points", pointsCsv(points));
    for (const auto &a : attrs) {
        el->attr(a.first, a.second);
    }
    return el;
}

struct ShapeSpec
{
    std::function<std::pair<double, double>(double, double, double)> size;
    std::function<std::vector<std::shared_ptr<El>>(double, double, const std::map<std::string, std::string> &)> draw;
    double labelOffset = 0.0;
};

const std::map<std::string, ShapeSpec> &shapeSpecs()
{
    static const std::map<std::string, ShapeSpec> specs = [] {
        std::map<std::string, ShapeSpec> m;
        auto rect = [](double tw, double th, double pad) { return std::make_pair(tw + 4 * pad, th + 2 * pad); };
        m["rect"] = { rect,
            [](double w, double h, const std::map<std::string, std::string> &a) {
                return std::vector<std::shared_ptr<El>> { rectEl(w, h, a, true, 0, 0) };
            },
            0.0 };
        m["round"] = { [](double tw, double th, double pad) { return std::make_pair(tw + 2 * pad, th + 2 * pad); },
            [](double w, double h, const std::map<std::string, std::string> &a) {
                return std::vector<std::shared_ptr<El>> { rectEl(w, h, a, true, 5, 5) };
            },
            0.0 };
        m["stadium"] = { [](double tw, double th, double pad) {
                            return std::make_pair(tw + pad + (th + pad) / 4, th + pad);
                        },
            [](double w, double h, const std::map<std::string, std::string> &a) {
                return std::vector<std::shared_ptr<El>> { rectEl(w, h, a, true, h / 2, h / 2) };
            },
            0.0 };
        m["odd"] = { [](double tw, double th, double pad) {
                        return std::make_pair(tw + pad + (th + pad) / 4, th + pad);
                    },
            [](double w, double h, const std::map<std::string, std::string> &a) {
                const double hw = w / 2, hh = h / 2, k = hh / 2;
                return std::vector<std::shared_ptr<El>> { polygonEl(
                        { { -hw, -hh }, { hw - k, -hh }, { hw, 0 }, { hw - k, hh }, { -hw, hh } }, a) };
            },
            0.0 };
        m["diamond"] = { [](double tw, double th, double pad) {
                            const double s = (tw + pad) + (th + pad);
                            return std::make_pair(s, s);
                        },
            [](double w, double h, const std::map<std::string, std::string> &a) {
                return std::vector<std::shared_ptr<El>> { polygonEl(diamondPoints(0, 0, w, h), a) };
            },
            0.0 };
        m["circle"] = { [](double tw, double th, double pad) {
                           return std::make_pair(tw + pad, std::max(tw + pad, th));
                       },
            [](double w, double h, const std::map<std::string, std::string> &a) {
                auto el = std::make_shared<El>("circle");
                el->attr("cx", 0);
                el->attr("cy", 0);
                el->attr("r", std::min(w, h) / 2);
                for (const auto &x : a) el->attr(x.first, x.second);
                return std::vector<std::shared_ptr<El>> { el };
            },
            0.0 };
        m["doublecircle"] = { [](double tw, double th, double pad) {
                                 return std::make_pair(tw + 2 * pad, std::max(tw + 2 * pad, th));
                             },
            [](double w, double h, const std::map<std::string, std::string> &a) {
                const double r = std::min(w, h) / 2;
                auto c1 = std::make_shared<El>("circle");
                c1->attr("cx", 0); c1->attr("cy", 0); c1->attr("r", r);
                auto c2 = std::make_shared<El>("circle");
                c2->attr("cx", 0); c2->attr("cy", 0); c2->attr("r", r - 5);
                for (const auto &x : a) { c1->attr(x.first, x.second); c2->attr(x.first, x.second); }
                return std::vector<std::shared_ptr<El>> { c1, c2 };
            },
            0.0 };
        m["hexagon"] = { [](double tw, double th, double pad) {
                            return std::make_pair(tw + pad + (th + pad) / 2, th + pad);
                        },
            [](double w, double h, const std::map<std::string, std::string> &a) {
                const double hw = w / 2, hh = h / 2, k = h / 4;
                return std::vector<std::shared_ptr<El>> { polygonEl(
                        { { -hw + k, -hh }, { hw - k, -hh }, { hw, 0 }, { hw - k, hh }, { -hw + k, hh }, { -hw, 0 } }, a) };
            },
            0.0 };
        m["subroutine"] = { [](double tw, double th, double pad) {
                               return std::make_pair(tw + pad + 2 * kSubroutineBar, th + pad);
                           },
            [](double w, double h, const std::map<std::string, std::string> &a) {
                const double hw = w / 2, hh = h / 2;
                auto path = std::make_shared<El>("path");
                path->attr("d", "M" + fmt(-hw + 8) + "," + fmt(-hh) + " V" + fmt(hh) + " M" + fmt(hw - 8) + ","
                                + fmt(-hh) + " V" + fmt(hh));
                path->attr("class", "node-shape");
                path->attr("fill", "none");
                return std::vector<std::shared_ptr<El>> { rectEl(w, h, a, false), path };
            },
            0.0 };
        for (const std::string &shape : { std::string("lean_right"), std::string("lean_left"),
                     std::string("trapezoid") }) {
            m[shape] = { [](double tw, double th, double pad) {
                            return std::make_pair(tw + (th + pad) + pad, th + pad);
                        },
                [shape](double w, double h, const std::map<std::string, std::string> &a) {
                    return std::vector<std::shared_ptr<El>> { polygonEl(slantedPoints(shape, 0, 0, w, h), a) };
                },
                0.0 };
        }
        m["inv_trapezoid"] = { [](double tw, double th, double pad) {
                                  return std::make_pair(tw + (th + 2 * pad) + 2 * pad, th + 2 * pad);
                              },
            [](double w, double h, const std::map<std::string, std::string> &a) {
                return std::vector<std::shared_ptr<El>> { polygonEl(slantedPoints("inv_trapezoid", 0, 0, w, h), a) };
            },
            0.0 };
        m["cylinder"] = { [](double tw, double th, double pad) {
                             const double w = tw + pad;
                             return std::make_pair(w, th + pad + 3 * cylinderRy(w));
                         },
            [](double w, double h, const std::map<std::string, std::string> &a) {
                const double hw = w / 2, hh = h / 2;
                const double ry = cylinderRy(w);
                Path path;
                path.M(-hw, -hh + ry);
                path.A(hw, ry, 0, 0, 0, hw, -hh + ry);
                path.V(hh - ry);
                path.A(hw, ry, 0, 0, 1, -hw, hh - ry);
                path.Z();
                auto barrel = std::make_shared<El>("path");
                barrel->attr("d", path.str());
                for (const auto &x : a) barrel->attr(x.first, x.second);
                auto lip = std::make_shared<El>("path");
                lip->attr("d", "M" + fmt(-hw) + "," + fmt(-hh + ry) + "A" + fmt(hw) + "," + fmt(ry) + " 0 0 0 "
                                + fmt(hw) + "," + fmt(-hh + ry));
                lip->attr("class", "node-shape");
                lip->attr("fill", "none");
                return std::vector<std::shared_ptr<El>> { barrel, lip };
            },
            1.0 / 1.5 };
        m["text"] = { [](double tw, double th, double) { return std::make_pair(tw, th); },
            [](double w, double h, const std::map<std::string, std::string> &) {
                auto el = std::make_shared<El>("rect");
                el->attr("x", -w / 2);
                el->attr("y", -h / 2);
                el->attr("width", w);
                el->attr("height", h);
                el->attr("fill", "none");
                el->attr("stroke", "none");
                return std::vector<std::shared_ptr<El>> { el };
            },
            0.0 };
        return m;
    }();
    return specs;
}

const ShapeSpec &shapeSpec(const std::string &shape)
{
    auto it = shapeSpecs().find(shape);
    return it == shapeSpecs().end() ? shapeSpecs().at("rect") : it->second;
}

//: Adapts a laid-out node to what shapeIntersect expects.
struct ShapeProxy
{
    double x = 0, y = 0, width = 0, height = 0;
    std::string shape = "rect";
};

} // namespace

RenderResult Flowchart::render(const RenderContext &ctx)
{
    // The renderer is a compiler of the parsed model; keep it in one place.
    struct Renderer
    {
        const Flowchart &d;
        const RenderContext &ctx;
        const Theme &theme;
        double nodeSpacing, rankSpacing, padding, diagramPadding, wrappingWidth, fontSize;
        std::string curve;
        std::shared_ptr<TextMeasurer> measurer;
        std::map<std::string, std::pair<double, double>> sizes;
        std::map<std::string, std::vector<std::string>> linesByNode;
        std::map<int, std::vector<std::string>> edgeLines;
        std::map<std::string, std::set<std::string>> descendants;
        std::set<std::string> extracted;

        std::map<std::string, FlowNode> realNodes() const
        {
            std::map<std::string, FlowNode> out;
            for (const auto &e : d.nodes) {
                if (!d.subgraphs.count(e.first)) out[e.first] = e.second;
            }
            return out;
        }

        std::vector<std::string> ancestorSubgraphs(const std::string &start, bool hasStart) const
        {
            std::vector<std::string> chain;
            std::string current = start;
            bool has = hasStart;
            int guard = 0;
            while (has && !current.empty() && guard < 64) {
                chain.push_back(current);
                auto it = d.subgraphs.find(current);
                has = it != d.subgraphs.end() && it->second.hasParent;
                current = has ? it->second.parent : std::string();
                ++guard;
            }
            return chain;
        }

        std::string parentOf(const std::string &endpoint) const
        {
            auto sit = d.subgraphs.find(endpoint);
            if (sit != d.subgraphs.end()) return sit->second.hasParent ? sit->second.parent : std::string();
            auto nit = d.nodes.find(endpoint);
            return (nit != d.nodes.end() && nit->second.hasParent) ? nit->second.parent : std::string();
        }

        std::map<std::string, std::set<std::string>> clusterDescendants() const
        {
            std::map<std::string, std::set<std::string>> inside;
            for (const auto &e : d.subgraphs) inside[e.first] = {};
            auto addAll = [&](const std::string &id, bool hasParent, const std::string &parent) {
                for (const std::string &sub : ancestorSubgraphs(parent, hasParent)) {
                    inside[sub].insert(id);
                }
            };
            for (const auto &e : realNodes()) addAll(e.first, e.second.hasParent, e.second.parent);
            for (const auto &e : d.subgraphs) addAll(e.first, e.second.hasParent, e.second.parent);
            return inside;
        }

        std::set<std::string> extractedSubgraphs() const
        {
            std::set<std::string> out;
            for (const auto &e : d.subgraphs) {
                auto it = descendants.find(e.first);
                const std::set<std::string> inside = (it != descendants.end()) ? it->second : std::set<std::string>();
                if (inside.empty()) continue;
                if (e.second.hasDirection) { out.insert(e.first); continue; }
                bool crossing = false;
                for (const FlowEdge &edge : d.edges) {
                    if (inside.count(edge.source) != inside.count(edge.target)) { crossing = true; break; }
                }
                if (!crossing) out.insert(e.first);
            }
            return out;
        }

        std::string levelOf(const std::string &parent, bool hasParent) const
        {
            for (const std::string &c : ancestorSubgraphs(parent, hasParent)) {
                if (extracted.count(c)) return c;
            }
            return std::string();
        }

        std::string edgeLevel(const FlowEdge &edge) const
        {
            const std::string sp = parentOf(edge.source);
            const std::string tp = parentOf(edge.target);
            const bool sh = !sp.empty() || d.subgraphs.count(edge.source);
            const bool th = !tp.empty() || d.subgraphs.count(edge.target);
            const std::vector<std::string> source = ancestorSubgraphs(d.subgraphs.count(edge.source) ? edge.source : sp,
                    d.subgraphs.count(edge.source) || !sp.empty());
            const std::vector<std::string> target = ancestorSubgraphs(d.subgraphs.count(edge.target) ? edge.target : tp,
                    d.subgraphs.count(edge.target) || !tp.empty());
            for (const std::string &sub : source) {
                if (extracted.count(sub) && std::find(target.begin(), target.end(), sub) != target.end()) {
                    return sub;
                }
            }
            (void)sh; (void)th;
            return std::string();
        }

        std::string anchor(const std::string &endpoint, const std::string &level) const
        {
            std::string collapsed;
            const bool endpointIsSub = d.subgraphs.count(endpoint) != 0;
            const std::vector<std::string> chain = endpointIsSub
                    ? ancestorSubgraphs(endpoint, true)
                    : ancestorSubgraphs(parentOf(endpoint), !parentOf(endpoint).empty());
            for (const std::string &sub : chain) {
                if (sub == level) break;
                if (extracted.count(sub)) collapsed = sub;
            }
            if (!collapsed.empty()) return collapsed;
            if (endpointIsSub) {
                auto it = descendants.find(endpoint);
                if (it != descendants.end()) {
                    for (const auto &n : realNodes()) {
                        if (it->second.count(n.first)) return n.first;
                    }
                }
            }
            return endpoint;
        }

        std::pair<double, double> nodeSize(const FlowNode &node, std::vector<std::string> *lines)
        {
            TextBlock block = measurer->block(node.label, fontSize, false, wrappingWidth, HTML_LABEL_LINE_HEIGHT);
            const double labelH = std::max(block.height(), fontSize * HTML_LABEL_LINE_HEIGHT);
            const auto sz = shapeSpec(node.shape).size(block.width(), labelH, padding);
            *lines = block.lines;
            return sz;
        }

        double clusterLabelWidth(const Subgraph &sub) const
        {
            if (sub.label.empty()) return 0.0;
            TextBlock block = measurer->block(sub.label, fontSize, false, 0, HTML_LABEL_LINE_HEIGHT);
            return block.width() + kClusterLabelPadding;
        }

        std::vector<std::string> edgeLabelLines(const FlowEdge &edge)
        {
            if (edge.label.empty()) return {};
            TextBlock block = measurer->block(edge.label, fontSize, false, wrappingWidth, HTML_LABEL_LINE_HEIGHT);
            return block.lines;
        }

        std::pair<double, double> edgeLabelSize(const FlowEdge &edge) const
        {
            if (edge.label.empty()) return { 0.0, 0.0 };
            TextBlock block = measurer->block(edge.label, fontSize, false, wrappingWidth, HTML_LABEL_LINE_HEIGHT);
            return { block.width(), std::max(block.height(), fontSize * HTML_LABEL_LINE_HEIGHT) };
        }

        DagreGraph layoutLevel(const std::string &level, const std::string &direction, double rankSpacing)
        {
            DagreGraph g;
            const double padCross = nodeSpacing / 2 + kClusterCrossPadding;
            const double padRank = rankSpacing / 2;
            std::map<std::string, std::tuple<DagreGraph, double, double>> children;

            for (const auto &e : d.subgraphs) {
                const Subgraph &sub = e.second;
                const std::string subParent = sub.hasParent ? sub.parent : std::string();
                if (levelOf(subParent, sub.hasParent) != level) continue;
                const bool hasParent = sub.hasParent && sub.parent != level;
                const std::string parent = hasParent ? sub.parent : std::string();
                if (extracted.count(sub.id)) {
                    const double innerRank = rankSpacing + kNestedRankExtra;
                    const std::string inner = sub.hasDirection ? sub.direction : perpendicular(direction);
                    DagreGraph child = layoutLevel(sub.id, inner, innerRank);
                    double growX, growY;
                    if (inner == "LR" || inner == "RL") {
                        growX = innerRank / 2;
                        growY = padCross;
                    } else {
                        growX = padCross;
                        growY = innerRank / 2;
                    }
                    const double width = std::max(child.width + 2 * growX, clusterLabelWidth(sub));
                    DagreNode &n = g.addNode(sub.id, width, child.height + 2 * growY);
                    if (hasParent) { n.parent = parent; n.hasParent = true; }
                    children[sub.id] = { child, (width - child.width) / 2, growY };
                } else {
                    DagreCluster &cluster = g.addCluster(sub.id, sub.label);
                    if (hasParent) { cluster.parent = parent; cluster.hasParent = true; }
                    cluster.padding = padCross;
                    cluster.minWidth = clusterLabelWidth(sub);
                }
            }

            for (const auto &e : realNodes()) {
                const FlowNode &node = e.second;
                const std::string nodeParent = node.hasParent ? node.parent : std::string();
                if (levelOf(nodeParent, node.hasParent) != level) continue;
                const auto sz = sizes.at(node.id);
                DagreNode &n = g.addNode(node.id, sz.first, sz.second);
                const bool hasParent = node.hasParent && node.parent != level;
                if (hasParent) { n.parent = node.parent; n.hasParent = true; }
            }

            for (const FlowEdge &edge : d.edges) {
                if (edgeLevel(edge) != level) continue;
                std::string source = anchor(edge.source, level);
                std::string target = anchor(edge.target, level);
                for (const std::string &endpoint : { source, target }) {
                    if (!g.nodes.count(endpoint)) g.addNode(endpoint, 60, kMinHeight);
                }
                const auto ls = edgeLabelSize(edge);
                edgeLines[edge.index] = edgeLabelLines(edge);
                DagreEdge &de = g.addEdge(source, target);
                de.minlen = edge.minlen;
                de.weight = 1.0;
                de.labelWidth = ls.first;
                de.labelHeight = ls.second;
                de.data = edge.index;
            }

            LayoutOptions opts;
            opts.rankdir = direction;
            opts.nodesep = nodeSpacing;
            opts.ranksep = rankSpacing;
            opts.edgesep = nodeSpacing / 5;
            opts.clusterPadding = padCross;
            opts.clusterPaddingX = padCross;
            opts.clusterPaddingY = padRank;
            opts.marginX = 0;
            opts.marginY = 0;
            dagreLayout(g, opts);

            for (auto &entry : children) {
                const std::string &subId = entry.first;
                DagreGraph &child = std::get<0>(entry.second);
                const double growX = std::get<1>(entry.second);
                const double growY = std::get<2>(entry.second);
                const DagreNode placed = g.nodes.at(subId);
                // merge translated
                for (auto &ce : child.nodes) {
                    ce.second.x += placed.x - placed.width / 2 + growX;
                    ce.second.y += placed.y - placed.height / 2 + growY;
                    g.nodes[ce.first] = ce.second;
                }
                for (auto &ce : child.clusters) {
                    ce.second.x += placed.x - placed.width / 2 + growX;
                    ce.second.y += placed.y - placed.height / 2 + growY;
                    g.clusters[ce.first] = ce.second;
                }
                for (DagreEdge &ce : child.edges) {
                    for (Point &p : ce.points) {
                        p.first += placed.x - placed.width / 2 + growX;
                        p.second += placed.y - placed.height / 2 + growY;
                    }
                    ce.labelX += placed.x - placed.width / 2 + growX;
                    ce.labelY += placed.y - placed.height / 2 + growY;
                    g.edges.push_back(ce);
                }
                DagreCluster &cluster = g.addCluster(subId, d.subgraphs.at(subId).label);
                cluster.x = placed.x;
                cluster.y = placed.y;
                cluster.width = placed.width;
                cluster.height = placed.height;
            }
            return g;
        }

        void drawCluster(std::shared_ptr<El> parent, const DagreCluster &cluster) const
        {
            auto group = std::make_shared<El>("g");
            group->attr("class", "cluster");
            group->attr("id", cluster.id);
            const double x = cluster.x - cluster.width / 2;
            const double y = cluster.y - cluster.height / 2;
            auto rect = std::make_shared<El>("rect");
            rect->attr("x", x); rect->attr("y", y);
            rect->attr("width", cluster.width); rect->attr("height", cluster.height);
            rect->attr("rx", 6); rect->attr("ry", 6); rect->attr("class", "cluster-rect");
            group->add(rect);
            if (!cluster.label.empty()) {
                const std::vector<std::string> lines = measurer->splitLines(cluster.label);
                group->add(textElement(lines, cluster.x, y + fontSize * 0.9, "middle", fontSize, 1.25,
                        "cluster-label"));
            }
            parent->add(group);
        }

        std::string nodeStyle(const FlowNode &node) const
        {
            std::string out;
            for (const std::string &clsName : node.classes) {
                auto it = d.classDefs.find(clsName);
                if (it != d.classDefs.end() && !it->second.empty()) {
                    if (!out.empty()) out += ";";
                    out += it->second;
                }
            }
            if (!node.style.empty()) {
                if (!out.empty()) out += ";";
                out += node.style;
            }
            return out;
        }

        void drawNode(std::shared_ptr<El> parent, const FlowNode &node, const DagreNode &laid,
                const std::vector<std::string> &lines) const
        {
            auto group = std::make_shared<El>("g");
            std::string classes = "node default";
            for (const std::string &c : node.classes) classes += " cls-" + c;
            group->attr("class", classes);
            group->attr("id", "flowchart-" + node.id);
            group->attr("transform", "translate(" + fmt(laid.x) + ", " + fmt(laid.y) + ")");
            const std::string style = nodeStyle(node);
            std::map<std::string, std::string> attrs;
            attrs["class"] = "node-shape";
            const std::string shapeCss = styleToCss(style);
            if (!shapeCss.empty()) attrs["style"] = shapeCss;
            for (const auto &el : shapeSpec(node.shape).draw(laid.width, laid.height, attrs)) {
                group->add(el);
            }
            std::string labelStyle = extractLabelStyle(style);
            const double labelY = shapeSpec(node.shape).labelOffset * padding;
            group->add(textElement(lines, 0, labelY, "middle", fontSize, HTML_LABEL_LINE_HEIGHT, "nodeLabel",
                    labelStyle));
            parent->add(group);
        }

        std::vector<std::pair<double, double>> edgePoints(const DagreNode &source, const FlowNode *sFlow,
                const DagreNode &target, const FlowNode *tFlow, const DagreEdge &edge) const
        {
            std::vector<Point> mid = edge.points;
            const Point firstOutside = mid.empty() ? Point { target.x, target.y } : mid.front();
            const Point lastOutside = mid.empty() ? Point { source.x, source.y } : mid.back();
            ShapeProxy sp { source.x, source.y, source.width, source.height, sFlow ? sFlow->shape : "rect" };
            ShapeProxy tp { target.x, target.y, target.width, target.height, tFlow ? tFlow->shape : "rect" };
            const Point start = shapeIntersect(sp.shape, sp.x, sp.y, sp.width, sp.height, firstOutside);
            const Point end = shapeIntersect(tp.shape, tp.x, tp.y, tp.width, tp.height, lastOutside);
            std::vector<Point> out;
            out.push_back(start);
            for (const Point &p : mid) out.push_back(p);
            out.push_back(end);
            return out;
        }

        void drawEdge(std::shared_ptr<SvgDocument> doc, std::shared_ptr<El> paths, std::shared_ptr<El> labels,
                const DagreGraph &g, const DagreEdge &edge, const FlowEdge &flowEdge,
                const std::vector<std::string> &labelLines) const
        {
            auto sIt = g.nodes.find(edge.v);
            auto tIt = g.nodes.find(edge.w);
            if (sIt == g.nodes.end() || tIt == g.nodes.end()) return;
            auto sfIt = d.nodes.find(edge.v);
            auto tfIt = d.nodes.find(edge.w);
            const std::vector<Point> points = edgePoints(sIt->second,
                    sfIt != d.nodes.end() ? &sfIt->second : nullptr, tIt->second,
                    tfIt != d.nodes.end() ? &tfIt->second : nullptr, edge);
            if (flowEdge.kind == "invisible") return;
            const std::string colour = theme.get("lineColor", "#333");
            std::string overrideStyle;
            auto lsIt = d.linkStyles.find(flowEdge.index);
            if (lsIt != d.linkStyles.end()) overrideStyle = lsIt->second;
            else if (!d.linkStyleDefault.empty()) overrideStyle = d.linkStyleDefault;
            auto path = std::make_shared<El>("path");
            path->attr("d", renderCurve(points, curve));
            path->attr("class", "flowchart-link link-" + flowEdge.kind);
            path->attr("id", "L_" + flowEdge.source + "_" + flowEdge.target + "_" + std::to_string(flowEdge.index));
            path->attr("fill", "none");
            if (flowEdge.kind == "dotted") path->attr("stroke-dasharray", "3 3");
            if (flowEdge.kind == "thick") path->attr("stroke-width", "3.5");
            if (!overrideStyle.empty()) path->attr("style", overrideStyle);
            if (!flowEdge.endHead.empty()) {
                path->attr("marker-end", markerDefs(*doc, "flowchart-" + flowEdge.endHead + "-end", colour,
                                              flowEdge.endHead));
            }
            if (!flowEdge.startHead.empty()) {
                path->attr("marker-start", markerDefs(*doc, "flowchart-" + flowEdge.startHead + "-start", colour,
                                                flowEdge.startHead));
            }
            paths->add(path);
            if (!labelLines.empty()) drawEdgeLabel(labels, edge, labelLines);
        }

        void drawEdgeLabel(std::shared_ptr<El> parent, const DagreEdge &edge,
                const std::vector<std::string> &lines) const
        {
            double width = 0;
            for (const std::string &ln : lines) width = std::max(width, measurer->width(ln, fontSize));
            const double height = static_cast<double>(lines.size()) * fontSize * HTML_LABEL_LINE_HEIGHT;
            auto group = std::make_shared<El>("g");
            group->attr("class", "edgeLabel");
            auto rect = std::make_shared<El>("rect");
            rect->attr("x", edge.labelX - width / 2);
            rect->attr("y", edge.labelY - height / 2);
            rect->attr("width", width);
            rect->attr("height", height);
            rect->attr("class", "edge-label-bkg");
            rect->attr("rx", 2); rect->attr("ry", 2);
            group->add(rect);
            group->add(textElement(lines, edge.labelX, edge.labelY, "middle", fontSize, HTML_LABEL_LINE_HEIGHT,
                    "edgeLabel-text"));
            parent->add(group);
        }

        std::string css() const
        {
            const Theme &t = theme;
            std::ostringstream out;
            out << "#{id} { font-family: " << t.fontFamily() << "; font-size: " << fmt(t.fontSize()) << "px; }\n";
            out << "#{id} .node-shape { fill: " << t.get("mainBkg", "#ECECFF") << "; stroke: "
                << t.get("nodeBorder", "#9370DB") << "; stroke-width: 1px; }\n";
            out << "#{id} .nodeLabel, #{id} .nodeLabel tspan { fill: " << t.get("nodeTextColor", "#333")
                << "; color: " << t.get("nodeTextColor", "#333") << "; }\n";
            out << "#{id} .flowchart-link { stroke: " << t.get("lineColor", "#333")
                << "; stroke-width: 2px; fill: none; }\n";
            out << "#{id} .link-thick { stroke-width: 3.5px; }\n";
            out << "#{id} .edgeLabel-text, #{id} .edgeLabel-text tspan { fill: "
                << t.get("edgeLabelTextColor", "#333") << "; font-size: " << fmt(t.fontSize() * 0.85) << "px; }\n";
            out << "#{id} .edge-label-bkg { fill: " << t.get("edgeLabelBackground", "#e8e8e8") << "; stroke: none; }\n";
            out << "#{id} .cluster-rect { fill: " << t.get("clusterBkg", "#ffffde") << "; stroke: "
                << t.get("clusterBorder", "#aaaa33") << "; stroke-width: 1px; }\n";
            out << "#{id} .cluster-label, #{id} .cluster-label tspan { fill: " << t.get("clusterTextColor", "#333")
                << "; font-weight: bold; }\n";
            std::string rules = out.str();
            while (!rules.empty() && rules.back() == '\n') rules.pop_back();
            for (const auto &cd : d.classDefs) {
                const std::string cssShape = styleToCss(cd.second);
                if (!cssShape.empty()) rules += "\n#{id} .cls-" + cd.first + " .node-shape { " + cssShape + " }";
                const std::string cssLabel = extractLabelStyle(cd.second);
                if (!cssLabel.empty()) {
                    rules += "\n#{id} .cls-" + cd.first + " .nodeLabel, #{id} .cls-" + cd.first
                            + " .nodeLabel tspan { " + cssLabel + " }";
                }
            }
            return rules;
        }
    };

    Renderer r { *this, ctx, ctx.theme,
        asNumber(ctx.sub("flowchart")["nodeSpacing"].toString(), 50),
        asNumber(ctx.sub("flowchart")["rankSpacing"].toString(), 50),
        asNumber(ctx.sub("flowchart")["padding"].toString(), 15),
        asNumber(ctx.sub("flowchart")["diagramPadding"].toString(), 8),
        asNumber(ctx.sub("flowchart")["wrappingWidth"].toString(), 200), ctx.theme.fontSize(),
        ctx.sub("flowchart")["curve"].isNull() ? std::string("basis") : ctx.sub("flowchart")["curve"].toString(),
        ctx.measurer, {}, {}, {}, {}, {} };

    auto doc = newDocument(ctx);
    doc->addStyle(r.css());

    for (const auto &e : r.realNodes()) {
        std::vector<std::string> lines;
        r.sizes[e.first] = r.nodeSize(e.second, &lines);
        r.linesByNode[e.first] = lines;
    }
    r.descendants = r.clusterDescendants();
    r.extracted = r.extractedSubgraphs();

    DagreGraph g = r.layoutLevel("", direction, r.rankSpacing);

    auto clustersGroup = std::make_shared<El>("g");
    clustersGroup->attr("class", "clusters");
    auto edgesGroup = std::make_shared<El>("g");
    edgesGroup->attr("class", "edgePaths");
    auto edgeLabelsGroup = std::make_shared<El>("g");
    edgeLabelsGroup->attr("class", "edgeLabels");
    auto nodesGroup = std::make_shared<El>("g");
    nodesGroup->attr("class", "nodes");
    doc->root()->add(clustersGroup);
    doc->root()->add(edgesGroup);
    doc->root()->add(edgeLabelsGroup);
    doc->root()->add(nodesGroup);

    std::vector<DagreCluster> clusters;
    for (const auto &e : g.clusters) clusters.push_back(e.second);
    std::sort(clusters.begin(), clusters.end(),
            [](const DagreCluster &a, const DagreCluster &b) { return a.width > b.width; });
    for (const DagreCluster &cluster : clusters) {
        if (cluster.width) r.drawCluster(clustersGroup, cluster);
    }
    for (const DagreEdge &edge : g.edges) {
        const FlowEdge *flowEdge = nullptr;
        for (const FlowEdge &fe : edges) {
            if (fe.index == edge.data) { flowEdge = &fe; break; }
        }
        if (flowEdge == nullptr) continue;
        r.drawEdge(doc, edgesGroup, edgeLabelsGroup, g, edge, *flowEdge, r.edgeLines[flowEdge->index]);
    }
    for (const auto &e : r.realNodes()) {
        auto it = g.nodes.find(e.first);
        if (it == g.nodes.end()) continue;
        r.drawNode(nodesGroup, e.second, it->second, r.linesByNode[e.first]);
    }

    return finish(ctx, *doc, g.width, g.height, r.diagramPadding);
}

} // namespace tqtw_mermaid2svg
