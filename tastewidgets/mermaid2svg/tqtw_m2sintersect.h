/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#pragma once

// Clip an edge polyline at the boundary of its endpoint node: a C++ port of
// mermaid2svg's layout/intersect.py.

#include "tqtw_m2scurves.h"

#include <string>
#include <vector>

namespace tqtw_mermaid2svg {

Point rectIntersect(double cx, double cy, double w, double h, const Point &outside);
Point ellipseIntersect(double cx, double cy, double rx, double ry, const Point &outside);
Point circleIntersect(double cx, double cy, double r, const Point &outside);
Point polygonIntersect(double cx, double cy, const std::vector<Point> &points, const Point &outside);

//: The outline a named shape is clipped against.
Point shapeIntersect(const std::string &shape, double cx, double cy, double w, double h, const Point &outside);

std::vector<Point> diamondPoints(double cx, double cy, double w, double h);
std::vector<Point> slantedPoints(const std::string &shape, double cx, double cy, double w, double h);

} // namespace tqtw_mermaid2svg
