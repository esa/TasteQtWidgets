/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2ser.h"

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

//: word cardinality -> (name, marker kind). side-agnostic.
const std::map<std::string, std::pair<std::string, std::string>> &wordCards()
{
    static const std::map<std::string, std::pair<std::string, std::string>> m = {
        { "one or zero", { "zero-or-one", "zero_one" } }, { "zero or one", { "zero-or-one", "zero_one" } },
        { "one or more", { "one-or-more", "one_many" } }, { "one or many", { "one-or-more", "one_many" } },
        { "many(1)", { "one-or-more", "one_many" } }, { "1+", { "one-or-more", "one_many" } },
        { "zero or more", { "zero-or-more", "zero_many" } }, { "zero or many", { "zero-or-more", "zero_many" } },
        { "many(0)", { "zero-or-more", "zero_many" } }, { "many", { "zero-or-more", "zero_many" } },
        { "0+", { "zero-or-more", "zero_many" } }, { "only one", { "exactly-one", "one" } },
        { "one", { "exactly-one", "one" } }, { "1", { "exactly-one", "one" } },
    };
    return m;
}

std::pair<std::string, std::string> leftCard(const std::string &token)
{
    const std::string t = lower(token);
    if (t == "|o") return { "zero-or-one", "zero_one" };
    if (t == "||") return { "exactly-one", "one" };
    if (t == "}o") return { "zero-or-many", "zero_many" };
    if (t == "}|") return { "one-or-many", "one_many" };
    auto it = wordCards().find(t);
    return it == wordCards().end() ? std::make_pair(std::string("exactly-one"), std::string("one")) : it->second;
}

std::pair<std::string, std::string> rightCard(const std::string &token)
{
    const std::string t = lower(token);
    if (t == "o|") return { "zero-or-one", "zero_one" };
    if (t == "||") return { "exactly-one", "one" };
    if (t == "o{") return { "zero-or-many", "zero_many" };
    if (t == "|{") return { "one-or-many", "one_many" };
    auto it = wordCards().find(t);
    return it == wordCards().end() ? std::make_pair(std::string("exactly-one"), std::string("one")) : it->second;
}

} // namespace

ErDiagram::ErDiagram(const Source &source, Diagnostics *diagnostics)
{
    this->source = source;
    this->diagnostics = diagnostics;
    diagramId = "er";
    role = "er";
    configKey = "er";
    hasTitle = source.hasTitle;
    title = source.title;
    hasAccDescr = source.hasAccDescr;
    accDescr = source.accDescr;
}

