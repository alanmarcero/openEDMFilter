#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <vector>

namespace openedm::test {

inline constexpr double kSampleRate = 48000.0;

inline std::vector<float> sine(double frequencyHz, int numSamples, float amplitude = 0.5f) {
    std::vector<float> samples(static_cast<size_t>(numSamples));
    std::ranges::generate(samples, [&, index = 0]() mutable {
        return amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * frequencyHz * index++ / kSampleRate));
    });
    return samples;
}

// RMS of the second half, after the filter has settled.
inline float settledRms(const std::vector<float>& samples) {
    const auto settled = samples.begin() + static_cast<std::ptrdiff_t>(samples.size() / 2);
    const float sumOfSquares = std::accumulate(settled, samples.end(), 0.0f,
        [](float sum, float sample) { return sum + sample * sample; });
    return std::sqrt(sumOfSquares / static_cast<float>(std::distance(settled, samples.end())));
}

inline float peak(const std::vector<float>& samples) {
    return std::accumulate(samples.begin(), samples.end(), 0.0f,
        [](float loudest, float sample) { return std::max(loudest, std::abs(sample)); });
}

} // namespace openedm::test
