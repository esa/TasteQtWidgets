/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2sintersect.h"

#include <cmath>
#include <limits>

namespace tqtw_mermaid2svg {

namespace {

bool segmentIntersect(const Point &p1, const Point &p2, const Point &p3, const Point &p4, Point *out)
{
    const double x1 = p1.first, y1 = p1.second;
    const double x2 = p2.first, y2 = p2.second;
    const double x3 = p3.first, y3 = p3.second;
    const double x4 = p4.first, y4 = p4.second;
    const double denom = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);
    if (std::fabs(denom) < 1e-12) {
        return false;
    }
    const double ua = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / denom;
    const double ub = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / denom;
    if (ua < 0 || ua > 1 || ub < 0 || ub > 1) {
        return false;
    }
    out->first = x1 + ua * (x2 - x1);
    out->second = y1 + ua * (y2 - y1);
    return true;
}

} // namespace

Point rectIntersect(double cx, double cy, double w, double h, const Point &outside)
{
    const double dx = outside.first - cx;
    const double dy = outside.second - cy;
    if (dx == 0 && dy == 0) {
        return { cx, cy };
    }
    const double hw = w / 2;
    const double hh = h / 2;
    if (dx == 0) {
        return { cx, cy + std::copysign(hh, dy) };
    }
    if (dy == 0) {
        return { cx + std::copysign(hw, dx), cy };
    }
    const double sx = hw / std::fabs(dx);
    const double sy = hh / std::fabs(dy);
    const double s = std::min(sx, sy);
    return { cx + dx * s, cy + dy * s };
}

Point ellipseIntersect(double cx, double cy, double rx, double ry, const Point &outside)
{
    const double dx = outside.first - cx;
    const double dy = outside.second - cy;
    if (dx == 0 && dy == 0) {
        return { cx, cy };
    }
    if (rx <= 0 || ry <= 0) {
        return { cx, cy };
    }
    const double denom = std::sqrt((dx * dx) / (rx * rx) + (dy * dy) / (ry * ry));
    if (denom == 0) {
        return { cx, cy };
    }
    return { cx + dx / denom, cy + dy / denom };
}

Point circleIntersect(double cx, double cy, double r, const Point &outside)
{
    return ellipseIntersect(cx, cy, r, r, outside);
}

Point polygonIntersect(double cx, double cy, const std::vector<Point> &points, const Point &outside)
{
    Point best { 0, 0 };
    bool found = false;
    double bestDist = std::numeric_limits<double>::infinity();
    const std::size_t n = points.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Point &a = points[i];
        const Point &b = points[(i + 1) % n];
        Point hit;
        if (!segmentIntersect(a, b, { cx, cy }, outside, &hit)) {
            continue;
        }
        const double dist = std::hypot(hit.first - outside.first, hit.second - outside.second);
        if (dist < bestDist) {
            bestDist = dist;
            best = hit;
            found = true;
        }
    }
    return found ? best : Point { cx, cy };
}

std::vector<Point> diamondPoints(double cx, double cy, double w, double h)
{
    const double hw = w / 2;
    const double hh = h / 2;
    return { { cx, cy - hh }, { cx + hw, cy }, { cx, cy + hh }, { cx - hw, cy } };
}

std::vector<Point> slantedPoints(const std::string &shape, double cx, double cy, double w, double h)
{
    const double hw = w / 2;
    const double hh = h / 2;
    const double k = h / 2;
    if (shape == "lean_right") {
        return { { cx - hw + k, cy - hh }, { cx + hw, cy - hh }, { cx + hw - k, cy + hh }, { cx - hw, cy + hh } };
    }
    if (shape == "lean_left") {
        return { { cx - hw, cy - hh }, { cx + hw - k, cy - hh }, { cx + hw, cy + hh }, { cx - hw + k, cy + hh } };
    }
    if (shape == "trapezoid") {
        return { { cx - hw + k, cy - hh }, { cx + hw - k, cy - hh }, { cx + hw, cy + hh }, { cx - hw, cy + hh } };
    }
    return { { cx - hw, cy - hh }, { cx + hw, cy - hh }, { cx + hw - k, cy + hh }, { cx - hw + k, cy + hh } };
}

Point shapeIntersect(const std::string &shape, double cx, double cy, double w, double h, const Point &outside)
{
    if (shape == "circle" || shape == "doublecircle") {
        return circleIntersect(cx, cy, std::max(w, h) / 2, outside);
    }
    if (shape == "ellipse" || shape == "stadium" || shape == "cylinder") {
        return ellipseIntersect(cx, cy, w / 2, h / 2, outside);
    }
    if (shape == "diamond" || shape == "rhombus" || shape == "question" || shape == "decision") {
        return polygonIntersect(cx, cy, diamondPoints(cx, cy, w, h), outside);
    }
    if (shape == "hexagon") {
        const double hw = w / 2;
        const double hh = h / 2;
        const double k = h / 4;
        std::vector<Point> points = { { cx - hw + k, cy - hh }, { cx + hw - k, cy - hh }, { cx + hw, cy },
            { cx + hw - k, cy + hh }, { cx - hw + k, cy + hh }, { cx - hw, cy } };
        return polygonIntersect(cx, cy, points, outside);
    }
    if (shape == "lean_right" || shape == "lean_left" || shape == "trapezoid" || shape == "inv_trapezoid") {
        return polygonIntersect(cx, cy, slantedPoints(shape, cx, cy, w, h), outside);
    }
    return rectIntersect(cx, cy, w, h, outside);
}

} // namespace tqtw_mermaid2svg
