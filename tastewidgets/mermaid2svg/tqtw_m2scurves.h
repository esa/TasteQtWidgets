/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Spline interpolators for edge routing: a C++ port of mermaid2svg's
// layout/curves.py.

#include <string>
#include <utility>
#include <vector>

namespace tqtw_mermaid2svg {

using Point = std::pair<double, double>;

std::string curveLinear(const std::vector<Point> &points);
std::string curveBasis(const std::vector<Point> &points);
std::string curveCardinal(const std::vector<Point> &points, double tension = 0.0);
std::string curveNatural(const std::vector<Point> &points);
std::string curveStep(const std::vector<Point> &points, double where = 0.5);

//: Render `points` with the named curve, falling back to basis.
std::string renderCurve(const std::vector<Point> &points, const std::string &curve = "basis");

} // namespace tqtw_mermaid2svg
