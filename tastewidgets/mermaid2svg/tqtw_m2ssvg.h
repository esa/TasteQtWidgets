/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// A small SVG document builder: a C++ port of mermaid2svg's svggen.py.
// Stable attribute order, self-closing tags and raw stylesheets are handled by
// building the string directly rather than through an XML tree.

#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace tqtw_mermaid2svg {

//: Escape a string for XML text or an attribute value.
std::string escapeXml(const std::string &text);
//: Format a coordinate compactly and deterministically.
std::string fmt(double value);

class El;
class RawText;

//: An SVG element. Children are owned by it.
class El
{
public:
    explicit El(std::string tag);

    //: Set attributes; a key's "__" becomes ":" and "_" becomes "-".
    El &set(const std::string &key, const std::string &value);
    El &set(const std::string &key, double value);
    El &set(const std::string &key, int value);
    El &attr(const std::string &name, const std::string &value);
    El &attr(const std::string &name, double value);

    //: Append a child element and return it.
    El &child(const std::string &tag);
    El &add(std::shared_ptr<El> child);
    El &addText(const std::string &text);
    El &addRaw(const std::string &content);

    void setText(const std::string &text);
    bool hasText() const { return m_hasText; }
    const std::string &text() const { return m_text; }
    const std::string &tag() const { return m_tag; }
    const std::map<std::string, std::string> &attrs() const { return m_attrs; }
    bool hasChildren() const { return !m_children.empty(); }

    void render(std::vector<std::string> &out, int indent = 0) const;
    std::string toString(int indent = 0) const;

private:
    struct Child
    {
        enum Kind { Element, Raw } kind = Element;
        std::shared_ptr<El> el;
        std::string raw;
    };

    std::string m_tag;
    std::map<std::string, std::string> m_attrs;
    std::vector<Child> m_children;
    std::string m_text;
    bool m_hasText = false;
};

//: An SVG path `d` builder.
class Path
{
public:
    Path &M(double x, double y);
    Path &L(double x, double y);
    Path &H(double x);
    Path &V(double y);
    Path &C(double x1, double y1, double x2, double y2, double x, double y);
    Path &Q(double x1, double y1, double x, double y);
    Path &A(double rx, double ry, double rot, int large, int sweep, double x, double y);
    Path &Z();
    Path &polyline(const std::vector<std::pair<double, double>> &points);
    std::string str() const;

private:
    std::vector<std::string> m_parts;
};

//: A rounded rectangle as a path (used where `rx` would not do).
std::string roundedRectPath(double x, double y, double w, double h, double r);

//: A raw stylesheet block, emitted verbatim.
class RawText
{
public:
    explicit RawText(std::string content)
        : m_content(std::move(content))
    {
    }
    const std::string &content() const { return m_content; }

private:
    std::string m_content;
};

//: `text_element` centres a label's em box on the `y` it is given; this is how
//: far that point sits above the alphabetic baseline.
constexpr double MIDDLE_ABOVE_BASELINE = 0.34;

//: A `y` + `dominant-baseline` pair, as a `textElement` `y`.
double baselineY(double y, double fontSize, const std::string &dominantBaseline = "auto");

//: A multi-line label centred vertically on `y`, as a `<g>` of `<text>`, one
//: per line (never positioned tspans).
std::shared_ptr<El> textElement(const std::vector<std::string> &lines, double x, double y,
        const std::string &anchor = "middle", double fontSize = 16.0, double lineHeight = 1.25,
        const std::string &cls = std::string(), const std::string &style = std::string(),
        bool rotate = false, double rotateDegrees = 0.0);

//: Top-level `<svg>` with defs, style and a content group.
class SvgDocument
{
public:
    explicit SvgDocument(const std::string &svgId = "my-svg");

    void addStyle(const std::string &css);
    //: Insert a stylesheet block at the front (order matters for overrides).
    void prependStyle(const std::string &css);
    //: Replace every occurrence of `from` in every stylesheet block.
    void replaceInStyles(const std::string &from, const std::string &to);
    const std::string &svgId() const { return m_svgId; }
    bool hasMarker(const std::string &markerId) const;
    void addMarker(const std::string &markerId, std::shared_ptr<El> el);

    //: The content group (`<g>`): diagrams draw into it.
    std::shared_ptr<El> root() const { return m_root; }
    //: The `<defs>` element.
    std::shared_ptr<El> defs() const { return m_defs; }

    std::string title;
    std::string desc;
    std::string background;
    std::string overflow;
    std::string ariaRoledescription = "diagram";

    //: The whole document as text. `viewBox` is (x, y, w, h); when empty the
    //: box is (0, 0, width, height).
    std::string toString(double width, double height, const std::vector<double> &viewBox = {},
            bool useMaxWidth = true) const;

private:
    std::string m_svgId;
    std::shared_ptr<El> m_defs;
    std::shared_ptr<El> m_root;
    std::vector<std::string> m_styleBlocks;
    std::vector<std::string> m_markerIds;
};

} // namespace tqtw_mermaid2svg
