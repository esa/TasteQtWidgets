/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2stext.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace tqtw_mermaid2svg {

namespace {

//: The embedded Helvetica advance-width table, in 1/1000 em, for U+0020..U+007E.
const char *const kHelvetica = "278 278 355 556 556 889 667 222 333 333 389 584 278 333 278 278 "
                               "556 556 556 556 556 556 556 556 556 556 278 278 584 584 584 556 "
                               "1015 667 667 722 722 667 611 778 722 278 500 667 556 833 722 778 "
                               "667 778 722 667 611 722 667 944 667 667 611 278 278 278 469 556 "
                               "222 556 556 500 556 556 278 556 556 222 222 500 222 833 556 556 "
                               "556 556 333 500 278 556 500 722 500 500 500 334 260 334 584";

const char *const kHelveticaBold = "278 333 474 556 556 889 722 278 333 333 389 584 278 333 278 278 "
                                   "556 556 556 556 556 556 556 556 556 556 333 333 584 584 584 611 "
                                   "975 722 722 722 722 667 611 778 722 278 556 722 611 833 722 778 "
                                   "667 778 722 667 611 722 667 944 667 667 611 333 278 333 584 556 "
                                   "278 556 611 556 611 556 333 611 611 278 278 556 278 889 611 611 "
                                   "611 611 389 556 333 611 556 778 556 556 500 389 280 389 584";

std::map<unsigned int, int> table(const char *spec)
{
    std::map<unsigned int, int> out;
    std::istringstream ss(spec);
    int width;
    unsigned int cp = 0x20;
    while (ss >> width) {
        out[cp++] = width;
    }
    return out;
}

const std::map<unsigned int, int> &regularWidths()
{
    static const std::map<unsigned int, int> t = table(kHelvetica);
    return t;
}

const std::map<unsigned int, int> &boldWidths()
{
    static const std::map<unsigned int, int> t = table(kHelveticaBold);
    return t;
}

//: Latin-1 and common punctuation worth pinning rather than defaulting.
const std::map<unsigned int, int> &extraWidths()
{
    static const std::map<unsigned int, int> t = {
        { 0xA0, 278 }, { 0xA9, 737 }, { 0xAE, 737 }, { 0xB0, 400 }, { 0xB1, 584 }, { 0xB7, 278 },
        { 0xD7, 584 }, { 0xF7, 584 }, { 0x2013, 556 }, { 0x2014, 1000 }, { 0x2018, 222 },
        { 0x2019, 222 }, { 0x201C, 333 }, { 0x201D, 333 }, { 0x2022, 350 }, { 0x2026, 1000 },
        { 0x2190, 1000 }, { 0x2192, 1000 }, { 0x2260, 549 }, { 0x2264, 549 }, { 0x2265, 549 },
    };
    return t;
}

constexpr int kDefaultWidth = 556;

bool isWide(unsigned int cp)
{
    return (cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0x303E) || (cp >= 0x3041 && cp <= 0x33FF)
            || (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xA000 && cp <= 0xA4CF)
            || (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE30 && cp <= 0xFE6F)
            || (cp >= 0xFF00 && cp <= 0xFF60) || (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F300 && cp <= 0x1FAFF)
            || (cp >= 0x20000 && cp <= 0x3FFFD);
}

bool isZeroWidth(unsigned int cp)
{
    return (cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x200B && cp <= 0x200F) || (cp >= 0xFE00 && cp <= 0xFE0F)
            || (cp >= 0x20D0 && cp <= 0x20FF) || cp == 0xFEFF;
}

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

} // namespace

std::vector<unsigned int> utf8Codepoints(const std::string &text)
{
    std::vector<unsigned int> out;
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        unsigned int cp = c;
        int extra = 0;
        if (c >= 0xF0) {
            cp = c & 0x07;
            extra = 3;
        } else if (c >= 0xE0) {
            cp = c & 0x0F;
            extra = 2;
        } else if (c >= 0xC0) {
            cp = c & 0x1F;
            extra = 1;
        }
        ++i;
        for (int k = 0; k < extra && i < text.size(); ++k, ++i) {
            cp = (cp << 6) | (static_cast<unsigned char>(text[i]) & 0x3F);
        }
        out.push_back(cp);
    }
    return out;
}