namespace {

Entity &entity(ErDiagram &d, const std::string &nameIn, int line)
{
    const std::string key = unquote(trim(nameIn));
    auto it = d.entities.find(key);
    if (it == d.entities.end()) {
        Entity e;
        e.id = key;
        e.label = key;
        e.line = line;
        d.entityOrder.push_back(key);
        return d.entities.emplace(key, e).first->second;
    }
    return it->second;
}

bool isEntityName(const std::string &text)
{
    static const std::regex re(R"(^[\w"][\w\-"\s\[\]]*$)");
    return std::regex_match(text, re);
}

bool parseAttribute(const std::string &text, ErAttribute *out)
{
    static const std::regex re(
            R"RE(^\s*([\w\[\]<>(),]+)\s+([\w\-]+)((?:\s+(?:PK|FK|UK))*)\s*(?:"([^"]*)")?\s*$)RE");
    std::smatch match;
    if (!std::regex_match(text, match, re)) return false;
    out->type = match[1].str();
    out->name = match[2].str();
    // collapse whitespace in keys
    {
        std::istringstream ss(match[3].str());
        std::string word;
        std::string keys;
        while (ss >> word) { if (!keys.empty()) keys += " "; keys += word; }
        out->keys = keys;
    }
    out->comment = match[4].matched ? match[4].str() : "";
    return true;
}

//: Parse `Left lcard <line> rcard Right : label`. Returns false if not a relation.
bool parseRelation(const std::string &text, ErDiagram &d, int line, Diagnostics &diag)
{
    // Find the line token: -- .. .- -. or the words "to"/"optionally to".
    struct Hit
    {
        std::size_t pos, len;
        bool identifying;
    };
    Hit hit { std::string::npos, 0, false };
    const std::string lowered = lower(text);
    // symbolic tokens
    for (const auto &tok : { std::make_pair(std::string("--"), true), std::make_pair(std::string(".."), false),
                 std::make_pair(std::string(".-"), false), std::make_pair(std::string("-."), false) }) {
        const std::size_t pos = text.find(tok.first);
        if (pos != std::string::npos && (hit.pos == std::string::npos || pos < hit.pos)) {
            hit = { pos, tok.first.size(), tok.second };
        }
    }
    // word tokens "to" and "optionally to"
    {
        std::smatch m;
        std::regex re(R"(\s+(optionally to|to)\s+)", std::regex::icase);
        std::smatch best;
        std::size_t bestPos = std::string::npos;
        auto begin = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            const std::size_t pos = static_cast<std::size_t>(it->position());
            if (pos < bestPos) { bestPos = pos; best = *it; }
        }
        if (bestPos != std::string::npos && (hit.pos == std::string::npos || bestPos < hit.pos)) {
            const std::string word = lower(best[1].str());
            hit = { bestPos, static_cast<std::size_t>(best.length()), word == "to" };
        }
    }
    if (hit.pos == std::string::npos) return false;

    std::string before = trim(text.substr(0, hit.pos));
    std::string after = trim(text.substr(hit.pos + hit.len));
    // split label after ':'
    std::string label;
    {
        const std::size_t colon = after.find(':');
        if (colon != std::string::npos) {
            label = trim(after.substr(colon + 1));
            after = trim(after.substr(0, colon));
        }
    }
    // The cardinality token sits at the end of `before` / start of `after`.
    // Symbolic: |o || }o }|   o| || o{ |{   Word: "one"/"many"/"zero or more"...
    auto splitCard = [](std::string &side, bool leftSide) {
        std::string card;
        if (leftSide) {
            // token is the last 1-2 chars of the symbolic forms, or trailing words
            const std::size_t sp = side.find_last_of(' ');
            // check symbolic 2-char suffix
            for (const char *sym : { "|o", "||", "}o", "}|" }) {
                if (side.size() >= 2 && side.substr(side.size() - 2) == sym) {
                    card = sym;
                    side = trim(side.substr(0, side.size() - 2));
                    return card;
                }
            }
            (void)sp;
        } else {
            for (const char *sym : { "o|", "||", "o{", "|{" }) {
                if (side.size() >= 2 && side.substr(0, 2) == sym) {
                    card = sym;
                    side = trim(side.substr(2));
                    return card;
                }
            }
        }
        // word forms: longest match at the end (left) / start (right)
        std::vector<std::string> words;
        for (const auto &e : wordCards()) words.push_back(e.first);
        std::sort(words.begin(), words.end(), [](const std::string &a, const std::string &b) { return a.size() > b.size(); });
        if (leftSide) {
            const std::string low = lower(side);
            for (const std::string &w : words) {
                if (low.size() >= w.size() && low.compare(low.size() - w.size(), w.size(), w) == 0) {
                    // must be preceded by whitespace or start
                    const std::size_t at = side.size() - w.size();
                    if (at == 0 || std::isspace(static_cast<unsigned char>(side[at - 1]))) {
                        card = side.substr(at);
                        side = trim(side.substr(0, at));
                        return card;
                    }
                }
            }
        } else {
            const std::string low = lower(side);
            for (const std::string &w : words) {
                if (low.size() >= w.size() && low.compare(0, w.size(), w) == 0) {
                    if (side.size() == w.size() || std::isspace(static_cast<unsigned char>(side[w.size()]))) {
                        card = side.substr(0, w.size());
                        side = trim(side.substr(w.size()));
                        return card;
                    }
                }
            }
        }
        return card;
    };
    const std::string lcardTok = splitCard(before, true);
    const std::string rcardTok = splitCard(after, false);
    if (!isEntityName(before) || !isEntityName(after)) return false;
    Entity &left = entity(d, before, line);
    Entity &right = entity(d, after, line);
    const auto lc = leftCard(lcardTok);
    const auto rc = rightCard(rcardTok);
    ErRelation rel;
    rel.source = left.id;
    rel.target = right.id;
    rel.leftCard = lc.second;
    rel.rightCard = rc.second;
    rel.identifying = hit.identifying;
    rel.label = unquote(label);
    rel.line = line;
    d.relations.push_back(rel);
    if (label.empty()) {
        diag.info("relation " + left.id + " -- " + right.id + " has no label", line, 1, "missing-label");
    }
    return true;
}

} // namespace

