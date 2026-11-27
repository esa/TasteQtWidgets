/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2sclass.h"

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

struct RelToken
{
    std::string token;
    std::string kind;
    std::string arrow; // start | end | none
    bool dotted;
};

const std::vector<RelToken> &relationTokens()
{
    static const std::vector<RelToken> k = {
        { "<|--", "extension", "start", false }, { "--|>", "extension", "end", false },
        { "<|..", "realization", "start", true }, { "..|>", "realization", "end", true },
        { "*--", "composition", "start", false }, { "--*", "composition", "end", false },
        { "o--", "aggregation", "start", false }, { "--o", "aggregation", "end", false },
        { "<--", "association", "start", false }, { "-->", "association", "end", false },
        { "<..", "dependency", "start", true }, { "..>", "dependency", "end", true },
        { "--", "link", "none", false }, { "..", "link", "none", true },
    };
    return k;
}

std::string markerFor(const std::string &kind)
{
    if (kind == "extension" || kind == "realization") return "triangle";
    if (kind == "composition" || kind == "aggregation") return "diamond";
    if (kind == "association" || kind == "dependency") return "open";
    return "";
}

std::string formatGeneric(const std::string &name)
{
    const std::size_t count = std::count(name.begin(), name.end(), '~');
    if (count < 2) return name;
    std::string out = name;
    const std::size_t first = out.find('~');
    out.replace(first, 1, "\xc2\xab"); // «
    const std::size_t second = out.find('~');
    out.replace(second, 1, "\xc2\xbb"); // »
    return out;
}

std::pair<std::string, std::string> splitCardinality(const std::string &textIn, bool trailing)
{
    const std::string text = textIn;
    if (trailing) {
        std::smatch match;
        const std::regex re("\"([^\"]*)\"\\s*$");
        if (std::regex_search(text, match, re)) {
            const std::string card = match[1].str();
            std::string rest = trim(text.substr(0, match.position()));
            return { rest, card };
        }
    } else {
        std::smatch match;
        const std::regex re("^\\s*\"([^\"]*)\"");
        if (std::regex_search(text, match, re)) {
            const std::string card = match[1].str();
            std::string rest = trim(text.substr(match.position() + match.length()));
            return { rest, card };
        }
    }
    return { trim(text), "" };
}

} // namespace

ClassDiagram::ClassDiagram(const Source &source, Diagnostics *diagnostics)
{
    this->source = source;
    this->diagnostics = diagnostics;
    diagramId = "class";
    role = "class";
    configKey = "class";
    hasTitle = source.hasTitle;
    title = source.title;
    hasAccDescr = source.hasAccDescr;
    accDescr = source.accDescr;
}

namespace {

struct Stack
{
    std::vector<std::pair<std::string, std::string>> items;
};

ClassDef &classDef(ClassDiagram &d, const std::string &nameIn, int line, const Stack &stack)
{
    std::string name = unquote(trim(nameIn));
    // strip generics for the id
    std::string base;
    {
        std::string t = name;
        std::regex re("~[^~]*~");
        base = trim(std::regex_replace(t, re, ""));
    }
    auto it = d.classes.find(base);
    if (it == d.classes.end()) {
        ClassDef def;
        def.id = base;
        def.label = formatGeneric(name);
        def.line = line;
        for (auto rit = stack.items.rbegin(); rit != stack.items.rend(); ++rit) {
            if (rit->first == "namespace") {
                def.hasNamespace = true;
                def.nameSpace = rit->second;
                break;
            }
        }
        d.classOrder.push_back(base);
        return d.classes.emplace(base, def).first->second;
    }
    return it->second;
}

std::string formatMember(const std::string &text)
{
    const std::string t = trim(text);
    return t;
}

} // namespace

