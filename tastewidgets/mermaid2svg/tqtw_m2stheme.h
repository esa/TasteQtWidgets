/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Theme variables: a handful of seed colours resolved into a flat map. A C++
// port of mermaid2svg's theme.py and palettes.py.

#include <map>
#include <string>
#include <vector>

namespace tqtw_mermaid2svg {

extern const char *const DEFAULT_FONT_FAMILY;

//: A resolved set of theme variables.
class Theme
{
public:
    std::string name = "default";
    std::map<std::string, std::string> vars;

    //: One theme variable, sanitised so it is safe in CSS or an attribute.
    std::string get(const std::string &key, const std::string &fallback = std::string()) const;
    std::string fontFamily() const;
    double fontSize() const;
    bool dark() const;
    //: A categorical palette entry, e.g. palette("pie", 3).
    std::string palette(const std::string &prefix, int index, int count = 12) const;
};

//: Resolve a theme by name, then apply user themeVariables overrides. Accepts
//: the built-in themes, "base", and any palette name.
Theme buildTheme(const std::string &name = "default", const std::map<std::string, std::string> &overrides = {});

//: Every accepted theme name, built-ins first.
std::vector<std::string> allThemeNames();

//: value cut short at the first character that could escape its CSS.
std::string cssValue(const std::string &value);

} // namespace tqtw_mermaid2svg