std::shared_ptr<ErDiagram> ErDiagram::parse(const Source &source, Diagnostics *diagnostics)
{
    auto d = std::make_shared<ErDiagram>(source, diagnostics);
    auto &diag = *diagnostics;
    const auto header = firstLine(source);
    {
        std::smatch match;
        const std::regex re(R"(\b(TB|TD|BT|LR|RL)\b)");
        if (std::regex_search(header.first, match, re)) {
            d->direction = match[1].str() == "TD" ? "TB" : match[1].str();
        }
    }
    std::string current;
    bool hasCurrent = false;

    for (const SourceLine &line : iterLines(source)) {
        const std::string text = line.stripped();
        if (consumeCommon(*d, line)) continue;
        if (hasCurrent) {
            if (text == "}") { hasCurrent = false; continue; }
            ErAttribute attr;
            if (!parseAttribute(text, &attr)) {
                throw ParseError("could not parse attribute; expected '<type> <name> [PK|FK|UK]'", line.number, 1);
            }
            d->entities[current].attributes.push_back(attr);
            continue;
        }
        if (text == "}") {
            diag.error("'}' without a matching '{'", line.number, 1, "block");
            continue;
        }
        if (startsWith(lower(text), "direction ")) {
            std::string dir = trim(text.substr(std::string("direction").size()));
            std::transform(dir.begin(), dir.end(), dir.begin(), [](unsigned char c) { return std::toupper(c); });
            d->direction = dir == "TD" ? "TB" : dir;
            continue;
        }
        if (parseRelation(text, *d, line.number, diag)) continue;
        if (text.find('{') != std::string::npos) {
            const std::size_t brace = text.find('{');
            const std::string namePart = trim(text.substr(0, brace));
            std::string rest = trim(text.substr(brace + 1));
            if (!isEntityName(namePart)) {
                throw ParseError("expected an entity name before '{'", line.number, 1);
            }
            Entity &e = entity(*d, namePart, line.number);
            current = e.id;
            hasCurrent = true;
            const bool closed = !rest.empty() && rest.back() == '}';
            if (closed) rest = trim(rest.substr(0, rest.size() - 1));
            if (!rest.empty()) {
                ErAttribute attr;
                if (parseAttribute(rest, &attr)) d->entities[current].attributes.push_back(attr);
            }
            if (closed) hasCurrent = false;
            continue;
        }
        if (isEntityName(text)) {
            entity(*d, unquote(text), line.number);
            continue;
        }
        throw ParseError("could not parse ER statement", line.number, 1);
    }

    if (hasCurrent) {
        diag.error("entity '" + current + "' block is never closed", 0, 0, "block");
    }
    if (d->entities.empty()) diag.warn("ER diagram has no entities", 0, 0, "empty");
    for (const auto &e : d->entities) {
        std::set<std::string> seen;
        std::map<std::string, int> counts;
        for (const ErAttribute &a : e.second.attributes) counts[a.name]++;
        for (const auto &c : counts) {
            if (c.second > 1) {
                diag.warn("entity '" + e.second.label + "' has duplicate attribute '" + c.first + "'", e.second.line, 1,
                        "duplicate-attribute");
            }
        }
    }
    return d;
}