std::shared_ptr<ClassDiagram> ClassDiagram::parse(const Source &source, Diagnostics *diagnostics)
{
    auto d = std::make_shared<ClassDiagram>(source, diagnostics);
    auto &diag = *diagnostics;
    const auto header = firstLine(source);
    {
        std::smatch match;
        const std::regex re(R"(\b(TB|TD|BT|LR|RL)\b)");
        if (std::regex_search(header.first, match, re)) {
            d->direction = match[1].str() == "TD" ? "TB" : match[1].str();
        }
    }
    Stack stack;

    auto member = [&](const std::string &classId, const std::string &textIn) {
        const std::string text = trim(textIn);
        if (text.empty()) return;
        if (text.size() >= 4 && text.substr(0, 2) == "<<" && text.substr(text.size() - 2) == ">>") {
            d->classes[classId].annotation = trim(text.substr(2, text.size() - 4));
            return;
        }
        std::string body = text;
        if (body.find(":::") != std::string::npos) {
            const std::size_t pos = body.find(":::");
            d->classes[classId].cssClass = trim(body.substr(pos + 3));
            body = trim(body.substr(0, pos));
        }
        auto it = d->classes.find(classId);
        if (it == d->classes.end()) return;
        if (body.find('(') != std::string::npos && body.find(')') != std::string::npos) {
            it->second.methods.push_back(formatMember(body));
        } else {
            it->second.attributes.push_back(formatMember(body));
        }
    };

    auto classStmt = [&](const SourceLine &line) {
        std::string rest = trim(line.stripped().substr(std::string("class").size()));
        bool opens = !rest.empty() && rest.back() == '{';
        if (opens) rest = trim(rest.substr(0, rest.size() - 1));
        std::string labelOverride;
        bool hasOverride = false;
        {
            std::smatch match;
            const std::regex re("^(.+?)\\s*\\[\\s*\"(.*)\"\\s*\\]$");
            if (std::regex_match(rest, match, re)) {
                rest = trim(match[1].str());
                labelOverride = match[2].str();
                hasOverride = true;
            }
        }
        std::string css;
        if (rest.find(":::") != std::string::npos) {
            const std::size_t pos = rest.find(":::");
            css = trim(rest.substr(pos + 3));
            rest = trim(rest.substr(0, pos));
        }
        std::string body;
        if (rest.find('{') != std::string::npos) {
            const std::size_t pos = rest.find('{');
            body = rest.substr(pos + 1);
            rest = trim(rest.substr(0, pos));
            while (!body.empty() && (body.back() == '}' || std::isspace(static_cast<unsigned char>(body.back()))))
                body.pop_back();
            body = trim(body);
            opens = false;
        }
        ClassDef &entry = classDef(*d, rest, line.number, stack);
        if (hasOverride) entry.label = labelOverride;
        if (!css.empty()) entry.cssClass = css;
        if (!body.empty()) {
            std::size_t start = 0;
            while (start <= body.size()) {
                const std::size_t nl = body.find('\n', start);
                const std::string piece = body.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
                if (!trim(piece).empty()) member(entry.id, trim(piece));
                if (nl == std::string::npos) break;
                start = nl + 1;
            }
        }
        if (opens) stack.items.push_back({ "class", entry.id });
    };

    auto namespaceStmt = [&](const SourceLine &line) {
        std::string rest = trim(line.stripped().substr(std::string("namespace").size()));
        while (!rest.empty() && rest.back() == '{') rest.pop_back();
        rest = trim(rest);
        const std::string name = unquote(rest);
        std::string parent;
        for (auto rit = stack.items.rbegin(); rit != stack.items.rend(); ++rit) {
            if (rit->first == "namespace") { parent = rit->second; break; }
        }
        d->namespaces[name] = parent;
        d->namespaceOrder.push_back(name);
        stack.items.push_back({ "namespace", name });
    };

    auto noteStmt = [&](const SourceLine &line) {
        std::string text = line.stripped();
        std::smatch match;
        std::regex re("^note(?:\\s+for\\s+(\\S+))?\\s+\"(.*)\"\\s*$", std::regex::icase);
        if (!std::regex_match(text, match, re)) {
            std::regex re2("^note(?:\\s+for\\s+(\\S+))?\\s+(.*)$", std::regex::icase);
            if (!std::regex_match(text, match, re2)) {
                throw ParseError("could not parse note", line.number, 1);
            }
        }
        ClassNote n;
        n.text = unquote(match[2].str());
        if (match[1].matched) { n.hasTarget = true; n.target = match[1].str(); }
        n.line = line.number;
        d->notes.push_back(n);
    };

    auto relationStmt = [&](const SourceLine &line) -> bool {
        std::string text = line.stripped();
        std::string label;
        if (text.find(':') != std::string::npos) {
            const std::size_t pos = text.find(':');
            label = trim(text.substr(pos + 1));
            text = trim(text.substr(0, pos));
        }
        for (const RelToken &tok : relationTokens()) {
            const std::size_t index = text.find(tok.token);
            if (index == std::string::npos || index == 0) continue;
            std::string left = trim(text.substr(0, index));
            std::string right = trim(text.substr(index + tok.token.size()));
            if (left.empty() || right.empty()) continue;
            std::string sourceCard, targetCard;
            {
                auto sc = splitCardinality(left, true);
                left = sc.first; sourceCard = sc.second;
                auto tc = splitCardinality(right, false);
                right = tc.first; targetCard = tc.second;
            }
            if (left.empty() || right.empty()) continue;
            ClassDef &src = classDef(*d, left, line.number, stack);
            ClassDef &tgt = classDef(*d, right, line.number, stack);
            Relation r;
            r.source = src.id;
            r.target = tgt.id;
            r.kind = tok.kind;
            r.arrow = tok.arrow;
            r.dotted = tok.dotted;
            r.label = label;
            r.sourceCard = sourceCard;
            r.targetCard = targetCard;
            r.line = line.number;
            d->relations.push_back(r);
            return true;
        }
        return false;
    };

    for (const SourceLine &line : iterLines(source)) {
        const std::string text = line.stripped();
        if (consumeCommon(*d, line)) continue;
        if (text == "}") {
            if (!stack.items.empty()) stack.items.pop_back();
            else diag.error("'}' without a matching '{'", line.number, 1, "block");
            continue;
        }
        if (startsWith(lower(text), "direction ")) {
            std::string dir = trim(text.substr(std::string("direction").size()));
            std::transform(dir.begin(), dir.end(), dir.begin(), [](unsigned char c) { return std::toupper(c); });
            d->direction = dir == "TD" ? "TB" : dir;
            continue;
        }
        if (startsWith(lower(text), "namespace ")) {
            namespaceStmt(line);
            continue;
        }
        if (startsWith(lower(text), "note")) {
            noteStmt(line);
            continue;
        }
        if (text.size() >= 2 && text.substr(0, 2) == "<<" && text.find(">>") != std::string::npos) {
            std::smatch match;
            const std::regex re("^<<(.+?)>>\\s*(.*)$");
            if (!std::regex_match(text, match, re)) {
                throw ParseError("could not parse annotation", line.number, 1);
            }
            const std::string name = trim(match[2].str());
            if (name.empty()) {
                if (!stack.items.empty() && stack.items.back().first == "class") {
                    d->classes[stack.items.back().second].annotation = trim(match[1].str());
                } else {
                    diag.warn("annotation has no target class", line.number, 1, "annotation");
                }
            } else {
                classDef(*d, name, line.number, stack).annotation = trim(match[1].str());
            }
            continue;
        }
        if (startsWith(lower(text), "class ") || lower(text) == "class") {
            classStmt(line);
            continue;
        }
        if (startsWith(lower(text), "cssclass ") || startsWith(lower(text), "link ")
                || startsWith(lower(text), "callback ") || startsWith(lower(text), "click ")) {
            continue;
        }
        if (!stack.items.empty() && stack.items.back().first == "class") {
            member(stack.items.back().second, text);
            continue;
        }
        if (relationStmt(line)) continue;
        if (text.find(':') != std::string::npos) {
            const std::string name = trim(text.substr(0, text.find(':')));
            const std::string mem = trim(text.substr(text.find(':') + 1));
            ClassDef &target = classDef(*d, name, line.number, stack);
            member(target.id, mem);
            continue;
        }
        if (!text.empty()) {
            throw ParseError("could not parse class diagram statement", line.number, 1);
        }
    }

    if (d->classes.empty()) diag.warn("class diagram has no classes", 0, 0, "empty");
    for (const ClassNote &note : d->notes) {
        if (note.hasTarget && !d->classes.count(note.target)) {
            diag.warn("note refers to unknown class '" + note.target + "'", note.line, 1, "unknown-class");
        }
    }
    return d;
}