double FontMetrics::kern(unsigned int, unsigned int, bool) const
{
    return 0.0;
}

double FontMetrics::width(const std::string &text, double size, bool bold) const
{
    double total = 0.0;
    const std::vector<unsigned int> cps = utf8Codepoints(text);
    if (!hasKerning) {
        for (unsigned int cp : cps) {
            total += advance(cp, bold);
        }
        return total * size;
    }
    int previous = -1;
    for (unsigned int cp : cps) {
        total += advance(cp, bold);
        if (previous >= 0) {
            total += kern(static_cast<unsigned int>(previous), cp, bold);
        }
        previous = static_cast<int>(cp);
    }
    return total * size;
}

double BuiltinMetrics::advance(unsigned int codepoint, bool bold) const
{
    if (isZeroWidth(codepoint)) {
        return 0.0;
    }
    const std::map<unsigned int, int> &t = bold ? boldWidths() : regularWidths();
    auto it = t.find(codepoint);
    if (it != t.end()) {
        return it->second / 1000.0;
    }
    auto extra = extraWidths().find(codepoint);
    if (extra != extraWidths().end()) {
        return extra->second / 1000.0;
    }
    if (isWide(codepoint)) {
        return 1000 / 1000.0;
    }
    if (codepoint == 0x09) {
        return kDefaultWidth * 4 / 1000.0;
    }
    return kDefaultWidth / 1000.0;
}

double svgTextBBoxHeight(const FontMetrics &metrics, double size, int lines, double lineHeight,
        double baselineShift)
{
    constexpr double subpixel = 64.0;
    double first = std::floor(metrics.ascent * size + 0.5) + std::floor(metrics.descent * size + 0.5);
    if (baselineShift != 0.0) {
        const double ticks = baselineShift * size * subpixel;
        if (std::fabs(ticks - std::floor(ticks) - 0.5) < 1e-9) {
            first += 1.0 / subpixel;
        }
    }
    return first + std::max(0, lines - 1) * size * lineHeight;
}

double TextBlock::width() const
{
    if (boxWidth >= 0.0) {
        return boxWidth;
    }
    double m = 0.0;
    for (double w : widths) {
        m = std::max(m, w);
    }
    return m;
}

double TextBlock::height() const
{
    if (lines.empty()) {
        return 0.0;
    }
    return static_cast<double>(lines.size()) * fontSize * lineHeight;
}

TextMeasurer::TextMeasurer(std::shared_ptr<const FontMetrics> metrics, double fontSize, double lineHeight,
        const std::string &family)
    : m_metrics(std::move(metrics))
    , m_fontSize(fontSize)
    , m_lineHeight(lineHeight)
    , m_family(family)
{
}

double TextMeasurer::width(const std::string &text, double size, bool bold) const
{
    return m_metrics->width(text, size > 0 ? size : m_fontSize, bold);
}

namespace {

std::string replaceAll(std::string s, const std::string &from, const std::string &to)
{
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string stripTags(const std::string &s)
{
    std::string out;
    bool inTag = false;
    for (char c : s) {
        if (c == '<') {
            inTag = true;
        } else if (c == '>') {
            inTag = false;
        } else if (!inTag) {
            out.push_back(c);
        }
    }
    return out;
}

//: Case-insensitive replacement of <br>, <br/>, <br /> with a newline.
std::string breakTags(const std::string &s)
{
    std::string out;
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '<' && i + 3 <= s.size() && std::tolower(s[i + 1]) == 'b' && std::tolower(s[i + 2]) == 'r') {
            std::size_t j = i + 3;
            // optional whitespace then optional '/' then '>'
            while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) {
                ++j;
            }
            if (j < s.size() && s[j] == '/') {
                ++j;
            }
            while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) {
                ++j;
            }
            if (j < s.size() && s[j] == '>') {
                out.push_back('\n');
                i = j + 1;
                continue;
            }
        }
        out.push_back(s[i]);
        ++i;
    }
    return out;
}

} // namespace