namespace {

constexpr double kErPadding = 20.0;
constexpr double kRowHeight = 42.75 / 16.0;
constexpr double kMinWidth = 100.0;
constexpr double kMinHeight = 75.0;

std::string crowMarker(SvgDocument &doc, const std::string &kind, const std::string &colour, bool start)
{
    const std::string name = "er-" + kind + (start ? "-start" : "-end");
    const std::string markerId = doc.svgId() + "_" + name;
    if (doc.hasMarker(markerId)) return "url(#" + markerId + ")";
    auto marker = std::make_shared<El>("marker");
    marker->attr("id", markerId);
    marker->attr("viewBox", "0 0 18 18");
    marker->attr("refX", start ? 0.0 : 18.0);
    marker->attr("refY", 9);
    marker->attr("markerUnits", "userSpaceOnUse");
    marker->attr("markerWidth", 18);
    marker->attr("markerHeight", 18);
    marker->attr("orient", "auto");
    std::vector<std::string> paths;
    std::vector<std::pair<double, double>> circles;
    const bool many = kind == "zero_many" || kind == "one_many";
    const bool optional = kind == "zero_one" || kind == "zero_many";
    const double barX = many ? 6.0 : 13.0;
    if (many) {
        paths.push_back("M 18 9 L 8 2 M 18 9 L 8 16 M 18 9 L 8 9");
    }
    if (kind == "one" || kind == "one_many" || kind == "zero_one") {
        paths.push_back("M " + fmt(barX) + " 2 L " + fmt(barX) + " 16");
    }
    if (kind == "one") {
        paths.push_back("M 8 2 L 8 16");
    }
    if (optional) {
        circles.push_back({ many ? 4.0 : 6.0, 9.0 });
    }
    for (const std::string &dd : paths) {
        auto path = std::make_shared<El>("path");
        path->attr("d", dd);
        path->attr("stroke", colour);
        path->attr("stroke-width", 1.4);
        path->attr("fill", "none");
        marker->add(path);
    }
    for (const auto &c : circles) {
        auto circle = std::make_shared<El>("circle");
        circle->attr("cx", c.first);
        circle->attr("cy", c.second);
        circle->attr("r", 3.2);
        circle->attr("stroke", colour);
        circle->attr("stroke-width", 1.4);
        circle->attr("fill", "white");
        marker->add(circle);
    }
    doc.addMarker(markerId, marker);
    return "url(#" + markerId + ")";
}

} // namespace

