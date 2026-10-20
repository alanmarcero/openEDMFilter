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

// Stereo-linked lookahead peak limiter. Peaks never exceed the ceiling, and each gain drop ramps in over
// the lookahead window instead of jumping, so limiting does not clip the waveform.
class PeakLimiter {
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kLookahead = 64;
    static constexpr int kLatency = kLookahead - 1;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(float* const* channels, int numChannels, int numSamples, float ceiling) noexcept;

private:
    float releaseCoefficient_ = 0.0f;
    float releasedGain_ = 1.0f;
    int position_ = 0;
    std::array<float, kLookahead> requiredGains_{};
    std::array<float, kLookahead> releasedGains_{};
    std::array<std::array<float, kLookahead>, kMaxChannels> delayLines_{};
};

} // namespace openedm::core