std::vector<std::string> TextMeasurer::splitLines(const std::string &label) const
{
    std::string text = breakTags(label);
    text = replaceAll(text, "\\n", "\n");
    text = stripTags(text);
    std::vector<std::string> out;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            out.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    out.push_back(trim(current));
    return out;
}

int TextMeasurer::longestPrefix(const std::string &text, double maxWidth, double size, bool bold) const
{
    int low = 1;
    int high = static_cast<int>(text.size());
    while (low < high) {
        const int mid = (low + high + 1) / 2;
        if (m_metrics->width(text.substr(0, mid), size, bold) > maxWidth) {
            high = mid - 1;
        } else {
            low = mid;
        }
    }
    return low;
}

std::vector<std::string> TextMeasurer::wrap(const std::string &text, double maxWidth, double size, bool bold) const
{
    const double useSize = size > 0 ? size : m_fontSize;
    std::vector<std::string> out;
    for (const std::string &raw : splitLines(text)) {
        if (raw.empty()) {
            out.push_back("");
            continue;
        }
        if (m_metrics->width(raw, useSize, bold) <= maxWidth) {
            out.push_back(raw);
            continue;
        }
        // Split on single spaces to mirror Python's str.split(" ").
        std::vector<std::string> words;
        {
            std::string current;
            for (char c : raw) {
                if (c == ' ') {
                    words.push_back(current);
                    current.clear();
                } else {
                    current.push_back(c);
                }
            }
            words.push_back(current);
        }
        const double space = m_metrics->width(" ", useSize, bold);
        std::string current;
        for (std::size_t index = 0; index < words.size(); ++index) {
            const std::string &word = words[index];
            std::string probe = current + " " + word;
            const std::size_t firstNonSpace = probe.find_first_not_of(' ');
            probe = firstNonSpace == std::string::npos ? "" : probe.substr(firstNonSpace);
            const double trailing = index < words.size() - 1 ? space : 0.0;
            if (!current.empty() && m_metrics->width(probe, useSize, bold) + trailing > maxWidth) {
                out.push_back(current);
                current = word;
            } else {
                current = probe;
            }
            while (m_metrics->width(current, useSize, bold) > maxWidth && current.size() > 1) {
                const int cut = longestPrefix(current, maxWidth, useSize, bold);
                out.push_back(current.substr(0, cut));
                current = current.substr(cut);
            }
        }
        if (!current.empty()) {
            out.push_back(current);
        }
    }
    return out;
}

TextBlock TextMeasurer::block(const std::string &label, double size, bool bold, double maxWidth,
        double lineHeight) const
{
    const double useSize = size > 0 ? size : m_fontSize;
    const std::vector<std::string> unwrapped = splitLines(label);
    std::vector<std::string> lines = maxWidth > 0 ? wrap(label, maxWidth, useSize, bold) : unwrapped;
    TextBlock block;
    block.lines = lines;
    block.fontSize = useSize;
    block.lineHeight = lineHeight < 0 ? m_lineHeight : lineHeight;
    double natural = 0.0;
    for (const std::string &ln : unwrapped) {
        natural = std::max(natural, m_metrics->width(ln, useSize, bold));
    }
    for (const std::string &ln : lines) {
        block.widths.push_back(m_metrics->width(ln, useSize, bold));
    }
    if (maxWidth > 0 && natural > maxWidth) {
        block.boxWidth = maxWidth;
    }
    return block;
}

} // namespace tqtw_mermaid2svg