RenderResult ErDiagram::render(const RenderContext &ctx)
{
    const Theme &theme = ctx.theme;
    const double fontSize = theme.fontSize();
    std::shared_ptr<TextMeasurer> measurer = ctx.measurer;
    const double padding = ctx.num("er", "diagramPadding", kErPadding);

    auto doc = newDocument(ctx);
    {
        std::ostringstream css;
        css << "#{id} .entity-box { fill: " << theme.get("mainBkg", "#ECECFF") << "; stroke: "
            << theme.get("nodeBorder", "#9370DB") << "; stroke-width: 1px; }\n";
        css << "#{id} .entity-header { fill: " << theme.get("mainBkg", "#ECECFF") << "; stroke: "
            << theme.get("nodeBorder", "#9370DB") << "; stroke-width: 1px; }\n";
        css << "#{id} .entity-title, #{id} .entity-title tspan { fill: " << theme.get("primaryTextColor", "#333")
            << "; font-weight: bold; }\n";
        css << "#{id} .entity-row-even { fill: " << theme.get("attributeBackgroundColorEven", "#fff") << "; }\n";
        css << "#{id} .entity-row-odd { fill: " << theme.get("attributeBackgroundColorOdd", "#f4f4f4") << "; }\n";
        css << "#{id} .entity-attr, #{id} .entity-attr tspan { fill: " << theme.get("textColor", "#333") << "; }\n";
        css << "#{id} .entity-attr-key, #{id} .entity-attr-key tspan { fill: " << theme.get("textColor", "#333")
            << "; font-weight: bold; }\n";
        css << "#{id} .er-relation { stroke: " << theme.get("lineColor", "#333") << "; stroke-width: 1px; fill: none; }\n";
        css << "#{id} .er-label, #{id} .er-label tspan { fill: " << theme.get("textColor", "#333") << "; }\n";
        css << "#{id} .er-label-bkg { fill: " << theme.get("edgeLabelBackground", "#e8e8e8") << "; stroke: none; }";
        doc->addStyle(css.str());
    }

    struct Geo
    {
        double headerH;
        std::vector<double> columns; // 3
    };
    std::map<std::string, Geo> geometry;
    auto sizeOf = [&](const Entity &entity, Geo *geo) -> std::pair<double, double> {
        const double fs = fontSize;
        const double row = fs * kRowHeight;
        const double labelW = measurer->width(entity.label, fs, true);
        std::vector<double> columns { 0.0, 0.0, 0.0 };
        for (const ErAttribute &a : entity.attributes) {
            columns[0] = std::max(columns[0], measurer->width(a.type, fs));
            columns[1] = std::max(columns[1], measurer->width(a.name, fs));
            std::string extra = a.keys;
            if (!a.comment.empty()) extra += (extra.empty() ? "" : " ") + a.comment;
            columns[2] = std::max(columns[2], measurer->width(extra, fs, true));
        }
        geo->columns = columns;
        if (entity.attributes.empty()) {
            geo->headerH = std::max(fs * HTML_LABEL_LINE_HEIGHT + 3 * padding, kMinHeight);
            return { std::max(labelW + 2 * padding, kMinWidth), geo->headerH };
        }
        const double gaps = padding * (columns[2] > 0 ? 4 : 3);
        const double width = std::max({ labelW + 2 * padding, columns[0] + columns[1] + columns[2] + gaps, kMinWidth });
        const double height = row * (1 + static_cast<double>(entity.attributes.size()));
        geo->headerH = row;
        return { width, std::max(height, kMinHeight) };
    };

    DagreGraph g;
    for (const std::string &eid : entityOrder) {
        const Entity &entity = entities.at(eid);
        Geo geo {};
        const auto sz = sizeOf(entity, &geo);
        geometry[entity.id] = geo;
        g.addNode(entity.id, sz.first, sz.second);
    }
    for (const ErRelation &relation : relations) {
        if (!g.nodes.count(relation.source) || !g.nodes.count(relation.target)) continue;
        const double labelW = measurer->width(relation.label, fontSize * 0.9) + 8;
        DagreEdge &edge = g.addEdge(relation.source, relation.target);
        edge.labelWidth = relation.label.empty() ? 0 : labelW;
        edge.labelHeight = relation.label.empty() ? 0 : fontSize * 1.3;
    }

    const JsonValue block = ctx.sub("er");
    runLayout(g, direction, asNum(block["nodeSpacing"], 140), asNum(block["rankSpacing"], 80), 0.0);

    auto edges = std::make_shared<El>("g");
    edges->attr("class", "relations");
    auto nodes = std::make_shared<El>("g");
    nodes->attr("class", "entities");
    doc->root()->add(edges);
    doc->root()->add(nodes);

    for (const DagreEdge &edge : g.edges) {
        const ErRelation *relation = nullptr;
        for (const ErRelation &r : relations) {
            if (r.source == edge.v && r.target == edge.w) { relation = &r; break; }
        }
        if (relation == nullptr) continue;
        const std::vector<Point> points = clipPoints(g, edge);
        if (points.empty()) continue;
        const std::string colour = theme.get("lineColor", "#333");
        std::map<std::string, std::string> attrs;
        attrs["class"] = "er-relation";
        if (!relation->identifying) attrs["stroke-dasharray"] = "8 8";
        attrs["marker-start"] = crowMarker(*doc, relation->leftCard, colour, true);
        attrs["marker-end"] = crowMarker(*doc, relation->rightCard, colour, false);
        edges->add(edgePath(points, "basis", attrs));
        if (!relation->label.empty()) {
            const double width = measurer->width(relation->label, fontSize * 0.9);
            auto rect = std::make_shared<El>("rect");
            rect->attr("x", edge.labelX - width / 2 - 4);
            rect->attr("y", edge.labelY - fontSize * 0.7);
            rect->attr("width", width + 8);
            rect->attr("height", fontSize * 1.4);
            rect->attr("class", "er-label-bkg");
            edges->add(rect);
            edges->add(textElement({ relation->label }, edge.labelX, edge.labelY, "middle", fontSize * 0.9, 1.25,
                    "er-label"));
        }
    }

    for (const std::string &eid : entityOrder) {
        const Entity &entity = entities.at(eid);
        auto it = g.nodes.find(entity.id);
        if (it == g.nodes.end()) continue;
        const DagreNode &laid = it->second;
        const Geo &geo = geometry[entity.id];
        const double x = laid.x - laid.width / 2;
        const double y = laid.y - laid.height / 2;
        auto group = std::make_shared<El>("g");
        group->attr("class", "entity");
        group->attr("id", "entity-" + entity.id);
        auto box = std::make_shared<El>("rect");
        box->attr("x", x); box->attr("y", y);
        box->attr("width", laid.width); box->attr("height", laid.height);
        box->attr("rx", 2); box->attr("ry", 2); box->attr("class", "entity-box");
        group->add(box);
        auto header = std::make_shared<El>("rect");
        header->attr("x", x); header->attr("y", y);
        header->attr("width", laid.width); header->attr("height", geo.headerH);
        header->attr("rx", 2); header->attr("ry", 2); header->attr("class", "entity-header");
        group->add(header);
        group->add(textElement({ entity.label }, laid.x, y + geo.headerH / 2, "middle", fontSize, 1.25,
                "entity-title"));
        double cursor = y + geo.headerH;
        for (std::size_t index = 0; index < entity.attributes.size(); ++index) {
            const ErAttribute &a = entity.attributes[index];
            const std::string rowClass = (index % 2) ? "entity-row-odd" : "entity-row-even";
            auto rowRect = std::make_shared<El>("rect");
            rowRect->attr("x", x); rowRect->attr("y", cursor);
            rowRect->attr("width", laid.width); rowRect->attr("height", fontSize * kRowHeight);
            rowRect->attr("class", rowClass);
            group->add(rowRect);
            const double mid = cursor + fontSize * kRowHeight / 2;
            double cx = x + padding;
            group->add(textElement({ a.type }, cx, mid, "start", fontSize, 1.25, "entity-attr"));
            cx += geo.columns[0] + padding;
            group->add(textElement({ a.name }, cx, mid, "start", fontSize, 1.25, "entity-attr"));
            std::string extra = a.keys;
            if (!a.comment.empty()) extra += (extra.empty() ? "" : " ") + a.comment;
            if (!extra.empty()) {
                cx += geo.columns[1] + padding;
                group->add(textElement({ extra }, cx, mid, "start", fontSize, 1.25, "entity-attr-key"));
            }
            cursor += fontSize * kRowHeight;
        }
        nodes->add(group);
    }

    return finish(ctx, *doc, g.width, g.height, 8.0);
}

} // namespace tqtw_mermaid2svg
