/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2scolors.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <vector>

namespace tqtw_mermaid2svg {

namespace {

const std::map<std::string, std::string> &namedColors()
{
    static const std::map<std::string, std::string> named = {
        { "black", "#000000" }, { "white", "#ffffff" }, { "red", "#ff0000" },
        { "green", "#008000" }, { "blue", "#0000ff" }, { "yellow", "#ffff00" },
        { "gray", "#808080" }, { "grey", "#808080" }, { "lightgrey", "#d3d3d3" },
        { "lightgray", "#d3d3d3" }, { "darkgrey", "#a9a9a9" }, { "darkgray", "#a9a9a9" },
        { "orange", "#ffa500" }, { "purple", "#800080" }, { "cyan", "#00ffff" },
        { "magenta", "#ff00ff" }, { "silver", "#c0c0c0" }, { "transparent", "#00000000" },
        { "lightblue", "#add8e6" }, { "navy", "#000080" }, { "teal", "#008080" },
        { "pink", "#ffc0cb" }, { "brown", "#a52a2a" }, { "gold", "#ffd700" },
    };
    return named;
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

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

int hexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool isHex(const std::string &s)
{
    if (s.size() < 2 || s[0] != '#') {
        return false;
    }
    for (std::size_t i = 1; i < s.size(); ++i) {
        if (hexVal(s[i]) < 0) {
            return false;
        }
    }
    return true;
}

//: Split the inside of rgb()/hsl() on commas, slashes or whitespace.
std::vector<std::string> splitParts(const std::string &inside)
{
    std::vector<std::string> parts;
    std::string current;
    for (char c : inside) {
        if (c == ',' || c == '/' || std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

double stripUnit(const std::string &token, const std::string &unit)
{
    std::string value = token;
    if (!unit.empty() && value.size() >= unit.size()
            && value.compare(value.size() - unit.size(), unit.size(), unit) == 0) {
        value = value.substr(0, value.size() - unit.size());
    }
    return std::atof(value.c_str());
}

void rgbToHls(double r, double g, double b, double &h, double &l, double &s)
{
    const double max = std::max({ r, g, b });
    const double min = std::min({ r, g, b });
    l = (max + min) / 2.0;
    const double d = max - min;
    if (d == 0) {
        h = 0;
        s = 0;
        return;
    }
    s = l < 0.5 ? d / (max + min) : d / (2.0 - max - min);
    if (max == r) {
        h = (g - b) / d + (g < b ? 6.0 : 0.0);
    } else if (max == g) {
        h = (b - r) / d + 2.0;
    } else {
        h = (r - g) / d + 4.0;
    }
    h /= 6.0;
}

double hueToRgb(double p, double q, double t)
{
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0 / 6) return p + (q - p) * 6 * t;
    if (t < 1.0 / 2) return q;
    if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
    return p;
}

void hlsToRgb(double h, double l, double s, double &r, double &g, double &b)
{
    if (s == 0) {
        r = g = b = l;
        return;
    }
    const double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    const double p = 2 * l - q;
    r = hueToRgb(p, q, h + 1.0 / 3);
    g = hueToRgb(p, q, h);
    b = hueToRgb(p, q, h - 1.0 / 3);
}

int clamp255(double v)
{
    return static_cast<int>(std::lround(std::max(0.0, std::min(255.0, v * 255))));
}

} // namespace

Color parseColor(const std::string &colorIn)
{
    if (colorIn.empty()) {
        return { 0, 0, 0, 1 };
    }
    std::string c = lower(trim(colorIn));
    auto it = namedColors().find(c);
    if (it != namedColors().end()) {
        c = it->second;
    }
    if (isHex(c)) {
        const std::string h = c.substr(1);
        if (h.size() == 3) {
            return { hexVal(h[0]) * 17 / 255.0, hexVal(h[1]) * 17 / 255.0, hexVal(h[2]) * 17 / 255.0, 1 };
        }
        if (h.size() == 6) {
            return { std::stoi(h.substr(0, 2), nullptr, 16) / 255.0, std::stoi(h.substr(2, 2), nullptr, 16) / 255.0,
                std::stoi(h.substr(4, 2), nullptr, 16) / 255.0, 1 };
        }
        if (h.size() == 8) {
            return { std::stoi(h.substr(0, 2), nullptr, 16) / 255.0, std::stoi(h.substr(2, 2), nullptr, 16) / 255.0,
                std::stoi(h.substr(4, 2), nullptr, 16) / 255.0, std::stoi(h.substr(6, 2), nullptr, 16) / 255.0 };
        }
    }
    if (c.rfind("rgb", 0) == 0) {
        const std::size_t open = c.find('(');
        const std::size_t close = c.rfind(')');
        if (open != std::string::npos && close != std::string::npos && close > open) {
            const std::vector<std::string> parts = splitParts(c.substr(open + 1, close - open - 1));
            std::array<double, 4> vals { 0, 0, 0, 1 };
            for (std::size_t i = 0; i < parts.size() && i < 4; ++i) {
                const std::string &p = parts[i];
                if (p.size() > 1 && p.back() == '%') {
                    vals[i] = (i < 3 ? std::atof(p.c_str()) / 100.0 : std::atof(p.c_str()) / 100.0);
                } else {
                    vals[i] = (i < 3 ? std::atof(p.c_str()) / 255.0 : std::atof(p.c_str()));
                }
            }
            return { vals[0], vals[1], vals[2], vals[3] };
        }
    }
    if (c.rfind("hsl", 0) == 0) {
        const std::size_t open = c.find('(');
        const std::size_t close = c.rfind(')');
        if (open != std::string::npos && close != std::string::npos && close > open) {
            const std::vector<std::string> parts = splitParts(c.substr(open + 1, close - open - 1));
            if (parts.size() >= 3) {
                const double h = stripUnit(parts[0], "deg") / 360.0;
                const double s = stripUnit(parts[1], "%") / 100.0;
                const double l = stripUnit(parts[2], "%") / 100.0;
                double a = 1;
                if (parts.size() > 3) {
                    a = parts[3].back() == '%' ? stripUnit(parts[3], "%") / 100.0 : std::atof(parts[3].c_str());
                }
                double r, g, b;
                hlsToRgb(std::fmod(h + 1.0, 1.0), l, s, r, g, b);
                return { r, g, b, a };
            }
        }
    }
    return { 0, 0, 0, 1 };
}

std::string toHex(const Color &color)
{
    const int r = clamp255(color.r);
    const int g = clamp255(color.g);
    const int b = clamp255(color.b);
    char buf[64];
    if (color.a >= 0.999) {
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
        return buf;
    }
    std::snprintf(buf, sizeof(buf), "rgba(%d, %d, %d, %.3g)", r, g, b, color.a);
    return buf;
}

std::string lighten(const std::string &color, double amount)
{
    const Color c = parseColor(color);
    double h, l, s;
    rgbToHls(c.r, c.g, c.b, h, l, s);
    l = std::max(0.0, std::min(1.0, l + amount / 100.0));
    double r, g, b;
    hlsToRgb(h, l, s, r, g, b);
    return toHex({ r, g, b, c.a });
}

std::string darken(const std::string &color, double amount)
{
    return lighten(color, -amount);
}

std::string adjust(const std::string &color, double hue, double sat, double light)
{
    const Color c = parseColor(color);
    double h, l, s;
    rgbToHls(c.r, c.g, c.b, h, l, s);
    h = std::fmod(h + hue / 360.0 + 1.0, 1.0);
    s = std::max(0.0, std::min(1.0, s + sat / 100.0));
    l = std::max(0.0, std::min(1.0, l + light / 100.0));
    double r, g, b;
    hlsToRgb(h, l, s, r, g, b);
    return toHex({ r, g, b, c.a });
}

std::string mix(const std::string &a, const std::string &b, double weight)
{
    const Color ca = parseColor(a);
    const Color cb = parseColor(b);
    const double w = std::max(0.0, std::min(1.0, weight));
    return toHex({ ca.r * (1 - w) + cb.r * w, ca.g * (1 - w) + cb.g * w, ca.b * (1 - w) + cb.b * w,
        ca.a * (1 - w) + cb.a * w });
}

std::string categorical(const std::string &seed, int index, bool dark, double step)
{
    const Color c = parseColor(seed);
    double hue, light, sat;
    rgbToHls(c.r, c.g, c.b, hue, light, sat);
    const double rotated = std::fmod(hue * 360.0 + index * step, 360.0) / 360.0;
    const double saturation = dark ? 0.42 : 0.58;
    const double lightness = (dark ? 0.34 : 0.78) + (index % 2 ? 0.06 : 0.0);
    double r, g, b;
    hlsToRgb(rotated, lightness, saturation, r, g, b);
    return toHex({ r, g, b, c.a });
}

bool isDark(const std::string &color)
{
    const Color c = parseColor(color);
    return (0.299 * c.r + 0.587 * c.g + 0.114 * c.b) < 0.5;
}

double luminance(const std::string &color)
{
    const Color c = parseColor(color);
    auto channel = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
}

double contrastRatio(const std::string &a, const std::string &b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    const double lo = std::min(la, lb);
    const double hi = std::max(la, lb);
    return (hi + 0.05) / (lo + 0.05);
}

double hueDistance(const std::string &a, const std::string &b)
{
    const Color ca = parseColor(a);
    const Color cb = parseColor(b);
    double h1, l1, s1, h2, l2, s2;
    rgbToHls(ca.r, ca.g, ca.b, h1, l1, s1);
    rgbToHls(cb.r, cb.g, cb.b, h2, l2, s2);
    const double d = std::fmod(std::fabs(h1 * 360.0 - h2 * 360.0), 360.0);
    return std::min(d, 360.0 - d);
}

bool sameColour(const std::string &a, const std::string &b)
{
    return contrastRatio(a, b) < 1.06 && hueDistance(a, b) < 25.0;
}

std::string contrastText(const std::string &background, const std::string &dark, const std::string &light)
{
    std::string best = contrastRatio(dark, background) >= contrastRatio(light, background) ? dark : light;
    if (contrastRatio(best, background) < AA_CONTRAST) {
        best = contrastRatio("#000000", background) >= contrastRatio("#ffffff", background) ? "#000000" : "#ffffff";
    }
    return best;
}

std::string legible(const std::string &text, const std::string &background, const std::string &dark,
        const std::string &light)
{
    if (contrastRatio(text, background) >= AA_CONTRAST) {
        return text;
    }
    std::string shifted = text;
    for (int i = 0; i < 20; ++i) {
        shifted = isDark(background) ? lighten(shifted, 6) : darken(shifted, 6);
        if (contrastRatio(shifted, background) >= AA_CONTRAST) {
            return shifted;
        }
    }
    return contrastText(background, dark, light);
}

std::string separate(const std::string &color, const std::string &background, double minimum)
{
    if (contrastRatio(color, background) >= minimum) {
        return color;
    }
    const std::string target = isDark(background) ? "#ffffff" : "#000000";
    std::string adjusted = color;
    for (int step = 1; step <= 40; ++step) {
        adjusted = mix(color, target, step * 0.02);
        if (contrastRatio(adjusted, background) >= minimum) {
            break;
        }
    }
    return adjusted;
}

} // namespace tqtw_mermaid2svg