namespace {

constexpr double kClassPadding = 12.0;

} // namespace

RenderResult ClassDiagram::render(const RenderContext &ctx)
{
    const Theme &theme = ctx.theme;
    const double fontSize = theme.fontSize();
    std::shared_ptr<TextMeasurer> measurer = ctx.measurer;
    const double padding = ctx.num("class", "padding", kClassPadding);

    auto doc = newDocument(ctx);
    {
        std::ostringstream css;
        css << "#{id} .class-box { fill: " << theme.get("mainBkg", "#ECECFF") << "; stroke: "
            << theme.get("nodeBorder", "#9370DB") << "; stroke-width: 1px; }\n";
        css << "#{id} .class-divider { stroke: " << theme.get("nodeBorder", "#9370DB") << "; stroke-width: 1px; }\n";
        css << "#{id} .class-title, #{id} .class-title tspan { fill: " << theme.get("classText", "#333")
            << "; font-weight: bold; }\n";
        css << "#{id} .class-member, #{id} .class-member tspan { fill: " << theme.get("classText", "#333") << "; }\n";
        css << "#{id} .class-annotation, #{id} .class-annotation tspan { fill: " << theme.get("classText", "#333")
            << "; }\n";
        css << "#{id} .relation { stroke: " << theme.get("lineColor", "#333") << "; stroke-width: 1.5px; fill: none; }\n";
        css << "#{id} .relation-label, #{id} .relation-label tspan, #{id} .relation-card, #{id} .relation-card tspan { fill: "
            << theme.get("textColor", "#333") << "; }\n";
        css << "#{id} .cluster-rect { fill: " << theme.get("clusterBkg", "#ffffde") << "; stroke: "
            << theme.get("clusterBorder", "#aaaa33") << "; }\n";
        css << "#{id} .cluster-label, #{id} .cluster-label tspan { fill: " << theme.get("primaryTextColor", "#333")
            << "; font-weight: bold; }";
        doc->addStyle(css.str());
    }

    struct Geo
    {
        std::string annotation, title;
        double memberX, annotationY, titleY, attrsY, methodsY, row;
    };
    std::map<std::string, Geo> geometry;
    auto boxSize = [&](const ClassDef &entry, Geo *geo) -> std::pair<double, double> {
        const double fs = fontSize;
        const double row = fs * HTML_LABEL_LINE_HEIGHT;
        const double gap = padding;
        const std::string annotation = entry.annotation.empty() ? "" : "\xc2\xab" + entry.annotation + "\xc2\xbb";
        const std::string title = entry.label.empty() ? entry.id : entry.label;
        const double annotationW = annotation.empty() ? 0.0 : measurer->width(annotation, fs);
        const double titleW = measurer->width(title, fs, true);
        double attrsW = 0, methodsW = 0;
        for (const std::string &a : entry.attributes) attrsW = std::max(attrsW, measurer->width(a, fs));
        for (const std::string &m : entry.methods) methodsW = std::max(methodsW, measurer->width(m, fs));
        const double left = std::max(annotationW / 2, titleW / 2);
        const double right = std::max({ annotationW / 2, titleW / 2, attrsW, methodsW });
        const double contentW = left + right;
        const double annotationH = annotation.empty() ? 0.0 : row;
        const double attrsH = entry.attributes.empty() ? gap / 2
                                                       : static_cast<double>(entry.attributes.size()) * row;
        const double titleY = annotationH;
        const double attrsY = annotationH + row + gap * 2;
        const double methodsY = annotationH + row + attrsH + gap * 4;
        double bottom = -row / 2;
        {
            std::vector<std::pair<double, int>> groups = { { 0.0, annotation.empty() ? 0 : 1 }, { titleY, 1 },
                { attrsY, static_cast<int>(entry.attributes.size()) }, { methodsY,
                    static_cast<int>(entry.methods.size()) } };
            for (const auto &g : groups) {
                if (g.second) bottom = std::max(bottom, g.first + g.second * row - row / 2);
            }
        }
        double contentH = bottom + row / 2;
        const bool empty = entry.attributes.empty() && entry.methods.empty();
        if (empty) contentH += gap;
        else if (!entry.attributes.empty() && entry.methods.empty()) contentH += gap * 2;
        const double extraH = empty ? gap * 2 : 0.0;
        const double top = -contentH / 2 - gap - (empty ? gap : 0.0);
        geo->annotation = annotation;
        geo->title = title;
        geo->memberX = -contentW / 2;
        geo->annotationY = top + gap + row / 2;
        geo->titleY = top + gap + titleY + row / 2;
        geo->attrsY = top + gap + attrsY + row / 2;
        geo->methodsY = top + gap + methodsY + row / 2;
        geo->row = row;
        return { contentW + 2 * gap, contentH + 2 * gap + extraH };
    };

    DagreGraph g;
    for (const std::string &cid : classOrder) {
        const ClassDef &entry = classes.at(cid);
        Geo geo {};
        const auto sz = boxSize(entry, &geo);
        geometry[entry.id] = geo;
        DagreNode &n = g.addNode(entry.id, sz.first, sz.second);
        if (entry.hasNamespace) { n.parent = entry.nameSpace; n.hasParent = true; }
    }
    for (const std::string &name : namespaceOrder) {
        DagreCluster &c = g.addCluster(name, name);
        const std::string parent = namespaces.count(name) ? namespaces.at(name) : std::string();
        if (!parent.empty()) { c.parent = parent; c.hasParent = true; }
        c.labelHeight = fontSize * 1.6;
    }
    for (const Relation &relation : relations) {
        if (!g.nodes.count(relation.source) || !g.nodes.count(relation.target)) continue;
        const double labelW = measurer->width(relation.label, fontSize * 0.9) + 8;
        DagreEdge &edge = g.addEdge(relation.source, relation.target);
        edge.labelWidth = relation.label.empty() ? 0 : labelW;
        edge.labelHeight = relation.label.empty() ? 0 : fontSize * 1.2;
    }

    const JsonValue block = ctx.sub("class");
    runLayout(g, direction, asNum(block["nodeSpacing"], 50), asNum(block["rankSpacing"], 50), 0.0);

    auto clusters = std::make_shared<El>("g");
    clusters->attr("class", "clusters");
    auto edges = std::make_shared<El>("g");
    edges->attr("class", "relations");
    auto nodes = std::make_shared<El>("g");
    nodes->attr("class", "classes");
    doc->root()->add(clusters);
    doc->root()->add(edges);
    doc->root()->add(nodes);

    std::vector<DagreCluster> clusterList;
    for (const auto &e : g.clusters) clusterList.push_back(e.second);
    std::sort(clusterList.begin(), clusterList.end(),
            [](const DagreCluster &a, const DagreCluster &b) { return a.width > b.width; });
    for (const DagreCluster &cluster : clusterList) {
        if (!cluster.width) continue;
        auto group = std::make_shared<El>("g");
        group->attr("class", "cluster");
        auto rect = std::make_shared<El>("rect");
        rect->attr("x", cluster.x - cluster.width / 2);
        rect->attr("y", cluster.y - cluster.height / 2);
        rect->attr("width", cluster.width);
        rect->attr("height", cluster.height);
        rect->attr("rx", 4); rect->attr("ry", 4); rect->attr("class", "cluster-rect");
        group->add(rect);
        group->add(textElement({ cluster.label }, cluster.x, cluster.y - cluster.height / 2 + fontSize, "middle",
                fontSize, 1.25, "cluster-label"));
        clusters->add(group);
    }

    for (const DagreEdge &edge : g.edges) {
        const Relation *relation = nullptr;
        for (const Relation &r : relations) {
            if (r.source == edge.v && r.target == edge.w) { relation = &r; break; }
        }
        if (relation == nullptr) continue;
        const std::vector<Point> points = clipPoints(g, edge);
        if (points.empty()) continue;
        const std::string colour = theme.get("lineColor", "#333");
        std::map<std::string, std::string> attrs;
        attrs["class"] = "relation relation-" + relation->kind;
        if (relation->dotted) attrs["stroke-dasharray"] = "5 5";
        const std::string markerKind = markerFor(relation->kind);
        if (!markerKind.empty()) {
            const bool filled = relation->kind == "composition";
            const std::string fill = filled ? colour : theme.get("mainBkg", "#fff");
            if (relation->arrow == "start" || relation->arrow == "end") {
                const bool atStart = relation->arrow == "start";
                attrs[std::string("marker-") + relation->arrow]
                        = hollowMarker(*doc, "class-" + relation->kind, colour, fill, markerKind, atStart);
            }
        }
        edges->add(edgePath(points, "basis", attrs));
        if (!relation->label.empty()) {
            edges->add(textElement({ relation->label }, edge.labelX, edge.labelY, "middle", fontSize * 0.9, 1.25,
                    "relation-label"));
        }
        if (!relation->targetCard.empty()) {
            edges->add(textElement({ relation->targetCard }, points.back().first, points.back().second - 8, "middle",
                    fontSize * 0.85, 1.25, "relation-card"));
        }
        if (!relation->sourceCard.empty()) {
            edges->add(textElement({ relation->sourceCard }, points.front().first, points.front().second - 8, "middle",
                    fontSize * 0.85, 1.25, "relation-card"));
        }
    }

    for (const std::string &cid : classOrder) {
        const ClassDef &entry = classes.at(cid);
        auto it = g.nodes.find(entry.id);
        if (it == g.nodes.end()) continue;
        const DagreNode &laid = it->second;
        const Geo &geo = geometry[entry.id];
        const double x = laid.x - laid.width / 2;
        const double y = laid.y - laid.height / 2;
        std::string cls = "class-node";
        if (!entry.cssClass.empty()) cls += " cls-" + entry.cssClass;
        auto group = std::make_shared<El>("g");
        group->attr("class", cls);
        group->attr("id", "class-" + entry.id);
        auto rect = std::make_shared<El>("rect");
        rect->attr("x", x); rect->attr("y", y);
        rect->attr("width", laid.width); rect->attr("height", laid.height);
        rect->attr("class", "class-box");
        group->add(rect);
        const double memberX = laid.x + geo.memberX;
        if (!geo.annotation.empty()) {
            group->add(textElement({ geo.annotation }, laid.x, laid.y + geo.annotationY, "middle", fontSize, 1.25,
                    "class-annotation"));
        }
        group->add(textElement({ geo.title }, laid.x, laid.y + geo.titleY, "middle", fontSize, 1.25, "class-title"));
        for (std::size_t i = 0; i < entry.attributes.size(); ++i) {
            group->add(textElement({ entry.attributes[i] }, memberX, laid.y + geo.attrsY + i * geo.row, "start",
                    fontSize, 1.25, "class-member"));
        }
        for (std::size_t i = 0; i < entry.methods.size(); ++i) {
            group->add(textElement({ entry.methods[i] }, memberX, laid.y + geo.methodsY + i * geo.row, "start",
                    fontSize, 1.25, "class-member"));
        }
        nodes->add(group);
    }

    return finish(ctx, *doc, g.width, g.height, asNum(block["diagramPadding"], 8));
}

} // namespace tqtw_mermaid2svg
