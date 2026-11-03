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

void BeatStutter::prepare(double sampleRate) {
    for (auto& channel : history_) channel.assign(static_cast<size_t>(sampleRate * 2.0) + 1, 0.0f);
    reset();
}

void BeatStutter::reset() noexcept {
    position_ = {};
    available_ = {};
}

float BeatStutter::process(int channel, float input, double beatPhase, double bpm, double sampleRate) noexcept {
    auto& history = history_[channel];
    const int capacity = static_cast<int>(history.size());
    const double beatSamples = sampleRate * 60.0 / std::max(1.0, bpm);
    double subdivisions = 4.0;
    while (3.0 * beatSamples / subdivisions >= capacity) subdivisions *= 2.0;
    const double slices = (beatPhase - std::floor(beatPhase)) * subdivisions;
    const int slice = static_cast<int>(slices) % 4;
    const double sliceSamples = beatSamples / subdivisions;
    const int delay = std::clamp(static_cast<int>(std::round(slice * sliceSamples)), 0, capacity - 1);
    const int position = position_[channel];
    history[position] = input;
    float output = input;
    if (delay > 0 && available_[channel] >= delay) {
        const float repeated = history[(position + capacity - delay) % capacity];
        const double fraction = slices - std::floor(slices);
        const float fade = static_cast<float>(std::min(1.0, std::min(fraction, 1.0 - fraction) * sliceSamples / (0.002 * sampleRate)));
        output += fade * (repeated - input);
    }
    position_[channel] = (position + 1) % capacity;
    available_[channel] = std::min(available_[channel] + 1, capacity - 1);
    return output;
}

void PeakLimiter::prepare(double sampleRate) noexcept {
    releaseCoefficient_ = static_cast<float>(std::exp(-1.0 / (kLimiterReleaseMs * 0.001 * sampleRate)));
    reset();
}

void PeakLimiter::reset() noexcept {
    releasedGain_ = 1.0f;
    position_ = 0;
    requiredGains_.fill(1.0f);
    releasedGains_.fill(1.0f);
    delayLines_ = {};
}

void PeakLimiter::process(float* const* channels, int numChannels, int numSamples, float ceiling) noexcept {
    const auto channelIndices = std::views::iota(0, std::min(numChannels, kMaxChannels));
    std::ranges::for_each(std::views::iota(0, numSamples), [&](int sample) {
        const float peak = std::accumulate(channelIndices.begin(), channelIndices.end(), 0.0f,
            [&](float loudest, int channel) { return std::max(loudest, std::abs(channels[channel][sample])); });
        requiredGains_[position_] = peak > ceiling ? ceiling / peak : 1.0f;

        // Each averaged gain is at most the minimum of a window that holds the oldest delayed sample,
        // so the average never exceeds the gain that sample needs.
        const float heldGain = std::ranges::min(requiredGains_);
        releasedGain_ = std::min(heldGain, heldGain + releaseCoefficient_ * (releasedGain_ - heldGain));
        releasedGains_[position_] = releasedGain_;
        const float gain = std::accumulate(releasedGains_.begin(), releasedGains_.end(), 0.0f) / kLookahead;

        const int oldest = (position_ + 1) % kLookahead;
        std::ranges::for_each(channelIndices, [&](int channel) {
            auto& delayLine = delayLines_[channel];
            delayLine[position_] = channels[channel][sample];
            channels[channel][sample] = std::clamp(delayLine[oldest] * gain, -ceiling, ceiling);
        });
        position_ = oldest;
    });
}

} // namespace openedm::core
