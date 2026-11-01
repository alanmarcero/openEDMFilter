#include "Modulators.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace openedm::core {

namespace {

constexpr float kDetectorQ = 0.7f;
constexpr float kEnvelopeGain = 2.0f;
constexpr float kMinSweepHz = 20.0f;
constexpr float kMaxSweepHz = 20000.0f;
constexpr float kSlopeOctaveScale = 3.0f;

float smoothingCoefficient(float timeMs, double sampleRate) noexcept {
    return static_cast<float>(std::exp(-1.0 / (std::max(0.01, static_cast<double>(timeMs)) * 0.001 * sampleRate)));
}

} // namespace

void Lfo::syncToTransport(double ppqPosition, double beatsPerCycle) noexcept {
    const double cycles = ppqPosition / beatsPerCycle;
    phase_ = cycles - std::floor(cycles);
}

float Lfo::next(LfoShape shape, double phaseIncrement) noexcept {
    const float value = valueAt(shape, phase_);
    const double advanced = phase_ + phaseIncrement;
    phase_ = advanced - std::floor(advanced);
    return value;
}

float Lfo::valueAt(LfoShape shape, double phase) noexcept {
    const auto p = static_cast<float>(phase);
    switch (shape) {
        case LfoShape::Sine: return std::sin(2.0f * std::numbers::pi_v<float> * p);
        case LfoShape::Triangle: return 1.0f - 4.0f * std::abs(p - 0.5f);
        case LfoShape::SawUp: return 2.0f * p - 1.0f;
        case LfoShape::SawDown: return 1.0f - 2.0f * p;
        case LfoShape::Square: return p < 0.5f ? 1.0f : -1.0f;
    }
    return 0.0f;
}

void AutoSweep::syncToTransport(double ppqPosition, double beatsPerBar, int bars) noexcept {
    const double totalBeats = std::max(0.25, beatsPerBar * static_cast<double>(std::max(1, bars)));
    const double cycles = ppqPosition / totalBeats;
    phase_ = cycles - std::floor(cycles);
}

double AutoSweep::nextPhase(double phaseIncrement) noexcept {
    const double current = phase_;
    const double advanced = phase_ + phaseIncrement;
    phase_ = advanced - std::floor(advanced);
    return current;
}

bool AutoSweep::appliesToBand(const AutoSweepSettings& settings, int band) noexcept {
    if (!settings.enabled) {
        return false;
    }
    if (band == 0) {
        return settings.target == SweepTarget::BandA || settings.target == SweepTarget::Both
            || settings.target == SweepTarget::Mirror;
    }
    if (band == 1) {
        return settings.target == SweepTarget::BandB || settings.target == SweepTarget::Both
            || settings.target == SweepTarget::Mirror;
    }
    return false;
}

float AutoSweep::progressAt(SweepMode mode, int bars, double phase) noexcept {
    const double wrapped = phase - std::floor(phase);
    const double barCount = static_cast<double>(std::max(1, bars));
    const double barCycles = wrapped * barCount;
    const auto barPhase = static_cast<float>(barCycles - std::floor(barCycles));
    const auto windowPhase = static_cast<float>(wrapped);

    switch (mode) {
        case SweepMode::BarWindow: return windowPhase;
        case SweepMode::EveryBar: return barPhase;
        case SweepMode::UpDown: return 1.0f - std::abs(2.0f * windowPhase - 1.0f);
        case SweepMode::BarMacro: return 0.5f * (barPhase + windowPhase);
    }
    return windowPhase;
}

float AutoSweep::applySlope(float progress, float slope) noexcept {
    const float clampedProgress = std::clamp(progress, 0.0f, 1.0f);
    if (clampedProgress <= 0.0f || clampedProgress >= 1.0f) {
        return clampedProgress;
    }
    const float clampedSlope = std::clamp(slope, -1.0f, 1.0f);
    const float exponent = std::exp2(clampedSlope * kSlopeOctaveScale);
    return std::pow(clampedProgress, exponent);
}

float AutoSweep::slopeFromMidpoint(float midpointFraction) noexcept {
    const float clamped = std::clamp(midpointFraction, 0.00390625f, 0.99609375f);
    const float exponent = -std::log2(clamped);
    return std::clamp(std::log2(exponent) / kSlopeOctaveScale, -1.0f, 1.0f);
}

SweepValue AutoSweep::valueAt(const AutoSweepSettings& settings, int band, double phase) noexcept {
    float progress = progressAt(settings.mode, settings.bars, phase);
    if (band == 1 && settings.target == SweepTarget::Mirror) {
        progress = 1.0f - progress;
    }
    const float cutoffT = applySlope(progress, settings.cutoffSlope);
    const float logStart = std::log2(std::clamp(settings.cutoffStartHz, kMinSweepHz, kMaxSweepHz));
    const float logEnd = std::log2(std::clamp(settings.cutoffEndHz, kMinSweepHz, kMaxSweepHz));
    const float cutoffHz = std::clamp(std::exp2(logStart + cutoffT * (logEnd - logStart)), kMinSweepHz, kMaxSweepHz);

    const float resT = applySlope(progress, settings.resonanceSlope);
    const float resStart = std::clamp(settings.resonanceStart, 0.0f, 1.0f);
    const float resEnd = std::clamp(settings.resonanceEnd, 0.0f, 1.0f);
    const float resonance = std::clamp(resStart + resT * (resEnd - resStart), 0.0f, 1.0f);

    return { cutoffHz, resonance };
}

void EnvelopeFollower::reset() noexcept {
    detector_ = {};
    level_ = 0.0f;
}

void EnvelopeFollower::configure(float sourceHz, float attackMs, float releaseMs, double sampleRate) noexcept {
    const double safeSource = std::clamp(static_cast<double>(sourceHz), 10.0, sampleRate * 0.45);
    const float g = static_cast<float>(std::tan(std::numbers::pi * safeSource / sampleRate));
    detectorCoefficients_ = makeSvfCoefficients(g, kDetectorQ);
    attackCoefficient_ = smoothingCoefficient(attackMs, sampleRate);
    releaseCoefficient_ = smoothingCoefficient(releaseMs, sampleRate);
}

float EnvelopeFollower::process(float input) noexcept {
    const SvfOutput detected = detector_.process(input, detectorCoefficients_);
    const float rectified = std::abs(detectorCoefficients_.k * detected.band);
    const float coefficient = rectified > level_ ? attackCoefficient_ : releaseCoefficient_;
    level_ = rectified + coefficient * (level_ - rectified);
    return std::min(1.0f, level_ * kEnvelopeGain);
}

} // namespace openedm::core
