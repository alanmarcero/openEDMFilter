#include "Oversampler.h"
#include <cmath>
#include <numbers>
#include <numeric>

namespace openedm::core {

namespace {

constexpr double kKaiserBeta = 8.0;
// The centre tap reaches the odd phase as a plain delay of this many base-rate samples.
constexpr int kCentreDelay = Oversampler::kBranchTaps / 2 - 1;

double besselI0(double x) noexcept {
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k < 32; ++k) {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
    }
    return sum;
}

} // namespace

Oversampler::Oversampler() noexcept {
    // Branch tap i sits at offset 2i + 1 - kBranchTaps from the centre of a (2 * kBranchTaps - 1)-tap halfband.
    const double halfLength = kBranchTaps - 1.0;
    for (int i = 0; i < kBranchTaps; ++i) {
        const double offset = 2.0 * i + 1.0 - kBranchTaps;
        const double sinc = std::sin(std::numbers::pi * offset / 2.0) / (std::numbers::pi * offset);
        const double ratio = offset / halfLength;
        const double window = besselI0(kKaiserBeta * std::sqrt(1.0 - ratio * ratio)) / besselI0(kKaiserBeta);
        taps_[static_cast<size_t>(i)] = static_cast<float>(sinc * window);
    }
    // The odd-offset taps must sum to 0.5 so that, with the 0.5 centre tap, DC gain is exactly 1.
    const float sum = std::accumulate(taps_.begin(), taps_.end(), 0.0f);
    std::ranges::transform(taps_, taps_.begin(), [sum](float tap) { return tap * 0.5f / sum; });
}

void Oversampler::reset() noexcept {
    upHistory_ = {};
    downHistory_ = {};
    downCentre_ = {};
}

void Oversampler::History::push(float sample) noexcept {
    head = (head + kBranchTaps - 1) % kBranchTaps;
    samples[static_cast<size_t>(head)] = sample;
    samples[static_cast<size_t>(head + kBranchTaps)] = sample;
}

float Oversampler::History::at(int age) const noexcept {
    return samples[static_cast<size_t>(head + age)];
}

float Oversampler::convolve(const History& history) const noexcept {
    const auto newestFirst = history.samples.begin() + history.head;
    return std::inner_product(taps_.begin(), taps_.end(), newestFirst, 0.0f);
}

Oversampler::Frame Oversampler::upsample(int channel, float input) noexcept {
    History& history = upHistory_[static_cast<size_t>(channel)];
    history.push(input);
    // Zero stuffing halves the level, so both phases carry a gain of 2.
    return { 2.0f * convolve(history), history.at(kCentreDelay) };
}

float Oversampler::downsample(int channel, const Frame& input) noexcept {
    History& history = downHistory_[static_cast<size_t>(channel)];
    History& centre = downCentre_[static_cast<size_t>(channel)];
    history.push(input[0]);
    centre.push(input[1]);
    return convolve(history) + 0.5f * centre.at(kCentreDelay + 1);
}

} // namespace openedm::core
