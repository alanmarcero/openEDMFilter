#include "Effects.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <ranges>

namespace openedm::core {

namespace {

constexpr float kMaxDriveGain = 15.0f;
constexpr double kLogTwo = 0.6931471805599453;

// Niemitalo 8-pole halfband Hilbert allpass coefficients (squared pole radii).
constexpr std::array<float, 4> kHilbertRealCoeffs{
    0.16175849836770f, 0.73302893234149f, 0.94534970032911f, 0.99059915668453f
};
constexpr std::array<float, 4> kHilbertImagCoeffs{
    0.47940086558884f, 0.87621849353931f, 0.97659758950819f, 0.99749925593555f
};

double tanhAntiderivative(double x) noexcept {
    const double mag = std::abs(x);
    return mag + std::log1p(std::exp(-2.0 * mag)) - kLogTwo;
}

float processLr4Lowpass(float input, std::array<SvfState, 2>& stages, const SvfCoefficients& coeffs) noexcept {
    const float first = stages[0].process(input, coeffs).low;
    return stages[1].process(first, coeffs).low;
}

float processLowpass8(float input, std::array<SvfState, 4>& stages, const SvfCoefficients& coeffs) noexcept {
    float signal = input;
    std::ranges::for_each(stages, [&](SvfState& stage) { signal = stage.process(signal, coeffs).low; });
    return signal;
}

} // namespace

float saturate(float input, float drive) noexcept {
    if (drive <= 1.0e-5f) return input;
    const float gain = 1.0f + kMaxDriveGain * drive;
    const float asym = 0.04f * drive * input * std::abs(input);
    const float saturated = std::tanh(gain * (input + asym)) / std::sqrt(gain);
    return input + drive * (saturated - input);
}

void AdaaSaturator::reset() noexcept {
    prevDriven_ = {};
    prevAntideriv_ = {};
    primed_ = {};
}

float AdaaSaturator::process(int channel, float input, float drive) noexcept {
    const auto ch = static_cast<size_t>(channel);
    if (drive <= 1.0e-5f) {
        prevDriven_[ch] = static_cast<double>(input);
        prevAntideriv_[ch] = tanhAntiderivative(prevDriven_[ch]);
        primed_[ch] = true;
        return input;
    }

    const double gain = 1.0 + static_cast<double>(kMaxDriveGain * drive);
    const double x = static_cast<double>(input);
    const double asym = 0.04 * static_cast<double>(drive) * x * std::abs(x);
    const double driven = gain * (x + asym);
    const double currentAntideriv = tanhAntiderivative(driven);

    if (!primed_[ch]) {
        primed_[ch] = true;
        prevDriven_[ch] = driven;
        prevAntideriv_[ch] = currentAntideriv;
        const float saturated = static_cast<float>(std::tanh(driven) / std::sqrt(gain));
        return input + drive * (saturated - input);
    }

    const double delta = driven - prevDriven_[ch];
    const double midpoint = 0.5 * (driven + prevDriven_[ch]);
    const double diff = currentAntideriv - prevAntideriv_[ch];
    prevDriven_[ch] = driven;
    prevAntideriv_[ch] = currentAntideriv;

    const double adaaTanh = std::abs(delta) < 1.0e-5 ? std::tanh(midpoint) : std::clamp(diff / delta, -1.0, 1.0);
    const float saturated = static_cast<float>(adaaTanh / std::sqrt(gain));
    return input + drive * (saturated - input);
}

void BitCrusher::reset() noexcept {
    phases_ = {};
    held_ = {};
    preFilters_ = {};
}

float BitCrusher::process(int channel, float input, float rateHz, int bits, double sampleRate) noexcept {
    const auto ch = static_cast<size_t>(channel);
    float filtered = input;
    if (static_cast<double>(rateHz) < sampleRate * 0.98) {
        const double cutoffHz = std::clamp(0.45 * static_cast<double>(rateHz), 20.0, sampleRate * 0.46);
        const float g = static_cast<float>(std::tan(std::numbers::pi * cutoffHz / sampleRate));
        const SvfCoefficients lpCoeffs = makeSvfCoefficientsDamped(g, std::numbers::sqrt2_v<float>);
        filtered = processLr4Lowpass(input, preFilters_[ch], lpCoeffs);
    }

    const float advanced = phases_[ch] + static_cast<float>(rateHz / sampleRate);
    phases_[ch] = advanced - std::floor(advanced);
    if (advanced >= 1.0f) {
        const float steps = std::exp2(static_cast<float>(bits - 1));
        held_[ch] = std::round(filtered * steps) / steps;
    }
    return held_[ch];
}

float HilbertRingModulator::Allpass2::process(float x, float coeff) noexcept {
    const float y = coeff * (x + y2) - x2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    return y;
}

void HilbertRingModulator::reset() noexcept {
    branchReal_ = {};
    branchImag_ = {};
    imagDelay_ = {};
    preNyquistLp_ = {};
    usbLpReal_ = {};
    usbLpImag_ = {};
}

float HilbertRingModulator::process(int channel, float input, double carrierPhase, float ringFrequencyHz,
                                    double oversampledRate, double hostRate, RingMode mode) noexcept {
    const auto ch = static_cast<size_t>(channel);
    const double nyquistLimit = std::min(0.45 * hostRate, 0.45 * oversampledRate);
    const double preCutoff = std::max(40.0, nyquistLimit - static_cast<double>(std::abs(ringFrequencyHz)));
    const float preG = static_cast<float>(std::tan(std::numbers::pi * preCutoff / oversampledRate));
    const SvfCoefficients preCoeffs = makeSvfCoefficientsDamped(preG, std::numbers::sqrt2_v<float>);
    const float bandLimited = processLowpass8(input, preNyquistLp_[ch], preCoeffs);

    float realSig = bandLimited;
    float imagPreDelay = bandLimited;
    std::ranges::for_each(std::views::iota(size_t{ 0 }, size_t{ 4 }), [&](size_t stage) {
        realSig = branchReal_[ch][stage].process(realSig, kHilbertRealCoeffs[stage]);
        imagPreDelay = branchImag_[ch][stage].process(imagPreDelay, kHilbertImagCoeffs[stage]);
    });
    const float imagSig = imagDelay_[ch];
    imagDelay_[ch] = imagPreDelay;

    const double usbCutoff = std::clamp(nyquistLimit - static_cast<double>(std::abs(ringFrequencyHz)),
                                        40.0, 0.46 * oversampledRate);
    const float usbG = static_cast<float>(std::tan(std::numbers::pi * usbCutoff / oversampledRate));
    const SvfCoefficients usbCoeffs = makeSvfCoefficientsDamped(usbG, std::numbers::sqrt2_v<float>);
    const float realUsb = processLr4Lowpass(realSig, usbLpReal_[ch], usbCoeffs);
    const float imagUsb = processLr4Lowpass(imagSig, usbLpImag_[ch], usbCoeffs);

    const double angle = 2.0 * std::numbers::pi * carrierPhase;
    const auto cosCarrier = static_cast<float>(std::cos(angle));
    const auto sinCarrier = static_cast<float>(std::sin(angle));

    const float upperSideband = realUsb * cosCarrier - imagUsb * sinCarrier;
    if (mode == RingMode::SingleSideband)
        return upperSideband;

    const float lowerSideband = realSig * cosCarrier + imagSig * sinCarrier;
    return 0.5f * (upperSideband + lowerSideband);
}

void BeatStutter::prepare(double sampleRate) {
    std::ranges::for_each(history_, [&](auto& channel) {
        channel.assign(static_cast<size_t>(sampleRate * 2.0) + 1, 0.0f);
    });
    reset();
}

void BeatStutter::reset() noexcept {
    position_ = {};
    available_ = {};
}

float BeatStutter::process(int channel, float input, double beatPhase, double bpm, double sampleRate) noexcept {
    auto& history = history_[static_cast<size_t>(channel)];
    const int capacity = static_cast<int>(history.size());
    const double beatSamples = sampleRate * 60.0 / std::max(1.0, bpm);
    double subdivisions = 4.0;
    while (3.0 * beatSamples / subdivisions >= capacity) subdivisions *= 2.0;
    const double slices = (beatPhase - std::floor(beatPhase)) * subdivisions;
    const int slice = static_cast<int>(slices) % 4;
    const double sliceSamples = beatSamples / subdivisions;
    const int delay = std::clamp(static_cast<int>(std::round(slice * sliceSamples)), 0, capacity - 1);
    const int position = position_[static_cast<size_t>(channel)];
    history[static_cast<size_t>(position)] = input;
    float output = input;
    if (delay > 0 && available_[static_cast<size_t>(channel)] >= delay) {
        const float repeated = history[static_cast<size_t>((position + capacity - delay) % capacity)];
        const double fraction = slices - std::floor(slices);
        const float fade = static_cast<float>(std::min(1.0, std::min(fraction, 1.0 - fraction) * sliceSamples / (0.002 * sampleRate)));
        output += fade * (repeated - input);
    }
    position_[static_cast<size_t>(channel)] = (position + 1) % capacity;
    available_[static_cast<size_t>(channel)] = std::min(available_[static_cast<size_t>(channel)] + 1, capacity - 1);
    return output;
}

} // namespace openedm::core
