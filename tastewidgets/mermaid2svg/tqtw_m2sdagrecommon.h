/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Shared helpers for the diagram types laid out as a graph (state, class, er):
// a C++ port of mermaid2svg's diagrams/dagre_common.py.

#include "tqtw_m2sdagre.h"
#include "tqtw_m2ssvg.h"

#include <string>
#include <vector>

namespace tqtw_mermaid2svg {

//: Extra room across the ranks inside a cluster.
constexpr double CLUSTER_CROSS_PADDING = 10.0;

//: Lay `g` out with the common options.
void runLayout(DagreGraph &g, const std::string &direction, double nodesep = 50.0, double ranksep = 50.0,
        double padding = 8.0);

//: Full polyline for `edge`, clipped at both endpoint boundaries.
std::vector<Point> clipPoints(const DagreGraph &g, const DagreEdge &edge, const std::string &sourceShape = "rect",
        const std::string &targetShape = "rect");

//: A path element from `points` with the given attributes.
std::shared_ptr<El> edgePath(const std::vector<Point> &points, const std::string &curve = "basis",
        const std::map<std::string, std::string> &attrs = {});

//: Register a UML arrowhead (triangle/diamond/open); returns its url(#...).
std::string hollowMarker(SvgDocument &doc, const std::string &name, const std::string &colour,
        const std::string &fill, const std::string &kind, bool atStart = false);

} // namespace tqtw_mermaid2svg
