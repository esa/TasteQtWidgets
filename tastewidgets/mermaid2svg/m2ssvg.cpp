/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2ssvg.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace tqtw_mermaid2svg {

namespace {

//: Tags that must never be collapsed to `<tag/>`.
bool neverEmpty(const std::string &tag)
{
    static const std::set<std::string> tags = { "text", "tspan", "style", "script", "textPath", "g", "svg", "a" };
    return tags.count(tag) != 0;
}

std::string attributeName(const std::string &raw)
{
    std::string out;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '_') {
            if (i + 1 < raw.size() && raw[i + 1] == '_') {
                out.push_back(':');
                ++i;
            } else {
                out.push_back('-');
            }
        } else {
            out.push_back(raw[i]);
        }
    }
    return out;
}

} // namespace

std::string escapeXml(const std::string &text)
{
    std::string out;
    for (unsigned char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default:
            if (c == '\n' || c == '\t' || c >= 0x20) {
                out.push_back(static_cast<char>(c));
            }
            // else: a control character XML 1.0 forbids; drop it.
        }
    }
    return out;
}

std::string fmt(double value)
{
    if (!std::isfinite(value)) {
        return "0";
    }
    const double rounded = std::round(value * 1000.0) / 1000.0;
    if (rounded == static_cast<long long>(rounded)) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(rounded));
        return buf;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.10g", rounded);
    return buf;
}

El::El(std::string tag)
    : m_tag(std::move(tag))
{
}

El &El::set(const std::string &key, const std::string &value)
{
    m_attrs[attributeName(key)] = value;
    return *this;
}

El &El::set(const std::string &key, double value)
{
    m_attrs[attributeName(key)] = fmt(value);
    return *this;
}

El &El::set(const std::string &key, int value)
{
    m_attrs[attributeName(key)] = std::to_string(value);
    return *this;
}

El &El::attr(const std::string &name, const std::string &value)
{
    m_attrs[name] = value;
    return *this;
}

El &El::attr(const std::string &name, double value)
{
    m_attrs[name] = fmt(value);
    return *this;
}

El &El::child(const std::string &tag)
{
    auto el = std::make_shared<El>(tag);
    Child c;
    c.kind = Child::Element;
    c.el = el;
    m_children.push_back(std::move(c));
    return *el;
}

El &El::add(std::shared_ptr<El> child)
{
    if (child) {
        Child c;
        c.kind = Child::Element;
        c.el = std::move(child);
        m_children.push_back(std::move(c));
    }
    return *this;
}

El &El::addText(const std::string &text)
{
    setText(text);
    return *this;
}

El &El::addRaw(const std::string &content)
{
    Child c;
    c.kind = Child::Raw;
    c.raw = content;
    m_children.push_back(std::move(c));
    return *this;
}

void El::setText(const std::string &text)
{
    m_text = text;
    m_hasText = true;
}

void El::render(std::vector<std::string> &out, int indent) const
{
    const std::string pad(static_cast<std::size_t>(indent) * 2, ' ');
    std::string open = pad + "<" + m_tag;
    for (const auto &entry : m_attrs) {
        if (entry.second.empty()) {
            continue;
        }
        open += " " + entry.first + "=\"" + escapeXml(entry.second) + "\"";
    }
    const bool hasBody = !m_children.empty() || m_hasText;
    if (!hasBody && !neverEmpty(m_tag)) {
        out.push_back(open + "/>");
        return;
    }
    open += ">";
    if (m_hasText) {
        open += escapeXml(m_text);
        if (m_children.empty()) {
            open += "</" + m_tag + ">";
            out.push_back(open);
            return;
        }
        out.push_back(open);
    } else {
        out.push_back(open);
    }
    for (const Child &c : m_children) {
        if (c.kind == Child::Element && c.el) {
            c.el->render(out, indent + 1);
        } else {
            out.push_back(pad + "  " + c.raw);
        }
    }
    out.push_back(pad + "</" + m_tag + ">");
}

std::string El::toString(int indent) const
{
    std::vector<std::string> out;
    render(out, indent);
    std::string joined;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (i > 0) {
            joined += "\n";
        }
        joined += out[i];
    }
    return joined;
}

