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

// Amplitude of one frequency in the second half of the signal (a single DFT bin).
inline double toneLevel(const std::vector<float>& samples, double frequencyHz) {
    const size_t start = samples.size() / 2;
    double real = 0.0;
    double imaginary = 0.0;
    for (size_t i = start; i < samples.size(); ++i) {
        const double angle = 2.0 * std::numbers::pi * frequencyHz * static_cast<double>(i) / kSampleRate;
        real += samples[i] * std::cos(angle);
        imaginary += samples[i] * std::sin(angle);
    }
    return 2.0 * std::hypot(real, imaginary) / static_cast<double>(samples.size() - start);
}

} // namespace openedm::test
