#pragma once
#include <algorithm>
#include <cmath>

namespace MathUtils
{
    // Linear Interpolation
    template <typename T>
    constexpr T Lerp(T a, T b, float t)
    {
        if (t < 0.0f) return a;
        if (t > 1.0f) return b;
        return a + (b - a) * t;
    }

    // Fraction of the remaining distance to cover this frame when easing toward a target
    // at `rate` per second. Gives the same curve at any frame rate, and never exceeds 1,
    // unlike the first-order form `rate * dt`.
    [[nodiscard]] inline float ExpDecayFactor(float rate, float dt)
    {
        return 1.0f - std::exp(-rate * dt);
    }
}