Path &Path::M(double x, double y)
{
    m_parts.push_back("M" + fmt(x) + "," + fmt(y));
    return *this;
}

Path &Path::L(double x, double y)
{
    m_parts.push_back("L" + fmt(x) + "," + fmt(y));
    return *this;
}

Path &Path::H(double x)
{
    m_parts.push_back("H" + fmt(x));
    return *this;
}

Path &Path::V(double y)
{
    m_parts.push_back("V" + fmt(y));
    return *this;
}

Path &Path::C(double x1, double y1, double x2, double y2, double x, double y)
{
    m_parts.push_back("C" + fmt(x1) + "," + fmt(y1) + " " + fmt(x2) + "," + fmt(y2) + " " + fmt(x) + "," + fmt(y));
    return *this;
}

Path &Path::Q(double x1, double y1, double x, double y)
{
    m_parts.push_back("Q" + fmt(x1) + "," + fmt(y1) + " " + fmt(x) + "," + fmt(y));
    return *this;
}

Path &Path::A(double rx, double ry, double rot, int large, int sweep, double x, double y)
{
    m_parts.push_back("A" + fmt(rx) + "," + fmt(ry) + " " + fmt(rot) + " " + std::to_string(large) + ","
            + std::to_string(sweep) + " " + fmt(x) + "," + fmt(y));
    return *this;
}

Path &Path::Z()
{
    m_parts.push_back("Z");
    return *this;
}

Path &Path::polyline(const std::vector<std::pair<double, double>> &points)
{
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (i == 0) {
            M(points[i].first, points[i].second);
        } else {
            L(points[i].first, points[i].second);
        }
    }
    return *this;
}

std::string Path::str() const
{
    std::string out;
    for (const std::string &part : m_parts) {
        out += part;
    }
    return out;
}

std::string roundedRectPath(double x, double y, double w, double h, double r)
{
    r = std::min({ r, w / 2, h / 2 });
    Path p;
    p.M(x + r, y).H(x + w - r);
    p.A(r, r, 0, 0, 1, x + w, y + r);
    p.V(y + h - r);
    p.A(r, r, 0, 0, 1, x + w - r, y + h);
    p.H(x + r);
    p.A(r, r, 0, 0, 1, x, y + h - r);
    p.V(y + r);
    p.A(r, r, 0, 0, 1, x + r, y);
    p.Z();
    return p.str();
}

double baselineY(double y, double fontSize, const std::string &dominantBaseline)
{
    static const std::map<std::string, double> shifts = {
        { "", 0.0 }, { "auto", 0.0 }, { "alphabetic", 0.0 }, { "middle", 0.264 }, { "central", 0.3465 },
        { "hanging", 0.724 }, { "text-before-edge", 0.724 },
    };
    auto it = shifts.find(dominantBaseline);
    const double shift = it == shifts.end() ? 0.0 : it->second;
    return y + (shift - MIDDLE_ABOVE_BASELINE) * fontSize;
}

std::shared_ptr<El> textElement(const std::vector<std::string> &lines, double x, double y,
        const std::string &anchor, double fontSize, double lineHeight, const std::string &cls,
        const std::string &style, bool rotate, double rotateDegrees)
{
    auto group = std::make_shared<El>("g");
    if (!cls.empty()) {
        group->set("class", cls);
    }
    const double step = fontSize * lineHeight;
    const double total = step * static_cast<double>(lines.size());
    const double offset = -total / 2 + step / 2 + fontSize * MIDDLE_ABOVE_BASELINE;
    double originX = x;
    double originY = y;
    if (rotate) {
        group->attr("transform",
                "translate(" + fmt(x) + ", " + fmt(y) + ") rotate(" + fmt(rotateDegrees) + ")");
        originX = 0.0;
        originY = 0.0;
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        auto el = std::make_shared<El>("text");
        el->attr("x", fmt(originX));
        el->attr("y", fmt(originY + offset + static_cast<double>(i) * step));
        el->attr("text-anchor", anchor);
        el->attr("font-size", fmt(fontSize) + "px");
        if (!cls.empty()) {
            el->set("class", cls);
        }
        if (!style.empty()) {
            el->attr("style", style);
        }
        el->setText(lines[i]);
        group->add(el);
    }
    return group;
}

