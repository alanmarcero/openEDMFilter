#include "MultiModeFilter.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <numeric>
#include <ranges>

namespace openedm::core {

namespace {

constexpr float kNarrowQ = 12.0f;
constexpr float kWideQ = 0.6f;
constexpr double kMinCutoffHz = 10.0;
constexpr double kMaxCutoffRatio = 0.45;
constexpr double kMaxFmCutoffRatio = 0.465;
constexpr double kBypassFadeSeconds = 0.01;
constexpr double kCoeffSmoothSeconds = 0.002;
constexpr float kFeedbackSatScale = 5.5f;
constexpr float kStageSatScale = 5.0f;
constexpr float kThirdStageSatScale = 3.0f;

// Exact -3 dB cascade bandwidth factors for 1, 2, 4, and 8 Butterworth-staggered biquad stages.
constexpr std::array<float, 4> kBandpassSlopeScale{ 1.0f, 0.47407264f, 0.2338611f, 0.1168931f };

struct StageDesign {
    int count;
    std::array<float, MultiModeFilter::kMaxStages> qs;
    float ladderK;
    std::array<float, 4> mix; // input, low, normalized band, high
};

float ladderFeedbackFor(const FilterShape& shape) noexcept {
    const bool usesLadder = shape.type == FilterType::LowPass
        || shape.type == FilterType::HighPass
        || shape.type == FilterType::BandPass;
    if (!usesLadder || shape.resonance <= 1.0e-6f)
        return 0.0f;
    const float r = std::clamp(shape.resonance, 0.0f, 1.0f);
    const float widthTrim = 1.0f - 0.03f * std::clamp(shape.width, 0.0f, 1.0f);
    return r * (1.75f + 2.45f * r * r * r) * widthTrim;
}

StageDesign designStages(const FilterShape& shape) noexcept {
    const int slopeIndex = std::clamp(static_cast<int>(shape.slope), 0, 3);
    const int count = 1 << slopeIndex;
    const float widthQ = kNarrowQ + (kWideQ - kNarrowQ) * shape.width;
    const float bpScale = kBandpassSlopeScale[static_cast<size_t>(slopeIndex)];
    const bool isLpHp = shape.type == FilterType::LowPass || shape.type == FilterType::HighPass;
    const bool shelf = shape.type == FilterType::LowShelf || shape.type == FilterType::HighShelf;

    StageDesign design{ count, {}, ladderFeedbackFor(shape), {} };
    std::ranges::for_each(std::views::iota(0, count), [&](int stage) {
        const double angle = (2.0 * stage + 1.0) * std::numbers::pi / (4.0 * count);
        const float sinAngle = static_cast<float>(std::sin(angle));
        const float butterworthQ = 0.5f / sinAngle;
        float q = widthQ;
        if (isLpHp)
            q = butterworthQ;
        if (shape.type == FilterType::BandPass)
            q = widthQ * bpScale * std::numbers::sqrt2_v<float> * butterworthQ;
        if (shape.type == FilterType::Notch)
            q = (widthQ * std::numbers::sqrt2_v<float> * sinAngle / bpScale) * (1.0f + 1.5f * shape.resonance);
        if (shelf)
            q = std::lerp(0.8f, 0.52f, shape.width) * std::pow(std::numbers::sqrt2_v<float> * butterworthQ, 0.35f);
        if (shape.type == FilterType::AllPass)
            q = widthQ * std::pow(std::numbers::sqrt2_v<float> * butterworthQ, 0.35f) * (0.5f + shape.resonance);
        if (shape.type == FilterType::BellCut)
            q = widthQ * std::pow(std::numbers::sqrt2_v<float> * butterworthQ, 0.35f);
        design.qs[static_cast<size_t>(stage)] = std::clamp(q, 0.1f, 60.0f);
    });

    switch (shape.type) {
        case FilterType::LowPass:  design.mix = { 0, 1, 0, 0 }; break;
        case FilterType::HighPass: design.mix = { 0, 0, 0, 1 }; break;
        case FilterType::BandPass: design.mix = { 0, 0, 1, 0 }; break;
        case FilterType::Notch:    design.mix = { 1, 0, -1, 0 }; break;
        case FilterType::AllPass:  design.mix = { 1, 0, -2, 0 }; break;
        case FilterType::LowShelf:
        case FilterType::HighShelf: {
            const float gain = std::pow(10.0f, (3.0f + 9.0f * shape.resonance) / (20.0f * count));
            design.mix = { 0, shape.type == FilterType::LowShelf ? gain : 1.0f,
                           std::sqrt(gain), shape.type == FilterType::HighShelf ? gain : 1.0f };
            break;
        }
        case FilterType::BellCut: {
            const float gain = std::pow(10.0f, (-3.0f - 15.0f * shape.resonance) / (20.0f * count));
            design.mix = { 1, 0, gain - 1.0f, 0 };
            break;
        }
    }
    return design;
}

} // namespace

float MultiModeFilter::LadderState::process(float input, float gOnePole, float feedbackK) noexcept {
    const float stateEnergy = std::abs(s[0]) + std::abs(s[1]) + std::abs(s[2]) + std::abs(s[3]);
    if (feedbackK <= 1.0e-5f && stateEnergy < 1.0e-7f)
        return input;

    const float g = std::clamp(gOnePole, 1.0e-5f, 0.98f);
    const float oneMinusG = 1.0f - g;
    const float g2 = g * g;
    const float g3 = g2 * g;
    const float g4 = g2 * g2;

    // Tiny bias kickstarts self-oscillation above k = 4 when input is digital zero.
    const float thermalSeed = feedbackK > 4.0f ? 1.0e-6f : 0.0f;
    const float historySum = oneMinusG * (g3 * s[0] + g2 * s[1] + g * s[2] + s[3]);
    float u0 = (input + thermalSeed - feedbackK * historySum) / (1.0f + feedbackK * g4);

    float y1 = 0.0f;
    std::array<float, 4> y{};
    // Mystran predictor + 4 Newton-Raphson iterations solve zero-delay nonlinear global feedback.
    std::ranges::for_each(std::views::iota(0, 4), [&](int) {
        const float t0 = std::tanh(u0 / kStageSatScale);
        y[0] = g * (kStageSatScale * t0 - s[0]) + s[0];
        const float t1 = std::tanh(y[0] / kStageSatScale);
        y[1] = g * (kStageSatScale * t1 - s[1]) + s[1];
        const float t2 = std::tanh(y[1] / kThirdStageSatScale);
        y[2] = g * (kThirdStageSatScale * t2 - s[2]) + s[2];
        const float t3 = std::tanh(y[2] / kStageSatScale);
        y[3] = g * (kStageSatScale * t3 - s[3]) + s[3];

        const float fbArg = input + thermalSeed - feedbackK * y[3];
        const float tFb = std::tanh(fbArg / kFeedbackSatScale);
        const float residual = u0 - kFeedbackSatScale * tFb;
        const float dy3du0 = g4 * (1.0f - t0 * t0) * (1.0f - t1 * t1) * (1.0f - t2 * t2) * (1.0f - t3 * t3);
        const float jacobian = 1.0f + feedbackK * (1.0f - tFb * tFb) * dy3du0;
        u0 -= residual / jacobian;
    });

    const float t0 = std::tanh(u0 / kStageSatScale);
    y[0] = g * (kStageSatScale * t0 - s[0]) + s[0];
    const float t1 = std::tanh(y[0] / kStageSatScale);
    y1 = g * (kStageSatScale * t1 - s[1]) + s[1];
    const float t2 = std::tanh(y1 / kThirdStageSatScale);
    y[2] = g * (kThirdStageSatScale * t2 - s[2]) + s[2];
    const float t3 = std::tanh(y[2] / kStageSatScale);
    y[3] = g * (kStageSatScale * t3 - s[3]) + s[3];

    s[0] = 2.0f * y[0] - s[0];
    s[1] = 2.0f * y1 - s[1];
    s[2] = 2.0f * y[2] - s[2];
    s[3] = 2.0f * y[3] - s[3];

    return u0 + feedbackK * y1;
}

void MultiModeFilter::reset() noexcept {
    stages_ = {};
    ladders_ = {};
    hasSmoothed_ = false;
    hasCached_ = false;
}

void MultiModeFilter::update(const FilterShape& shape, double cutoffHz, double sampleRate, float fmOctaves) noexcept {
    const float targetMix = shape.enabled ? 1.0f : 0.0f;
    const auto mixStep = static_cast<float>(1.0 / (kBypassFadeSeconds * sampleRate));
    if (activeMix_ < 0.0f) {
        activeMix_ = targetMix;
    }
    if (activeMix_ <= 0.0f && shape.enabled) {
        reset();
    }
    activeMix_ = std::clamp(targetMix, activeMix_ - mixStep, activeMix_ + mixStep);
    if (activeMix_ <= 0.0f) {
        return;
    }

    const double safeCutoff = std::clamp(cutoffHz, kMinCutoffHz, sampleRate * kMaxCutoffRatio);
    const double targetLogCutoff = std::log2(safeCutoff);
    const bool shapeSame = hasCached_
        && cachedShape_.type == shape.type
        && cachedShape_.slope == shape.slope
        && std::abs(cachedShape_.resonance - shape.resonance) < 1.0e-6f
        && std::abs(cachedShape_.width - shape.width) < 1.0e-6f;

    if (!shapeSame) {
        const int previousStages = hasCached_ ? numStages_ : 0;
        const StageDesign design = designStages(shape);
        if (design.count > numStages_) {
            std::ranges::for_each(stages_, [&](auto& channelStages) {
                std::fill(channelStages.begin() + numStages_, channelStages.end(), SvfState{});
            });
        }
        stageMix_ = design.mix;
        numStages_ = design.count;
        targetLadderK_ = design.ladderK;
        std::ranges::for_each(std::views::iota(0, numStages_), [&](int stage) {
            const auto idx = static_cast<size_t>(stage);
            targetKs_[idx] = 1.0f / design.qs[idx];
            if (!hasSmoothed_ || stage >= previousStages)
                smoothedKs_[idx] = targetKs_[idx];
        });
        cachedShape_ = shape;
        hasCached_ = true;
    }

    if (!hasSmoothed_) {
        smoothedLogCutoff_ = targetLogCutoff;
        smoothedLadderK_ = targetLadderK_;
        hasSmoothed_ = true;
    }

    const double smoothCoeff = std::exp(-1.0 / (kCoeffSmoothSeconds * sampleRate));
    smoothedLogCutoff_ = targetLogCutoff + smoothCoeff * (smoothedLogCutoff_ - targetLogCutoff);
    const auto smoothCoeffF = static_cast<float>(smoothCoeff);
    smoothedLadderK_ = targetLadderK_ + smoothCoeffF * (smoothedLadderK_ - targetLadderK_);

    const double fmCutoff = std::clamp(std::exp2(smoothedLogCutoff_ + static_cast<double>(fmOctaves)),
                                       kMinCutoffHz, sampleRate * kMaxFmCutoffRatio);
    const float g = static_cast<float>(std::tan(std::numbers::pi * fmCutoff / sampleRate));
    ladderG_ = g / (1.0f + g);

    std::ranges::for_each(std::views::iota(0, numStages_), [&](int stage) {
        const auto idx = static_cast<size_t>(stage);
        smoothedKs_[idx] = targetKs_[idx] + smoothCoeffF * (smoothedKs_[idx] - targetKs_[idx]);
        stageCoefficients_[idx] = makeSvfCoefficientsDamped(g, smoothedKs_[idx]);
    });
}

float MultiModeFilter::process(int channel, float input) noexcept {
    if (activeMix_ <= 0.0f) {
        return input;
    }
    const auto ch = static_cast<size_t>(channel);
    const float resonantInput = ladders_[ch].process(input, ladderG_, smoothedLadderK_);
    auto& channelStages = stages_[ch];
    const auto stageIndices = std::views::iota(0, numStages_);

    const float wet = std::accumulate(stageIndices.begin(), stageIndices.end(), resonantInput,
        [&](float signal, int stage) {
            const SvfCoefficients& c = stageCoefficients_[static_cast<size_t>(stage)];
            const SvfOutput out = channelStages[static_cast<size_t>(stage)].process(signal, c);
            return stageMix_[0] * signal + stageMix_[1] * out.low
                + stageMix_[2] * (c.k * out.band) + stageMix_[3] * out.high;
        });

    return input + activeMix_ * (wet - input);
}

double MultiModeFilter::magnitudeAt(const FilterShape& shape, double cutoffHz, double frequencyHz,
                                    double sampleRate) noexcept {
    return std::abs(responseAt(shape, cutoffHz, frequencyHz, sampleRate));
}

std::complex<double> MultiModeFilter::responseAt(const FilterShape& shape, double cutoffHz, double frequencyHz,
                                                double sampleRate) noexcept {
    if (!shape.enabled) return 1.0;
    using Complex = std::complex<double>;
    const StageDesign design = designStages(shape);
    const double safeCutoff = std::clamp(cutoffHz, kMinCutoffHz, sampleRate * kMaxCutoffRatio);
    const double warped = std::tan(std::numbers::pi * frequencyHz / sampleRate) / std::tan(std::numbers::pi * safeCutoff / sampleRate);
    const Complex s(0.0, warped);

    const Complex cascade = std::accumulate(design.qs.begin(), design.qs.begin() + design.count, Complex(1.0),
        [&](Complex response, float q) {
            const Complex denominator = s * s + s / static_cast<double>(q) + 1.0;
            const auto& m = design.mix;
            const auto numerator = double(m[1]) + double(m[2]) * s / static_cast<double>(q) + double(m[3]) * s * s;
            return response * (double(m[0]) + numerator / denominator);
        });

    const double k = std::min(3.85, static_cast<double>(design.ladderK));
    if (k <= 0.0)
        return cascade;
    const Complex onePlusS = 1.0 + s;
    const Complex sq = onePlusS * onePlusS;
    const Complex quad = sq * sq;
    return cascade * ((quad + k * sq) / (quad + k));
}

} // namespace openedm::core
