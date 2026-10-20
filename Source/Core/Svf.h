#pragma once

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

// Trapezoidal (TPT) state variable filter. It stays stable when the cutoff changes every sample.
// g is the prewarped cutoff, tan(pi * cutoff / sampleRate).
inline SvfCoefficients makeSvfCoefficients(float g, float q) noexcept {
    const float k = 1.0f / q;
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    return { k, a1, a2, g * a2 };
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
