/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Text measurement for the mermaid renderer: a C++ port of mermaid2svg's
// text.py. Everything in a layout is downstream of "how wide is this string?".
// The default metrics are an embedded Helvetica/Arial advance-width table, so
// output is deterministic and needs no font files.

#include <memory>
#include <string>
#include <vector>

namespace tqtw_mermaid2svg {

//: Advance widths for a single font, in em units (1.0 == font size).
class FontMetrics
{
public:
    virtual ~FontMetrics() = default;
    virtual double advance(unsigned int codepoint, bool bold = false) const = 0;
    //: Kerning between two adjacent codepoints, in em units (usually negative).
    virtual double kern(unsigned int left, unsigned int right, bool bold = false) const;

    //: Width of `text` rendered at `size` pixels.
    double width(const std::string &text, double size, bool bold = false) const;
    double lineHeight(double size) const { return (ascent + descent) * size; }

    //: Ascent above the baseline, in em units.
    double ascent = 0.905;
    //: Descent below the baseline, in em units (positive).
    double descent = 0.212;
    //: Height of a lowercase x, in em units.
    double xHeight = 0.523;
    //: Whether `kern` can ever return non-zero.
    bool hasKerning = false;
};

//: Deterministic Helvetica-compatible metrics with no external files.
class BuiltinMetrics : public FontMetrics
{
public:
    double advance(unsigned int codepoint, bool bold = false) const override;
};

//: The codepoints of a UTF-8 string.
std::vector<unsigned int> utf8Codepoints(const std::string &text);

//: The reported getBBox().height of a multi-line <text>.
double svgTextBBoxHeight(const FontMetrics &metrics, double size, int lines = 1, double lineHeight = 1.1,
        double baselineShift = 0.0);

//: Several diagram types size their labels as HTML blocks with line-height 1.5.
constexpr double HTML_LABEL_LINE_HEIGHT = 1.5;

//: A measured, wrapped run of text, one entry per line drawn.
struct TextBlock
{
    std::vector<std::string> lines;
    std::vector<double> widths;
    double fontSize = 16.0;
    double lineHeight = 1.25;
    //: Set when wrapping kicked in: the box is the wrap width, not the longest
    //: line. A negative value means "not pinned".
    double boxWidth = -1.0;

    double width() const;
    double height() const;
};

//: Measures and wraps label text for a given font size.
class TextMeasurer
{
public:
    explicit TextMeasurer(std::shared_ptr<const FontMetrics> metrics, double fontSize = 16.0,
            double lineHeight = 1.25, const std::string &family = std::string());

    double width(const std::string &text, double size = 0.0, bool bold = false) const;
    //: Split on <br> / literal \n and strip any other markup.
    std::vector<std::string> splitLines(const std::string &label) const;
    //: Greedy word-wrap to `maxWidth` pixels.
    std::vector<std::string> wrap(const std::string &text, double maxWidth, double size = 0.0,
            bool bold = false) const;
    //: Measure `label`, wrapping at `maxWidth` when it is too wide.
    TextBlock block(const std::string &label, double size = 0.0, bool bold = false,
            double maxWidth = 0.0, double lineHeight = -1.0) const;

    const std::shared_ptr<const FontMetrics> &metrics() const { return m_metrics; }
    double fontSize() const { return m_fontSize; }
    double lineHeight() const { return m_lineHeight; }
    const std::string &family() const { return m_family; }

private:
    int longestPrefix(const std::string &text, double maxWidth, double size, bool bold) const;

    std::shared_ptr<const FontMetrics> m_metrics;
    double m_fontSize;
    double m_lineHeight;
    std::string m_family;
};

} // namespace tqtw_mermaid2svg
