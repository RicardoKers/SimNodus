// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Grid geometry of the schematic. One grid unit is the canvas grid step; all
// pins, wire vertices and component origins are integer grid points. The y
// axis points down (screen convention); rotations are clockwise on screen.
#pragma once

#include <cstdint>

namespace simnodus::model {

struct Point {
    int x = 0;
    int y = 0;

    friend constexpr bool operator==(const Point&, const Point&) = default;
    // Explicit ordering instead of <=>: Qt 6.4's moc cannot parse <compare>.
    friend constexpr bool operator<(const Point& a, const Point& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); }
    constexpr Point operator+(const Point& o) const { return {x + o.x, y + o.y}; }
    constexpr Point operator-(const Point& o) const { return {x - o.x, y - o.y}; }
    constexpr Point operator-() const { return {-x, -y}; }
};

struct PointF {
    double x = 0.0;
    double y = 0.0;
};

// Orientation of a placed symbol: mirror (about the symbol's vertical axis)
// is applied first, then a clockwise rotation in 90 degree steps.
struct Orientation {
    int rotation = 0; // 0, 90, 180 or 270
    bool mirrored = false;

    friend constexpr bool operator==(const Orientation&, const Orientation&) = default;
};

constexpr int normalized_rotation(int degrees)
{
    int r = degrees % 360;
    if (r < 0) {
        r += 360;
    }
    return (r / 90) * 90;
}

template <typename P>
constexpr P orient(const P& local, const Orientation& o)
{
    P p = local;
    if (o.mirrored) {
        p.x = -p.x;
    }
    switch (normalized_rotation(o.rotation)) {
    case 90:
        return P{-p.y, p.x};
    case 180:
        return P{-p.x, -p.y};
    case 270:
        return P{p.y, -p.x};
    default:
        return p;
    }
}

// Local symbol point -> schematic point for a symbol placed at `origin`.
constexpr Point transform(const Point& local, const Point& origin, const Orientation& o)
{
    return origin + orient(local, o);
}

constexpr PointF transform(const PointF& local, const Point& origin, const Orientation& o)
{
    const PointF p = orient(local, o);
    return {origin.x + p.x, origin.y + p.y};
}

// Rotates `p` by 90 degrees clockwise about `center`.
constexpr Point rotate_cw(const Point& p, const Point& center)
{
    const Point d = p - center;
    return center + Point{-d.y, d.x};
}

// Mirrors `p` about the vertical line x = axis_x.
constexpr Point mirror_x(const Point& p, int axis_x)
{
    return {2 * axis_x - p.x, p.y};
}

// True if `p` lies on the axis-parallel segment a-b (end points included).
constexpr bool on_segment(const Point& p, const Point& a, const Point& b)
{
    if (a.x == b.x) {
        const int lo = a.y < b.y ? a.y : b.y;
        const int hi = a.y < b.y ? b.y : a.y;
        return p.x == a.x && p.y >= lo && p.y <= hi;
    }
    if (a.y == b.y) {
        const int lo = a.x < b.x ? a.x : b.x;
        const int hi = a.x < b.x ? b.x : a.x;
        return p.y == a.y && p.x >= lo && p.x <= hi;
    }
    return p == a || p == b;
}

} // namespace simnodus::model
