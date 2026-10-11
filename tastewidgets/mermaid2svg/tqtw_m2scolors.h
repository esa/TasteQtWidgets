/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Color maths for the mermaid renderer: a C++ port of mermaid2svg's
// colors.py. Parsing CSS colors, mixing, contrast and the categorical palettes
// a theme derives from a seed. Namespace `mermaid2svg`.

#include <array>
#include <string>

namespace tqtw_mermaid2svg {

//: An (r, g, b, a) color with each channel in 0..1.
struct Color
{
    double r = 0;
    double g = 0;
    double b = 0;
    double a = 1;
};

//: Parse a CSS color (hex, rgb()/rgba(), hsl()/hsla(), or a name).
Color parseColor(const std::string &color);
//: The CSS text of a color: #rrggbb, or rgba(...) when translucent.
std::string toHex(const Color &color);

std::string lighten(const std::string &color, double amount);
std::string darken(const std::string &color, double amount);
std::string adjust(const std::string &color, double hue = 0, double sat = 0, double light = 0);
std::string mix(const std::string &a, const std::string &b, double weight = 0.5);
std::string categorical(const std::string &seed, int index, bool dark = false, double step = 47.0);

bool isDark(const std::string &color);
double luminance(const std::string &color);
double contrastRatio(const std::string &a, const std::string &b);
double hueDistance(const std::string &a, const std::string &b);
bool sameColour(const std::string &a, const std::string &b);
std::string contrastText(const std::string &background, const std::string &dark = "#333333",
        const std::string &light = "#ffffff");
std::string legible(const std::string &text, const std::string &background, const std::string &dark = "#333333",
        const std::string &light = "#ffffff");
std::string separate(const std::string &color, const std::string &background, double minimum = 1.25);

//: WCAG 2 AA minimum contrast ratio for text at normal size.
constexpr double AA_CONTRAST = 4.5;

} // namespace tqtw_mermaid2svg
