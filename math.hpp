#pragma once

#include <cmath>
#include <numbers>

template <typename T>
struct vec2
{
    T x{};
    T y{};
};

using vec2i = vec2<int>;
using vec2f = vec2<float>;

constexpr vec2f operator+(const vec2f& v0, const vec2f& v1) { return {.x = v0.x + v1.x, .y = v0.y + v1.y}; }
constexpr vec2f operator-(const vec2f& v0, const vec2f& v1) { return {.x = v0.x - v1.x, .y = v0.y - v1.y}; }
constexpr vec2f operator*(const vec2f& v, const float x) { return {.x = v.x * x, .y = v.y * x}; }

constexpr vec2f rotate(const vec2f& v, const float radians)
{
    const auto c = std::cos(radians);
    const auto s = std::sin(radians);
    return {.x = v.x * c - v.y * s, .y = v.x * s + v.y * c};
}

constexpr vec2i to_vec2i(const vec2f& v) { return {.x = static_cast<int>(v.x), .y = static_cast<int>(v.y)}; }

constexpr auto pi = std::numbers::pi_v<float>;
constexpr auto to_radians(const vec2f& dir) { return pi + std::atan2(dir.y, dir.x); }