SvgDocument::SvgDocument(const std::string &svgId)
    : m_svgId(svgId)
    , m_defs(std::make_shared<El>("defs"))
    , m_root(std::make_shared<El>("g"))
{
}

void SvgDocument::addStyle(const std::string &css)
{
    std::string trimmed = css;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
        trimmed.erase(trimmed.begin());
    }
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
        trimmed.pop_back();
    }
    if (!trimmed.empty()) {
        m_styleBlocks.push_back(trimmed);
    }
}

void SvgDocument::prependStyle(const std::string &css)
{
    std::string trimmed = css;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
        trimmed.erase(trimmed.begin());
    }
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
        trimmed.pop_back();
    }
    if (!trimmed.empty()) {
        m_styleBlocks.insert(m_styleBlocks.begin(), trimmed);
    }
}

void SvgDocument::replaceInStyles(const std::string &from, const std::string &to)
{
    for (std::string &block : m_styleBlocks) {
        std::size_t pos = 0;
        while ((pos = block.find(from, pos)) != std::string::npos) {
            block.replace(pos, from.size(), to);
            pos += to.size();
        }
    }
}

bool SvgDocument::hasMarker(const std::string &markerId) const
{
    return std::find(m_markerIds.begin(), m_markerIds.end(), markerId) != m_markerIds.end();
}

void SvgDocument::addMarker(const std::string &markerId, std::shared_ptr<El> el)
{
    if (!hasMarker(markerId)) {
        m_markerIds.push_back(markerId);
        m_defs->add(el);
    }
}

std::string SvgDocument::toString(double width, double height, const std::vector<double> &viewBox,
        bool useMaxWidth) const
{
    std::vector<double> vb = viewBox.size() == 4 ? viewBox : std::vector<double> { 0.0, 0.0, width, height };
    auto svg = std::make_shared<El>("svg");
    svg->attr("id", m_svgId);
    svg->attr("width", useMaxWidth ? "100%" : fmt(width));
    svg->attr("xmlns", "http://www.w3.org/2000/svg");
    svg->attr("xmlns:xlink", "http://www.w3.org/1999/xlink");
    svg->attr("viewBox", fmt(vb[0]) + " " + fmt(vb[1]) + " " + fmt(vb[2]) + " " + fmt(vb[3]));
    if (useMaxWidth) {
        svg->attr("style", "max-width: " + fmt(vb[2]) + "px;");
    } else {
        svg->attr("height", fmt(height));
    }
    if (!overflow.empty()) {
        svg->attr("overflow", overflow);
    }
    svg->attr("role", "graphics-document document");
    svg->attr("aria-roledescription", ariaRoledescription);
    if (!title.empty()) {
        svg->attr("aria-labelledby", m_svgId + "-title");
    }
    if (!desc.empty()) {
        svg->attr("aria-describedby", m_svgId + "-desc");
    }
    if (!title.empty()) {
        auto t = std::make_shared<El>("title");
        t->attr("id", m_svgId + "-title");
        t->setText(title);
        svg->add(t);
    }
    if (!desc.empty()) {
        auto d = std::make_shared<El>("desc");
        d->attr("id", m_svgId + "-desc");
        d->setText(desc);
        svg->add(d);
    }
    if (!m_styleBlocks.empty()) {
        auto style = std::make_shared<El>("style");
        style->attr("type", "text/css");
        std::string joined;
        for (std::size_t i = 0; i < m_styleBlocks.size(); ++i) {
            if (i > 0) {
                joined += "\n";
            }
            joined += m_styleBlocks[i];
        }
        style->addRaw(joined);
        svg->add(style);
    }
    if (!background.empty() && background != "transparent") {
        auto rect = std::make_shared<El>("rect");
        rect->attr("x", fmt(vb[0]));
        rect->attr("y", fmt(vb[1]));
        rect->attr("width", fmt(vb[2]));
        rect->attr("height", fmt(vb[3]));
        rect->attr("fill", background);
        rect->attr("class", "background");
        svg->add(rect);
    }
    if (m_defs->hasChildren()) {
        svg->add(m_defs);
    }
    svg->add(m_root);
    return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n" + svg->toString() + "\n";
}

} // namespace tqtw_mermaid2svg
