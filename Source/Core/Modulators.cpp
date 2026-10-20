#include "Modulators.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace openedm::core {

namespace {

constexpr float kDetectorQ = 0.7f;
constexpr float kEnvelopeGain = 2.0f;

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
