#pragma once

#include "Svf.h"

namespace openedm::core {

enum class LfoShape {
    Sine,
    Triangle,
    SawUp,
    SawDown,
    Square
};

// Bipolar (-1..1) tempo-synced LFO.
class Lfo {
public:
    // Locks the phase to the host transport so each cycle starts on the beat grid.
    void syncToTransport(double ppqPosition, double beatsPerCycle) noexcept;

    // Returns the value at the current phase, then advances by phaseIncrement (cycles per sample).
    [[nodiscard]] float next(LfoShape shape, double phaseIncrement) noexcept;

    [[nodiscard]] static float valueAt(LfoShape shape, double phase) noexcept;

private:
    double phase_ = 0.0;
};

// Follows the level of the input around a source frequency. Output is 0..1.
class EnvelopeFollower {
public:
    void reset() noexcept;
    void configure(float sourceHz, float attackMs, float releaseMs, double sampleRate) noexcept;
    [[nodiscard]] float process(float input) noexcept;

private:
    SvfState detector_{};
    SvfCoefficients detectorCoefficients_{};
    float attackCoefficient_ = 0.0f;
    float releaseCoefficient_ = 0.0f;
    float level_ = 0.0f;
};

} // namespace openedm::core
