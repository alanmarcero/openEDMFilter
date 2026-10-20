#pragma once

#include <array>

namespace openedm::core {

// tanh saturation blended in by drive (0..1). Drive 0 returns the input unchanged.
[[nodiscard]] float saturate(float input, float drive) noexcept;

// Sample-rate reducer and bit crusher.
class BitCrusher {
public:
    static constexpr int kMaxChannels = 2;

    void reset() noexcept;
    [[nodiscard]] float process(int channel, float input, float rateHz, int bits, double sampleRate) noexcept;

private:
    std::array<float, kMaxChannels> phases_{};
    std::array<float, kMaxChannels> held_{};
};

// Stereo-linked peak limiter with instant attack. Peaks never exceed the ceiling.
class PeakLimiter {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(float* const* channels, int numChannels, int numSamples, float ceiling) noexcept;

private:
    float releaseCoefficient_ = 0.0f;
    float gain_ = 1.0f;
};

} // namespace openedm::core
