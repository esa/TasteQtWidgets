/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2stheme.h"

#include "tqtw_m2scolors.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace tqtw_mermaid2svg {

const char *const DEFAULT_FONT_FAMILY = "\"trebuchet ms\", verdana, arial, sans-serif";

namespace {

const std::vector<std::string> kAccentRoles = { "accent", "user", "tool", "note", "orange" };

//: role -> colour, per named palette (mermaid2svg's palettes.py).
const std::map<std::string, std::map<std::string, std::string>> &palettes()
{
    static const std::map<std::string, std::map<std::string, std::string>> p = {
        { "nord", { { "bg", "#2e3440" }, { "panel", "#3b4252" }, { "text", "#e5e9f0" }, { "subtle", "#7b88a1" },
                      { "accent", "#88c0d0" }, { "user", "#81a1c1" }, { "tool", "#8fbcbb" }, { "error", "#bf616a" },
                      { "note", "#a3be8c" }, { "orange", "#d08770" } } },
        { "esa-light", { { "bg", "#FFFFFF" }, { "panel", "#E8E8E4" }, { "text", "#003247" }, { "subtle", "#335E6F" },
                           { "accent", "#6DCFF6" }, { "user", "#6DCFF6" }, { "tool", "#A366A9" }, { "error", "#F1666A" },
                           { "note", "#76C8AE" }, { "orange", "#FFCC4E" } } },
        { "esa-dark", { { "bg", "#000000" }, { "panel", "#003247" }, { "text", "#E8E8E4" }, { "subtle", "#8197A6" },
                          { "accent", "#009BDB" }, { "user", "#6DCFF6" }, { "tool", "#76C8AE" }, { "error", "#F1666A" },
                          { "note", "#00AE9D" }, { "orange", "#FBAB18" } } },
        { "dracula", { { "bg", "#282a36" }, { "panel", "#44475a" }, { "text", "#f8f8f2" }, { "subtle", "#9aa0c0" },
                         { "accent", "#bd93f9" }, { "user", "#8be9fd" }, { "tool", "#50fa7b" }, { "error", "#ff5555" },
                         { "note", "#50fa7b" }, { "orange", "#ffb86c" } } },
        { "gruvbox", { { "bg", "#282828" }, { "panel", "#3c3836" }, { "text", "#ebdbb2" }, { "subtle", "#a89984" },
                         { "accent", "#fabd2f" }, { "user", "#83a598" }, { "tool", "#8ec07c" }, { "error", "#fb4934" },
                         { "note", "#b8bb26" }, { "orange", "#fe8019" } } },
        { "tokyo-night", { { "bg", "#1a1b26" }, { "panel", "#24283b" }, { "text", "#c0caf5" }, { "subtle", "#787c99" },
                             { "accent", "#7aa2f7" }, { "user", "#7dcfff" }, { "tool", "#9ece6a" },
                             { "error", "#f7768e" }, { "note", "#9ece6a" }, { "orange", "#ff9e64" } } },
        { "catppuccin", { { "bg", "#1e1e2e" }, { "panel", "#313244" }, { "text", "#cdd6f4" }, { "subtle", "#9399b2" },
                            { "accent", "#89b4fa" }, { "user", "#74c7ec" }, { "tool", "#a6e3a1" },
                            { "error", "#f38ba8" }, { "note", "#a6e3a1" }, { "orange", "#fab387" } } },
        { "light", { { "bg", "#fafafa" }, { "panel", "#eceff4" }, { "text", "#2e3440" }, { "subtle", "#6b7280" },
                       { "accent", "#5e81ac" }, { "user", "#5e81ac" }, { "tool", "#2e7d32" }, { "error", "#b00020" },
                       { "note", "#2e7d32" }, { "orange", "#d2691e" } } },
        { "textual-dark", { { "bg", "#121212" }, { "panel", "#1e1e1e" }, { "text", "#e0e0e0" },
                              { "subtle", "#8d8d8d" }, { "accent", "#0178d4" }, { "user", "#004578" },
                              { "tool", "#fea62b" }, { "error", "#b93c5b" }, { "note", "#4ebf71" },
                              { "orange", "#fea62b" } } },
        { "textual-light", { { "bg", "#e0e0e0" }, { "panel", "#d8d8d8" }, { "text", "#1f1f1f" },
                               { "subtle", "#6c6c6c" }, { "accent", "#004578" }, { "user", "#0178d4" },
                               { "tool", "#fea62b" }, { "error", "#b93c5b" }, { "note", "#4ebf71" },
                               { "orange", "#fea62b" } } },
        { "catppuccin-mocha", { { "bg", "#181825" }, { "panel", "#313244" }, { "text", "#cdd6f4" },
                                  { "subtle", "#848aa1" }, { "accent", "#f5c2e7" }, { "user", "#cba6f7" },
                                  { "tool", "#f9b387" }, { "error", "#f28fad" }, { "note", "#abe9b3" },
                                  { "orange", "#fae3b0" } } },
        { "monokai", { { "bg", "#272822" }, { "panel", "#2e2e2e" }, { "text", "#d6d6d6" }, { "subtle", "#90908e" },
                         { "accent", "#ae81ff" }, { "user", "#f82672" }, { "tool", "#66d9ef" }, { "error", "#f82672" },
                         { "note", "#a5e22e" }, { "orange", "#fc971f" } } },
        { "flexoki", { { "bg", "#100f0f" }, { "panel", "#1c1b1a" }, { "text", "#fffcf0" }, { "subtle", "#9f9d96" },
                         { "accent", "#205ea6" }, { "user", "#24837b" }, { "tool", "#9b76c8" }, { "error", "#ae3029" },
                         { "note", "#65800b" }, { "orange", "#ac8301" } } },
        { "catppuccin-latte", { { "bg", "#eff1f5" }, { "panel", "#e6e9ef" }, { "text", "#4c4f69" },
                                  { "subtle", "#8d8fa1" }, { "accent", "#8839ef" }, { "user", "#db8a78" },
                                  { "tool", "#fd640b" }, { "error", "#d10f39" }, { "note", "#40a02b" },
                                  { "orange", "#de8e1d" } } },
        { "catppuccin-frappe", { { "bg", "#303446" }, { "panel", "#414559" }, { "text", "#c6d0f5" },
                                   { "subtle", "#8a91af" }, { "accent", "#ca9ee6" }, { "user", "#ee9f76" },
                                   { "tool", "#f4b8e4" }, { "error", "#e68284" }, { "note", "#a6d189" },
                                   { "orange", "#e4c890" } } },
        { "catppuccin-macchiato", { { "bg", "#24273a" }, { "panel", "#363a4f" }, { "text", "#cad3f5" },
                                      { "subtle", "#878eaa" }, { "accent", "#c6a0f6" }, { "user", "#f4a97f" },
                                      { "tool", "#f5bde6" }, { "error", "#ed8796" }, { "note", "#a6da95" },
                                      { "orange", "#eed49f" } } },
        { "solarized-light", { { "bg", "#fdf6e3" }, { "panel", "#eee8d5" }, { "text", "#586e75" },
                                 { "subtle", "#9aa4a1" }, { "accent", "#268bd2" }, { "user", "#2aa198" },
                                 { "tool", "#6c71c4" }, { "error", "#db322f" }, { "note", "#849900" },
                                 { "orange", "#ca4b16" } } },
        { "solarized-dark", { { "bg", "#002b36" }, { "panel", "#073642" }, { "text", "#839496" },
                                { "subtle", "#4e6a6f" }, { "accent", "#268bd2" }, { "user", "#2aa198" },
                                { "tool", "#6c71c4" }, { "error", "#db322f" }, { "note", "#849900" },
                                { "orange", "#ca4b16" } } },
        { "rose-pine", { { "bg", "#191724" }, { "panel", "#1f1d2e" }, { "text", "#e0def4" }, { "subtle", "#908ea0" },
                           { "accent", "#c4a7e7" }, { "user", "#31748f" }, { "tool", "#ebbcba" },
                           { "error", "#ea6f92" }, { "note", "#9ccfd8" }, { "orange", "#f5c177" } } },
        { "rose-pine-moon", { { "bg", "#232136" }, { "panel", "#2a273f" }, { "text", "#e0def4" },
                                { "subtle", "#9492a8" }, { "accent", "#c4a7e7" }, { "user", "#3e8fb0" },
                                { "tool", "#ea9a97" }, { "error", "#ea6f92" }, { "note", "#9ccfd8" },
                                { "orange", "#f5c177" } } },
        { "rose-pine-dawn", { { "bg", "#faf4ed" }, { "panel", "#fffaf3" }, { "text", "#575279" },
                                { "subtle", "#9892a7" }, { "accent", "#907aa9" }, { "user", "#286983" },
                                { "tool", "#d6827e" }, { "error", "#b4637a" }, { "note", "#56949f" },
                                { "orange", "#e99d34" } } },
        { "atom-one-dark", { { "bg", "#282c34" }, { "panel", "#3b414d" }, { "text", "#abb2bf" },
                               { "subtle", "#767c87" }, { "accent", "#61afef" }, { "user", "#c678dd" },
                               { "tool", "#a378c2" }, { "error", "#ef6262" }, { "note", "#62f062" },
                               { "orange", "#ddb25b" } } },
        { "atom-one-light", { { "bg", "#fafafa" }, { "panel", "#e0e0e0" }, { "text", "#383a42" },
                                { "subtle", "#85868b" }, { "accent", "#4078f2" }, { "user", "#a626a4" },
                                { "tool", "#be9232" }, { "error", "#f13f3f" }, { "note", "#6bf23f" },
                                { "orange", "#d7d938" } } },
    };
    return p;
}

//: Seed colours per built-in theme.
const std::map<std::string, std::map<std::string, std::string>> &seeds()
{
    static const std::map<std::string, std::map<std::string, std::string>> s = {
        { "default", { { "darkMode", "false" }, { "background", "#ffffff" }, { "primaryColor", "#ECECFF" },
                         { "secondaryColor", "#ffffde" }, { "tertiaryColor", "#ffffde" },
                         { "primaryBorderColor", "#9370DB" }, { "secondaryBorderColor", "#aaaa33" },
                         { "tertiaryBorderColor", "#aaaa33" }, { "primaryTextColor", "#131300" },
                         { "secondaryTextColor", "#333333" }, { "tertiaryTextColor", "#333333" },
                         { "lineColor", "#333333" }, { "textColor", "#333333" },
                         { "edgeLabelBackground", "#e8e8e8" }, { "noteBkgColor", "#fff5ad" },
                         { "noteBorderColor", "#aaaa33" }, { "noteTextColor", "#333333" },
                         { "errorBkgColor", "#552222" }, { "errorTextColor", "#552222" } } },
        { "forest", { { "darkMode", "false" }, { "background", "#ffffff" }, { "primaryColor", "#cde498" },
                        { "secondaryColor", "#cdffb2" }, { "tertiaryColor", "#cdffb2" },
                        { "primaryBorderColor", "#13540c" }, { "secondaryBorderColor", "#6eaa49" },
                        { "tertiaryBorderColor", "#6eaa49" }, { "primaryTextColor", "#333333" },
                        { "secondaryTextColor", "#333333" }, { "tertiaryTextColor", "#333333" },
                        { "lineColor", "#1f7a1f" }, { "textColor", "#333333" },
                        { "edgeLabelBackground", "#e8e8e8" }, { "noteBkgColor", "#fff5ad" },
                        { "noteBorderColor", "#aaaa33" }, { "noteTextColor", "#333333" },
                        { "errorBkgColor", "#552222" }, { "errorTextColor", "#552222" } } },
        { "dark", { { "darkMode", "true" }, { "background", "#333333" }, { "primaryColor", "#1f2020" },
                      { "secondaryColor", "#3b3b3b" }, { "tertiaryColor", "#1f2020" }, { "altBackground", "#282a2c" },
                      { "primaryBorderColor", "#81B1DB" }, { "secondaryBorderColor", "#a0a0a0" },
                      { "tertiaryBorderColor", "#a0a0a0" }, { "primaryTextColor", "#cccccc" },
                      { "secondaryTextColor", "#cccccc" }, { "tertiaryTextColor", "#cccccc" },
                      { "lineColor", "#cccccc" }, { "textColor", "#cccccc" }, { "edgeLabelBackground", "#181818" },
                      { "noteBkgColor", "#fff5ad" }, { "noteBorderColor", "#d4b702" }, { "noteTextColor", "#1f2020" },
                      { "errorBkgColor", "#a44141" }, { "errorTextColor", "#dddddd" } } },
        { "neutral", { { "darkMode", "false" }, { "background", "#ffffff" }, { "primaryColor", "#eeeeee" },
                         { "secondaryColor", "#f4f4f4" }, { "tertiaryColor", "#f4f4f4" },
                         { "primaryBorderColor", "#999999" }, { "secondaryBorderColor", "#aaaaaa" },
                         { "tertiaryBorderColor", "#cccccc" }, { "primaryTextColor", "#333333" },
                         { "secondaryTextColor", "#333333" }, { "tertiaryTextColor", "#333333" },
                         { "lineColor", "#666666" }, { "textColor", "#333333" }, { "edgeLabelBackground", "#ffffff" },
                         { "noteBkgColor", "#fff5ad" }, { "noteBorderColor", "#aaaa33" },
                         { "noteTextColor", "#333333" }, { "errorBkgColor", "#552222" },
                         { "errorTextColor", "#552222" } } },
    };
    return s;
}

std::string via(const std::map<std::string, std::string> &v, const std::string &key, const std::string &fallback)
{
    auto it = v.find(key);
    return it == v.end() ? fallback : it->second;
}

//: The outline for a box filled with `fill`: the same colour a step darker
//: (or lighter on a dark page).
std::string peer(const std::string &fill, bool dark)
{
    return dark ? lighten(fill, 10) : darken(fill, 10);
}

//: Turn a ten-role palette into theme seeds.
std::map<std::string, std::string> paletteSeeds(const std::map<std::string, std::string> &roles)
{
    const std::string bg = roles.at("bg");
    const std::string text = roles.at("text");
    const std::string subtle = roles.at("subtle");
    const bool dark = isDark(bg);
    const std::string panel = separate(roles.at("panel"), bg, 1.25);

    std::map<std::string, std::string> s;
    s["darkMode"] = dark ? "true" : "false";
    s["background"] = bg;
    s["primaryColor"] = panel;
    s["primaryBorderColor"] = roles.at("accent");
    s["primaryTextColor"] = text;
    s["secondaryColor"] = mix(panel, roles.at("accent"), 0.18);
    s["secondaryBorderColor"] = roles.at("user");
    s["secondaryTextColor"] = text;
    s["tertiaryColor"] = mix(bg, panel, 0.55);
    s["tertiaryBorderColor"] = subtle;
    s["tertiaryTextColor"] = text;
    s["lineColor"] = mix(subtle, text, 0.35);
    s["textColor"] = text;
    s["mainBkg"] = panel;
    s["nodeBorder"] = roles.at("accent");
    s["clusterBkg"] = mix(bg, panel, 0.55);
    s["clusterBorder"] = subtle;
    s["edgeLabelBackground"] = mix(bg, panel, 0.75);
    s["titleColor"] = text;
    s["noteBkgColor"] = roles.at("note");
    s["noteBorderColor"] = darken(roles.at("note"), 12);
    s["noteTextColor"] = contrastText(roles.at("note"));
    s["errorBkgColor"] = roles.at("error");
    s["errorTextColor"] = contrastText(roles.at("error"));
    s["actorLineColor"] = subtle;
    s["altBackground"] = mix(bg, panel, 0.3);
    s["gridColor"] = mix(bg, subtle, 0.5);
    s["critBkgColor"] = roles.at("error");
    s["critBorderColor"] = lighten(roles.at("error"), 10);
    s["todayLineColor"] = roles.at("error");
    s["doneTaskBkgColor"] = mix(bg, subtle, 0.6);
    s["doneTaskBorderColor"] = subtle;
    s["activeTaskBkgColor"] = mix(panel, roles.at("accent"), 0.35);
    s["activeTaskBorderColor"] = roles.at("accent");
    s["specialStateColor"] = text;
    s["quadrantPointFill"] = roles.at("accent");
    s["xyChartAxisLineColor"] = subtle;
    s["xyChartAxisLabelColor"] = text;
    s["xyChartTitleColor"] = text;

    // The five accent roles seed the categorical ramps; a repeated colour takes
    // the palette's own sixth hue, then is nudged around the wheel.
    std::vector<std::string> accents;
    auto taken = [&accents](const std::string &colour) {
        for (const std::string &seen : accents) {
            if (sameColour(colour, seen)) {
                return true;
            }
        }
        return false;
    };
    for (const std::string &role : kAccentRoles) {
        std::string colour = roles.at(role);
        if (taken(colour) && !taken(roles.at("error"))) {
            colour = roles.at("error");
        }
        int attempt = 0;
        while (taken(colour)) {
            ++attempt;
            if (attempt > 12) {
                break;
            }
            colour = adjust(roles.at(role), attempt * 24, 0, (dark ? 6 : -6) * ((attempt + 1) / 2));
        }
        accents.push_back(colour);
    }

    auto ramp = [&accents, dark](int index) {
        const std::string base = accents[index % accents.size()];
        const int cycle = index / static_cast<int>(accents.size());
        if (cycle == 0) {
            return base;
        }
        return lighten(base, (dark ? 10 : -10) * cycle);
    };

    for (int i = 0; i < 12; ++i) {
        s["pie" + std::to_string(i + 1)] = ramp(i);
    }
    for (int i = 0; i < 8; ++i) {
        s["cScale" + std::to_string(i)] = ramp(i);
        s["cScaleLabel" + std::to_string(i)] = contrastText(ramp(i));
        s["git" + std::to_string(i)] = ramp(i);
        s["gitBranchLabel" + std::to_string(i)] = contrastText(ramp(i));
    }
    std::string xy;
    for (int i = 0; i < 5; ++i) {
        if (i > 0) {
            xy += ", ";
        }
        xy += ramp(i);
    }
    s["xyChartPlotColorPalette"] = xy;
    for (int i = 0; i < 4; ++i) {
        s["quadrant" + std::to_string(i + 1) + "Fill"] = mix(bg, accents[i], 0.14);
    }
    return s;
}

//: Fill in every variable derived from the seed colours.
std::map<std::string, std::string> derive(std::map<std::string, std::string> v)
{
    const bool dark = via(v, "darkMode", "") == "true";
    const std::string primary = v.at("primaryColor");
    const std::string secondary = v.at("secondaryColor");
    const std::string tertiary = v.at("tertiaryColor");
    const std::string line = v.at("lineColor");

    auto def = [&v](const std::string &key, const std::string &value) {
        if (v.find(key) == v.end()) {
            v[key] = value;
        }
    };

    def("mainBkg", primary);
    v["textColor"] = legible(via(v, "textColor", "#333"), v.at("mainBkg"));
    const std::string text = v["textColor"];
    def("nodeBorder", v.at("primaryBorderColor"));
    def("clusterBkg", tertiary);
    def("clusterBorder", v.at("tertiaryBorderColor"));
    def("titleColor", text);

    // flowchart
    def("nodeTextColor", legible(v.at("primaryTextColor"), v.at("mainBkg")));
    def("defaultLinkColor", line);
    def("clusterTextColor", v.at("tertiaryTextColor"));
    def("edgeLabelTextColor", text);
    def("arrowheadColor", line);

    // sequence
    def("actorBkg", v.at("mainBkg"));
    def("actorBorder", v.at("primaryBorderColor"));
    def("actorTextColor", v.at("primaryTextColor"));
    def("actorLineColor", dark ? "#cccccc" : darken(v.at("mainBkg"), 20));
    def("signalColor", text);
    def("signalTextColor", text);
    def("labelBoxBkgColor", primary);
    def("labelBoxBorderColor", v.at("primaryBorderColor"));
    def("labelTextColor", v.at("primaryTextColor"));
    def("loopTextColor", v.at("primaryTextColor"));
    def("activationBorderColor", darken(primary, 10));
    def("activationBkgColor", secondary);
    def("sequenceNumberColor", contrastText(via(v, "signalColor", line), "#ffffff", "#000000"));
    def("altBackground", tertiary);

    // class / state / er
    def("classText", v.at("primaryTextColor"));
    def("attributeBackgroundColorOdd", dark ? "#33333a" : lighten(v.at("background"), 0));
    def("attributeBackgroundColorEven", dark ? "#2b2b30" : darken(v.at("background"), 4));
    def("labelColor", text);
    def("compositeBackground", primary);
    def("compositeBorder", v.at("primaryBorderColor"));
    def("compositeTitleBackground", lighten(primary, 5));
    def("altSectionBkgColor", dark ? "#3b3b3b" : "#ffffff");
    def("innerEndBackground", v.at("primaryBorderColor"));
    def("specialStateColor", line);
    def("transitionColor", line);
    def("transitionLabelColor", text);
    def("stateLabelColor", v.at("primaryTextColor"));
    def("stateBkg", v.at("mainBkg"));

    // pie
    for (int i = 0; i < 12; ++i) {
        def("pie" + std::to_string(i + 1), categorical(primary, i, dark));
    }
    def("pieTitleTextSize", "25px");
    def("pieTitleTextColor", v.at("titleColor"));
    def("pieSectionTextSize", "17px");
    def("pieSectionTextColor", v.at("textColor"));
    def("pieLegendTextSize", "17px");
    def("pieLegendTextColor", v.at("textColor"));
    def("pieStrokeColor", dark ? "#cccccc" : "black");
    def("pieStrokeWidth", "2px");
    def("pieOuterStrokeWidth", "2px");
    def("pieOuterStrokeColor", dark ? "#cccccc" : "black");
    def("pieOpacity", "0.7");

    // gantt
    def("sectionBkgColor", tertiary);
    def("sectionBkgColor2", dark ? lighten(tertiary, 6) : lighten(tertiary, -6));
    def("taskBorderColor", v.at("primaryBorderColor"));
    def("taskBkgColor", primary);
    def("taskTextDarkColor", text);
    def("taskTextLightColor", "white");
    def("taskTextColor", contrastText(v.at("taskBkgColor"), v.at("taskTextDarkColor"), v.at("taskTextLightColor")));
    def("taskTextOutsideColor", v.at("taskTextDarkColor"));
    def("taskTextClickableColor", "#003163");
    def("activeTaskBorderColor", v.at("primaryBorderColor"));
    def("activeTaskBkgColor", dark ? darken(primary, 8) : lighten(primary, 12));
    def("gridColor", dark ? "#555555" : "lightgrey");
    def("doneTaskBkgColor", dark ? "#666666" : "lightgrey");
    def("doneTaskBorderColor", "grey");
    def("critBorderColor", "#ff8888");
    def("critBkgColor", "red");
    def("todayLineColor", "red");

    // git
    for (int i = 0; i < 8; ++i) {
        def("git" + std::to_string(i), categorical(primary, i, dark, 53.0));
        def("gitBranchLabel" + std::to_string(i), contrastText(v.at("git" + std::to_string(i))));
    }
    def("commitLabelColor", v.at("secondaryTextColor"));
    def("commitLabelBackground", secondary);
    def("commitLabelFontSize", "10px");
    def("tagLabelColor", v.at("primaryTextColor"));
    def("tagLabelBackground", primary);
    def("tagLabelBorder", v.at("primaryBorderColor"));
    def("tagLabelFontSize", "10px");

    // journey
    for (int i = 0; i < 8; ++i) {
        def("cScale" + std::to_string(i), categorical(primary, i, dark, 61.0));
        def("cScaleLabel" + std::to_string(i), contrastText(v.at("cScale" + std::to_string(i))));
        def("cScalePeer" + std::to_string(i), peer(v.at("cScale" + std::to_string(i)), dark));
    }
    {
        const std::vector<std::pair<std::string, int>> fills = { { primary, 0 }, { secondary, 0 }, { primary, 64 },
            { secondary, 64 }, { primary, -64 }, { secondary, -64 }, { primary, 128 }, { secondary, 128 } };
        for (int index = 0; index < static_cast<int>(fills.size()); ++index) {
            const std::string key = "fillType" + std::to_string(index);
            def(key, fills[index].second == 0 ? fills[index].first
                                              : adjust(fills[index].first, fills[index].second, 0, 0));
            def("fillTypePeer" + std::to_string(index), peer(v.at(key), dark));
        }
    }

    // quadrant / xy / radar
    def("quadrant1Fill", primary);
    def("quadrant2Fill", adjust(primary, 8, 0, dark ? 3 : -3));
    def("quadrant3Fill", adjust(primary, 16, 0, dark ? 6 : -6));
    def("quadrant4Fill", adjust(primary, 24, 0, dark ? 9 : -9));
    def("quadrantPointFill", v.at("primaryBorderColor"));
    def("quadrantPointTextFill", text);
    def("quadrantTitleFill", text);
    def("quadrantXAxisTextFill", text);
    def("quadrantYAxisTextFill", text);
    def("quadrantInternalBorderStrokeFill", v.at("primaryBorderColor"));
    def("quadrantExternalBorderStrokeFill", v.at("primaryBorderColor"));
    def("xyChartTitleColor", text);
    def("xyChartAxisLineColor", text);
    def("xyChartAxisLabelColor", text);
    {
        std::string xy;
        for (int i = 0; i < 5; ++i) {
            if (i > 0) {
                xy += ", ";
            }
            xy += categorical(primary, i, dark);
        }
        def("xyChartPlotColorPalette", xy);
    }

    def("fontFamily", DEFAULT_FONT_FAMILY);
    def("fontSize", "16px");

    // Last pass: every text role checked against the surface it is painted on.
    const std::vector<std::pair<std::string, std::vector<std::string>>> checks = {
        { "primaryTextColor", { "mainBkg" } },
        { "secondaryTextColor", { "secondaryColor" } },
        { "tertiaryTextColor", { "tertiaryColor" } },
        { "nodeTextColor", { "mainBkg" } },
        { "clusterTextColor", { "clusterBkg" } },
        { "actorTextColor", { "actorBkg" } },
        { "stateLabelColor", { "stateBkg" } },
        { "classText", { "mainBkg" } },
        { "loopTextColor", { "mainBkg" } },
        { "labelTextColor", { "labelBackground", "mainBkg" } },
        { "sectionTextColor", { "sectionBkgColor", "sectionBkgColor2" } },
    };
    for (const auto &entry : checks) {
        for (const std::string &surface : entry.second) {
            if (v.count(entry.first) && v.count(surface)) {
                v[entry.first] = legible(v[entry.first], v[surface]);
            }
        }
    }
    return v;
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

std::string cssValue(const std::string &value)
{
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (c == '<' || c == '>' || c == '{' || c == '}' || c == ';' || c == '\\' || c == '\n' || c == '\r'
                || (c == '/' && i + 1 < value.size() && value[i + 1] == '*')) {
            return trim(value.substr(0, i));
        }
    }
    return value;
}

std::string Theme::get(const std::string &key, const std::string &fallback) const
{
    auto it = vars.find(key);
    if (it == vars.end() || it->second.empty()) {
        return fallback;
    }
    const std::string cleaned = cssValue(it->second);
    return cleaned.empty() ? fallback : cleaned;
}

std::string Theme::fontFamily() const
{
    return get("fontFamily", DEFAULT_FONT_FAMILY);
}

double Theme::fontSize() const
{
    const std::string raw = get("fontSize", "16px");
    std::string digits;
    for (char c : raw) {
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-') {
            digits.push_back(c);
        }
    }
    return digits.empty() ? 16.0 : std::atof(digits.c_str());
}

