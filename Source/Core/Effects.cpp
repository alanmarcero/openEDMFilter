#include "Effects.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <ranges>

namespace openedm::core {

namespace {

constexpr float kMaxDriveGain = 15.0f;
constexpr double kLimiterReleaseMs = 80.0;

} // namespace

float saturate(float input, float drive) noexcept {
    const float gain = 1.0f + kMaxDriveGain * drive;
    const float saturated = std::tanh(gain * input) / std::sqrt(gain);
    return input + drive * (saturated - input);
}

void BitCrusher::reset() noexcept {
    phases_ = {};
    held_ = {};
}

float BitCrusher::process(int channel, float input, float rateHz, int bits, double sampleRate) noexcept {
    const float advanced = phases_[channel] + static_cast<float>(rateHz / sampleRate);
    phases_[channel] = advanced - std::floor(advanced);
    if (advanced >= 1.0f) {
        const float steps = std::exp2(static_cast<float>(bits - 1));
        held_[channel] = std::round(input * steps) / steps;
    }
    return held_[channel];
}

void PeakLimiter::prepare(double sampleRate) noexcept {
    releaseCoefficient_ = static_cast<float>(std::exp(-1.0 / (kLimiterReleaseMs * 0.001 * sampleRate)));
    reset();
}

void PeakLimiter::reset() noexcept {
    gain_ = 1.0f;
}

void PeakLimiter::process(float* const* channels, int numChannels, int numSamples, float ceiling) noexcept {
    const auto channelIndices = std::views::iota(0, numChannels);
    std::ranges::for_each(std::views::iota(0, numSamples), [&](int sample) {
        const float peak = std::accumulate(channelIndices.begin(), channelIndices.end(), 0.0f,
            [&](float loudest, int channel) { return std::max(loudest, std::abs(channels[channel][sample])); });
        const float targetGain = peak > ceiling ? ceiling / peak : 1.0f;
        const float releasedGain = targetGain + releaseCoefficient_ * (gain_ - targetGain);
        gain_ = std::min(targetGain, releasedGain);
        std::ranges::for_each(channelIndices, [&](int channel) {
            channels[channel][sample] = std::clamp(channels[channel][sample] * gain_, -ceiling, ceiling);
        });
    });
}

} // namespace openedm::core
