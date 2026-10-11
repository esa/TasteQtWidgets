/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>
   SPDX-License-Identifier: MIT

   A C++ port of mermaid2svg. The original mermaid2svg and the mermaid it
   reimplements are both MIT licensed; see LICENSE in this directory for
   their notices.
*/

#include "tqtw_m2scurves.h"

#include "tqtw_m2ssvg.h"

#include <algorithm>
#include <cmath>

namespace tqtw_mermaid2svg {

namespace {

std::vector<Point> dedupe(const std::vector<Point> &points)
{
    std::vector<Point> out;
    for (const Point &p : points) {
        if (out.empty() || std::fabs(p.first - out.back().first) > 1e-9
                || std::fabs(p.second - out.back().second) > 1e-9) {
            out.push_back(p);
        }
    }
    return out;
}

std::pair<std::vector<double>, std::vector<double>> naturalControls(const std::vector<double> &x)
{
    const int n = static_cast<int>(x.size()) - 1;
    std::vector<double> a(n), b(n), r(n);
    a[0] = 0.0;
    b[0] = 2.0;
    r[0] = x[0] + 2 * x[1];
    for (int i = 1; i < n - 1; ++i) {
        a[i] = 1.0;
        b[i] = 4.0;
        r[i] = 4 * x[i] + 2 * x[i + 1];
    }
    a[n - 1] = 2.0;
    b[n - 1] = 7.0;
    r[n - 1] = 8 * x[n - 1] + x[n];
    for (int i = 1; i < n; ++i) {
        const double m = a[i] / b[i - 1];
        b[i] -= m;
        r[i] -= m * r[i - 1];
    }
    a[n - 1] = r[n - 1] / b[n - 1];
    for (int i = n - 2; i >= 0; --i) {
        a[i] = (r[i] - a[i + 1]) / b[i];
    }
    b[n - 1] = (x[n] + a[n - 1]) / 2;
    for (int i = 0; i < n - 1; ++i) {
        b[i] = 2 * x[i + 1] - a[i + 1];
    }
    return { a, b };
}

} // namespace

std::string curveLinear(const std::vector<Point> &pointsIn)
{
    const std::vector<Point> points = dedupe(pointsIn);
    if (points.empty()) {
        return std::string();
    }
    Path path;
    path.polyline(points);
    return path.str();
}

std::string curveBasis(const std::vector<Point> &pointsIn)
{
    const std::vector<Point> p = dedupe(pointsIn);
    const int n = static_cast<int>(p.size());
    if (n == 0) {
        return std::string();
    }
    if (n == 1) {
        return Path().M(p[0].first, p[0].second).str();
    }
    if (n == 2) {
        return Path().M(p[0].first, p[0].second).L(p[1].first, p[1].second).str();
    }
    Path path;
    const double x0 = p[0].first;
    const double y0 = p[0].second;
    path.M(x0, y0);
    path.L((5 * x0 + p[1].first) / 6, (5 * y0 + p[1].second) / 6);
    for (int i = 1; i < n - 1; ++i) {
        const double xa = p[i - 1].first, ya = p[i - 1].second;
        const double xb = p[i].first, yb = p[i].second;
        const double xc = p[i + 1].first, yc = p[i + 1].second;
        path.C((2 * xa + xb) / 3, (2 * ya + yb) / 3, (xa + 2 * xb) / 3, (ya + 2 * yb) / 3,
                (xa + 4 * xb + xc) / 6, (ya + 4 * yb + yc) / 6);
    }
    const double xa = p[n - 2].first, ya = p[n - 2].second;
    const double xb = p[n - 1].first, yb = p[n - 1].second;
    path.C((2 * xa + xb) / 3, (2 * ya + yb) / 3, (xa + 2 * xb) / 3, (ya + 2 * yb) / 3, xb, yb);
    return path.str();
}

std::string curveCardinal(const std::vector<Point> &pointsIn, double tension)
{
    const std::vector<Point> points = dedupe(pointsIn);
    const int n = static_cast<int>(points.size());
    if (n < 3) {
        return curveLinear(points);
    }
    const double k = (1 - tension) / 6;
    Path path;
    path.M(points[0].first, points[0].second);
    std::vector<Point> extended;
    extended.push_back(points[0]);
    for (const Point &p : points) {
        extended.push_back(p);
    }
    extended.push_back(points.back());
    for (std::size_t i = 1; i + 2 < extended.size(); ++i) {
        const Point &p0 = extended[i - 1];
        const Point &p1 = extended[i];
        const Point &p2 = extended[i + 1];
        const Point &p3 = extended[i + 2];
        path.C(p1.first + k * (p2.first - p0.first), p1.second + k * (p2.second - p0.second),
                p2.first - k * (p3.first - p1.first), p2.second - k * (p3.second - p1.second), p2.first, p2.second);
    }
    return path.str();
}

std::string curveNatural(const std::vector<Point> &pointsIn)
{
    const std::vector<Point> points = dedupe(pointsIn);
    const int n = static_cast<int>(points.size());
    if (n < 3) {
        return curveLinear(points);
    }
    std::vector<double> xs, ys;
    for (const Point &p : points) {
        xs.push_back(p.first);
        ys.push_back(p.second);
    }
    const auto px = naturalControls(xs);
    const auto py = naturalControls(ys);
    Path path;
    path.M(points[0].first, points[0].second);
    for (int i = 0; i < n - 1; ++i) {
        path.C(px.first[i], py.first[i], px.second[i], py.second[i], points[i + 1].first, points[i + 1].second);
    }
    return path.str();
}

std::string curveStep(const std::vector<Point> &pointsIn, double where)
{
    const std::vector<Point> points = dedupe(pointsIn);
    if (points.size() < 2) {
        return curveLinear(points);
    }
    Path path;
    path.M(points[0].first, points[0].second);
    for (std::size_t i = 1; i < points.size(); ++i) {
        const Point &p0 = points[i - 1];
        const Point &p1 = points[i];
        const double xm = p0.first + (p1.first - p0.first) * where;
        path.L(xm, p0.second).L(xm, p1.second).L(p1.first, p1.second);
    }
    return path.str();
}

std::string renderCurve(const std::vector<Point> &points, const std::string &curve)
{
    std::string key = curve.empty() ? "basis" : curve;
    std::string lowered;
    for (char c : key) {
        const char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lc != '-' && lc != '_') {
            lowered.push_back(lc);
        }
    }
    if (lowered.rfind("curve", 0) == 0) {
        lowered = lowered.substr(5);
    }
    if (lowered == "linear") {
        return curveLinear(points);
    }
    if (lowered == "cardinal" || lowered == "catmullrom") {
        return curveCardinal(points);
    }
    if (lowered == "natural" || lowered == "monotonex" || lowered == "monotoney") {
        return curveNatural(points);
    }
    if (lowered == "step") {
        return curveStep(points, 0.5);
    }
    if (lowered == "stepbefore") {
        return curveStep(points, 0.0);
    }
    if (lowered == "stepafter") {
        return curveStep(points, 1.0);
    }
    return curveBasis(points);
}

} // namespace tqtw_mermaid2svg
