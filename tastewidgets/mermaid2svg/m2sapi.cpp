/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2sapi.h"

#include "tqtw_m2sbase.h"
#include "tqtw_m2sflowchart.h"
#include "tqtw_m2ssequence.h"
#include "tqtw_m2sclass.h"
#include "tqtw_m2ser.h"
#include "tqtw_m2sstate.h"
#include "tqtw_m2sfrontmatter.h"
#include "tqtw_m2stext.h"
#include "tqtw_m2stheme.h"

#include <cstdlib>
#include <memory>

namespace tqtw_mermaid2svg {

namespace {

//: The font the measurer came out of. The default names no one installed font,
//: so no font CSS is emitted and the renderer falls back to its own.
std::shared_ptr<TextMeasurer> buildMeasurer(const Theme &theme)
{
    auto metrics = std::make_shared<BuiltinMetrics>();
    return std::make_shared<TextMeasurer>(metrics, theme.fontSize(), 1.25);
}

RenderContext buildContext(const Options &options, const Theme &theme)
{
    RenderContext ctx;
    ctx.theme = theme;
    ctx.measurer = buildMeasurer(theme);
    ctx.config = JsonValue::object();
    for (const auto &entry : options.config) {
        ctx.config.set(entry.first, JsonValue::str(entry.second));
    }
    ctx.svgId = options.svgId;
    ctx.background = options.backgroundColor;
    ctx.extraCss = options.css;
    return ctx;
}

//: Build the diagram object for a detected type. Throws UnsupportedDiagramError
//: for a recognised type this port does not implement yet.
std::shared_ptr<DiagramBase> makeDiagram(const std::string &id, const Source &source, Diagnostics *diagnostics)
{
    if (id == "flowchart") {
        return Flowchart::parse(source, diagnostics);
    }
    if (id == "sequence") {
        return SequenceDiagram::parse(source, diagnostics);
    }
    if (id == "state") {
        return StateDiagram::parse(source, diagnostics);
    }
    if (id == "class") {
        return ClassDiagram::parse(source, diagnostics);
    }
    if (id == "er") {
        return ErDiagram::parse(source, diagnostics);
    }
    throw UnsupportedDiagramError(id, "not yet ported to C++");
}

} // namespace

std::string detectDiagramType(const std::string &text)
{
    Diagnostics diagnostics;
    const Source source = preprocess(text, &diagnostics);
    return detectType(source).first;
}

Result render(const std::string &text, const Options &options, Diagnostics *diagnostics)
{
    Diagnostics local;
    if (diagnostics == nullptr) {
        diagnostics = &local;
    }
    const Source source = preprocess(text, diagnostics);
    const auto type = detectType(source);
    const std::string id = type.first;

    const Theme theme = buildTheme(options.theme, {});
    const RenderContext ctx = buildContext(options, theme);
    const std::shared_ptr<DiagramBase> diagram = makeDiagram(id, source, diagnostics);
    const RenderResult result = diagram->render(ctx);

    Result out;
    out.svg = result.svg;
    out.width = result.width;
    out.height = result.height;
    out.hasTitle = result.hasTitle;
    out.title = result.title;
    out.hasDesc = result.hasDesc;
    out.desc = result.desc;
    out.diagramType = id;
    return out;
}

Result render(const std::string &text, const Options &options)
{
    return render(text, options, nullptr);
}

} // namespace tqtw_mermaid2svg
