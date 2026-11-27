/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2sbase.h"

#include "tqtw_m2serrors.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace tqtw_mermaid2svg {

namespace {

std::string trim(const std::string &s)
{
    std::size_t start = 0;
    std::size_t end = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

bool startsWith(const std::string &s, const std::string &prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

} // namespace

double asNumber(const std::string &value, double fallback)
{
    if (value.empty()) {
        return fallback;
    }
    char *end = nullptr;
    const double number = std::strtod(value.c_str(), &end);
    if (end == value.c_str()) {
        return fallback;
    }
    return std::isfinite(number) ? number : fallback;
}

JsonValue RenderContext::sub(const std::string &key) const
{
    const JsonValue &value = config[key];
    return value.isObject() ? value : JsonValue::object();
}

double RenderContext::num(const std::string &key, const std::string &name, double fallback) const
{
    const JsonValue block = sub(key);
    const JsonValue value = block[name];
    if (value.isNull()) {
        return fallback;
    }
    if (value.type() == JsonValue::Number) {
        return value.toNumber();
    }
    return asNumber(value.toString(), fallback);
}

bool RenderContext::useMaxWidth(const std::string &key) const
{
    const JsonValue block = sub(key);
    if (block.contains("useMaxWidth")) {
        return block["useMaxWidth"].toBool();
    }
    return config["useMaxWidth"].isNull() ? true : config["useMaxWidth"].toBool();
}

std::string SourceLine::stripped() const
{
    return trim(text);
}

std::vector<SourceLine> iterLines(const Source &source, bool skipFirst)
{
    std::vector<SourceLine> out;
    bool started = !skipFirst;
    const std::vector<std::string> lines = source.lines();
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const std::string &raw = lines[index];
        if (trim(raw).empty()) {
            continue;
        }
        const int number = source.absLine(static_cast<int>(index) + 1);
        if (!started) {
            started = true;
            continue;
        }
        int indent = 0;
        while (indent < static_cast<int>(raw.size()) && (raw[indent] == ' ' || raw[indent] == '\t')) {
            ++indent;
        }
        out.push_back({ trim(raw), number, indent });
    }
    return out;
}

std::pair<std::string, int> firstLine(const Source &source)
{
    const std::vector<std::string> lines = source.lines();
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (!trim(lines[index]).empty()) {
            return { trim(lines[index]), source.absLine(static_cast<int>(index) + 1) };
        }
    }
    return { "", 1 };
}

