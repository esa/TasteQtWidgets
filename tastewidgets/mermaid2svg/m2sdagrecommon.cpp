/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2sdagrecommon.h"

#include "tqtw_m2scurves.h"
#include "tqtw_m2sintersect.h"

namespace tqtw_mermaid2svg {

void runLayout(DagreGraph &g, const std::string &direction, double nodesep, double ranksep, double padding)
{
    LayoutOptions opts;
    opts.rankdir = direction;
    opts.nodesep = nodesep;
    opts.ranksep = ranksep;
    opts.edgesep = nodesep / 5;
    opts.clusterPadding = nodesep / 2 + CLUSTER_CROSS_PADDING;
    opts.clusterPaddingX = nodesep / 2 + CLUSTER_CROSS_PADDING;
    opts.clusterPaddingY = ranksep / 2;
    opts.marginX = padding;
    opts.marginY = padding;
    dagreLayout(g, opts);
}

std::vector<Point> clipPoints(const DagreGraph &g, const DagreEdge &edge, const std::string &sourceShape,
        const std::string &targetShape)
{
    auto sIt = g.nodes.find(edge.v);
    auto tIt = g.nodes.find(edge.w);
    if (sIt == g.nodes.end() || tIt == g.nodes.end()) {
        return {};
    }
    const DagreNode &source = sIt->second;
    const DagreNode &target = tIt->second;
    const std::vector<Point> mid = edge.points;
    const Point first = mid.empty() ? Point { target.x, target.y } : mid.front();
    const Point last = mid.empty() ? Point { source.x, source.y } : mid.back();
    const Point start = shapeIntersect(sourceShape, source.x, source.y, source.width, source.height, first);
    const Point end = shapeIntersect(targetShape, target.x, target.y, target.width, target.height, last);
    std::vector<Point> out;
    out.push_back(start);
    for (const Point &p : mid) out.push_back(p);
    out.push_back(end);
    return out;
}

std::shared_ptr<El> edgePath(const std::vector<Point> &points, const std::string &curve,
        const std::map<std::string, std::string> &attrs)
{
    auto el = std::make_shared<El>("path");
    el->attr("d", renderCurve(points, curve));
    el->attr("fill", "none");
    for (const auto &a : attrs) {
        el->attr(a.first, a.second);
    }
    return el;
}

std::string hollowMarker(SvgDocument &doc, const std::string &name, const std::string &colour,
        const std::string &fill, const std::string &kind, bool atStart)
{
    const std::string markerId = doc.svgId() + "_" + name + (atStart ? "-start" : "");
    if (doc.hasMarker(markerId)) {
        return "url(#" + markerId + ")";
    }
    std::string forwards, backwards;
    double width;
    bool openHead;
    if (kind == "triangle") {
        forwards = "M 1 1 L 13 7 L 1 13 z";
        backwards = "M 13 1 L 1 7 L 13 13 z";
        width = 14;
        openHead = false;
    } else if (kind == "diamond") {
        forwards = "M 1 7 L 8 2 L 15 7 L 8 12 z";
        backwards = "M 15 7 L 8 2 L 1 7 L 8 12 z";
        width = 16;
        openHead = false;
    } else {
        forwards = "M 1 1 L 12 7 L 1 13";
        backwards = "M 12 1 L 1 7 L 12 13";
        width = 13;
        openHead = true;
    }
    auto marker = std::make_shared<El>("marker");
    marker->attr("id", markerId);
    marker->attr("viewBox", "0 0 " + fmt(width) + " 14");
    marker->attr("refX", atStart ? 1.0 : width - 1);
    marker->attr("refY", 7);
    marker->attr("markerUnits", "userSpaceOnUse");
    marker->attr("markerWidth", width);
    marker->attr("markerHeight", 14);
    marker->attr("orient", "auto");
    auto path = std::make_shared<El>("path");
    path->attr("d", atStart ? backwards : forwards);
    path->attr("fill", openHead ? "none" : fill);
    path->attr("stroke", colour);
    path->attr("stroke-width", 1);
    marker->add(path);
    doc.addMarker(markerId, marker);
    return "url(#" + markerId + ")";
}

} // namespace tqtw_mermaid2svg