bool Theme::dark() const
{
    return get("darkMode") == "true";
}

std::string Theme::palette(const std::string &prefix, int index, int count) const
{
    return get(prefix + std::to_string(index % count), get("primaryColor", "#eee"));
}

std::vector<std::string> allThemeNames()
{
    std::vector<std::string> names = { "default", "forest", "dark", "neutral", "base" };
    for (const auto &entry : palettes()) {
        names.push_back(entry.first);
    }
    return names;
}

Theme buildTheme(const std::string &name, const std::map<std::string, std::string> &overrides)
{
    const std::map<std::string, std::map<std::string, std::string>> &p = palettes();
    std::map<std::string, std::string> base;
    if (p.count(name)) {
        base = paletteSeeds(p.at(name));
    } else if (seeds().count(name)) {
        base = seeds().at(name);
    } else {
        base = seeds().at("default");
    }
    if (!overrides.empty()) {
        std::map<std::string, std::string> seedKeys = seeds().at("default");
        seedKeys["fontFamily"] = "";
        seedKeys["fontSize"] = "";
        for (const auto &entry : overrides) {
            if (seedKeys.count(entry.first) && !entry.second.empty()) {
                base[entry.first] = entry.second;
            }
        }
    }
    std::map<std::string, std::string> resolved = derive(base);
    for (const auto &entry : overrides) {
        if (!entry.second.empty()) {
            resolved[entry.first] = entry.second;
        }
    }
    Theme theme;
    theme.name = name;
    theme.vars = resolved;
    return theme;
}

} // namespace tqtw_mermaid2svg