std::string unquote(const std::string &textIn)
{
    const std::string text = trim(textIn);
    if (text.size() >= 2 && text.front() == text.back() && (text.front() == '"' || text.front() == '\'')) {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

bool consumeCommon(DiagramBase &diagram, const SourceLine &line)
{
    const std::string text = line.stripped();
    // accTitle:
    if (text.size() >= 9 && (text.compare(0, 9, "accTitle:") == 0 || text.compare(0, 9, "accTitle:") == 0)) {
        const std::size_t colon = text.find(':');
        diagram.hasAccTitle = true;
        diagram.accTitle = trim(text.substr(colon + 1));
        return true;
    }
    // accDescr: (one-line form; the block form is lifted out by preprocess)
    if (text.size() >= 9 && startsWith(text, "accDescr:") ) {
        const std::size_t colon = text.find(':');
        diagram.hasAccDescr = true;
        diagram.accDescr = trim(text.substr(colon + 1));
        return true;
    }
    // title: or title <text>
    if (startsWith(text, "title ")) {
        diagram.hasTitle = true;
        diagram.title = unquote(trim(text.substr(6)));
        return true;
    }
    if (startsWith(text, "title:")) {
        diagram.hasTitle = true;
        diagram.title = unquote(trim(text.substr(6)));
        return true;
    }
    if (startsWith(text, "title\n") || text == "title") {
        return true;
    }
    return false;
}

std::vector<std::string> fontCss(const RenderContext &ctx)
{
    if (!ctx.measurer || ctx.measurer->family().empty()) {
        return {};
    }
    return { "#{id} text { font-family: \"" + ctx.measurer->family() + "\"; }" };
}

std::string markerDefs(SvgDocument &doc, const std::string &name, const std::string &color, const std::string &kind)
{
    const std::string markerId = doc.svgId() + "_" + name;
    if (!doc.hasMarker(markerId)) {
        double refX = 5;
        auto marker = std::make_shared<El>("marker");
        marker->attr("id", markerId);
        marker->attr("viewBox", "0 0 10 10");
        std::shared_ptr<El> shape;
        if (kind == "arrow") {
            refX = 9;
            shape = std::make_shared<El>("path");
            shape->attr("d", "M 0 0 L 10 5 L 0 10 z");
            shape->attr("fill", color);
            shape->attr("stroke", "none");
        } else if (kind == "circle") {
            refX = 10;
            shape = std::make_shared<El>("circle");
            shape->attr("cx", "5");
            shape->attr("cy", "5");
            shape->attr("r", "4");
            shape->attr("fill", color);
            shape->attr("stroke", color);
        } else if (kind == "cross") {
            refX = 10;
            shape = std::make_shared<El>("path");
            shape->attr("d", "M 1 1 L 9 9 M 9 1 L 1 9");
            shape->attr("stroke", color);
            shape->attr("stroke-width", "2");
            shape->attr("fill", "none");
        } else if (kind == "point") {
            refX = 5;
            shape = std::make_shared<El>("circle");
            shape->attr("cx", "5");
            shape->attr("cy", "5");
            shape->attr("r", "3");
            shape->attr("fill", color);
            shape->attr("stroke", "none");
        }
        marker->attr("refX", refX);
        marker->attr("refY", "5");
        marker->attr("markerUnits", "userSpaceOnUse");
        marker->attr("markerWidth", "12");
        marker->attr("markerHeight", "12");
        marker->attr("orient", "auto");
        if (shape) {
            marker->add(shape);
        }
        doc.addMarker(markerId, marker);
    }
    return "url(#" + markerId + ")";
}

std::shared_ptr<SvgDocument> DiagramBase::newDocument(const RenderContext &ctx) const
{
    auto doc = std::make_shared<SvgDocument>(ctx.svgId);
    doc->ariaRoledescription = role;
    doc->background = ctx.background;
    doc->title = hasAccTitle ? accTitle : (hasTitle ? title : std::string());
    doc->desc = hasAccDescr ? accDescr : std::string();
    return doc;
}

RenderResult DiagramBase::finish(const RenderContext &ctx, SvgDocument &doc, double width, double height,
        double pad, double padY) const
{
    // Resolve the {id} placeholder every stylesheet block may carry.
    doc.replaceInStyles("{id}", ctx.svgId);
    for (const std::string &rule : fontCss(ctx)) {
        doc.prependStyle(rule);
    }
    if (!ctx.extraCss.empty()) {
        doc.addStyle(ctx.extraCss);
    }
    const double useWidth = std::max(width, 1.0);
    const double useHeight = std::max(height, 1.0);
    const double usePadY = padY < 0 ? pad : padY;
    const double totalW = useWidth + pad * 2;
    const double totalH = useHeight + usePadY * 2;
    const std::string svg = doc.toString(totalW, totalH, { -pad, -usePadY, totalW, totalH },
            ctx.useMaxWidth(configKey.empty() ? diagramId : configKey));
    RenderResult result;
    result.svg = svg;
    result.width = totalW;
    result.height = totalH;
    if (!doc.title.empty()) {
        result.hasTitle = true;
        result.title = doc.title;
    }
    if (!doc.desc.empty()) {
        result.hasDesc = true;
        result.desc = doc.desc;
    }
    return result;
}

} // namespace tqtw_mermaid2svg
