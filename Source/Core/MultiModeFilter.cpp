#include "MultiModeFilter.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <numeric>
#include <ranges>

namespace openedm::core {

namespace {

constexpr float kMaxPeakGain = 5.0f;    // about +15.6 dB at full resonance
constexpr float kNarrowQ = 12.0f;
constexpr float kWideQ = 0.6f;
constexpr double kMaxCutoffRatio = 0.45; // of the sample rate
constexpr double kBypassFadeSeconds = 0.01;

struct StageDesign {
    int count;
    std::array<float, MultiModeFilter::kMaxStages> qs;
    float peakQ;
    float peakGain;
};

// Low-pass and high-pass stages use Butterworth Qs so the cascade stays flat below the cutoff.
// Band-pass and notch stages all use the width Q.
StageDesign designStages(const FilterShape& shape) noexcept {
    const int count = 1 << static_cast<int>(shape.slope);
    const float widthQ = kNarrowQ + (kWideQ - kNarrowQ) * shape.width;
    const bool isButterworth = shape.type == FilterType::LowPass || shape.type == FilterType::HighPass;

    StageDesign design{ count, {}, widthQ, kMaxPeakGain * shape.resonance };
    std::ranges::generate(design.qs, [&, stage = 0]() mutable {
        const double angle = (2.0 * stage++ + 1.0) * std::numbers::pi / (4.0 * count);
        const float butterworthQ = static_cast<float>(1.0 / (2.0 * std::sin(angle)));
        return isButterworth ? butterworthQ : widthQ;
    });
    return design;
}

} // namespace

void MultiModeFilter::reset() noexcept {
    stages_ = {};
    peaks_ = {};
}

void MultiModeFilter::update(const FilterShape& shape, double cutoffHz, double sampleRate) noexcept {
    const StageDesign design = designStages(shape);
    const double safeCutoff = std::clamp(cutoffHz, 10.0, sampleRate * kMaxCutoffRatio);
    const float g = static_cast<float>(std::tan(std::numbers::pi * safeCutoff / sampleRate));

    const float targetMix = shape.enabled ? 1.0f : 0.0f;
    const auto mixStep = static_cast<float>(1.0 / (kBypassFadeSeconds * sampleRate));
    if (activeMix_ < 0.0f) {
        activeMix_ = targetMix;
    }
    if (activeMix_ <= 0.0f && shape.enabled) {
        reset();
    }
    activeMix_ = std::clamp(targetMix, activeMix_ - mixStep, activeMix_ + mixStep);
    if (design.count > numStages_) {
        std::ranges::for_each(stages_, [&](auto& channelStages) {
            std::fill(channelStages.begin() + numStages_, channelStages.end(), SvfState{});
        });
    }

    type_ = shape.type;
    numStages_ = design.count;
    peakGain_ = design.peakGain;
    peakCoefficients_ = makeSvfCoefficients(g, design.peakQ);
    std::ranges::transform(design.qs.begin(), design.qs.begin() + design.count, stageCoefficients_.begin(),
        [g](float q) { return makeSvfCoefficients(g, q); });
}

float MultiModeFilter::process(int channel, float input) noexcept {
    if (activeMix_ <= 0.0f) {
        return input;
    }
    auto& channelStages = stages_[channel];
    const auto stageIndices = std::views::iota(0, numStages_);

    const float filtered = std::accumulate(stageIndices.begin(), stageIndices.end(), input,
        [&](float signal, int stage) {
            const SvfCoefficients& c = stageCoefficients_[stage];
            const SvfOutput out = channelStages[stage].process(signal, c);
            switch (type_) {
                case FilterType::LowPass: return out.low;
                case FilterType::HighPass: return out.high;
                case FilterType::BandPass: return c.k * out.band;
                case FilterType::Notch: return signal - c.k * out.band;
            }
            return signal;
        });

    const SvfOutput peak = peaks_[channel].process(filtered, peakCoefficients_);
    const float wet = filtered + peakGain_ * peakCoefficients_.k * peak.band;
    return input + activeMix_ * (wet - input);
}

double MultiModeFilter::magnitudeAt(const FilterShape& shape, double cutoffHz, double frequencyHz) noexcept {
    if (!shape.enabled) {
        return 1.0;
    }
    using Complex = std::complex<double>;
    const StageDesign design = designStages(shape);
    const Complex s(0.0, frequencyHz / cutoffHz);

    const Complex cascade = std::accumulate(design.qs.begin(), design.qs.begin() + design.count, Complex(1.0),
        [&](Complex response, float q) {
            const Complex denominator = s * s + s / static_cast<double>(q) + 1.0;
            switch (shape.type) {
                case FilterType::LowPass: return response / denominator;
                case FilterType::HighPass: return response * s * s / denominator;
                case FilterType::BandPass: return response * (s / static_cast<double>(q)) / denominator;
                case FilterType::Notch: return response * (s * s + 1.0) / denominator;
            }
            return response;
        });

    const double peakQ = design.peakQ;
    const Complex peak = 1.0 + static_cast<double>(design.peakGain) * (s / peakQ) / (s * s + s / peakQ + 1.0);
    return std::abs(cascade * peak);
}

} // namespace openedm::core
