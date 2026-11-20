#pragma once

#include <algorithm>
#include <cmath>

namespace openedm::core {

struct SvfCoefficients {
    float k = 1.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;
};

struct SvfOutput {
    float low;
    float band;
    float high;
};

// Trapezoidal (TPT) state variable filter with stability-clamped prewarped g and damping k.
inline SvfCoefficients makeSvfCoefficientsDamped(float g, float k) noexcept {
    const float safeG = std::clamp(g, 1.0e-5f, 11.43f);
    const float safeK = std::clamp(k, 0.005f, 20.0f);
    const float a1 = 1.0f / (1.0f + safeG * (safeG + safeK));
    const float a2 = safeG * a1;
    return { safeK, a1, a2, safeG * a2 };
}

inline SvfCoefficients makeSvfCoefficients(float g, float q) noexcept {
    return makeSvfCoefficientsDamped(g, 1.0f / std::max(0.05f, q));
}

struct SvfState {
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;

    SvfOutput process(float input, const SvfCoefficients& c) noexcept {
        const float v3 = input - ic2eq;
        const float v1 = c.a1 * ic1eq + c.a2 * v3;
        const float v2 = ic2eq + c.a2 * ic1eq + c.a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        return { v2, v1, input - c.k * v1 - v2 };
    }
};

} // namespace openedm::core
