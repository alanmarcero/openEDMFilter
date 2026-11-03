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

enum class SweepMode {
    BarWindow, // sweeps Start -> End across the full N-bar window, then loops
    EveryBar,  // repeats the Start -> End slope on every bar within the N-bar window
    UpDown,    // sweeps Start -> End over the first half of N bars, then End -> Start
    BarMacro   // repeats a bar-rate slope that builds progressively across the N bars
};

enum class SweepTarget {
    BandA,
    BandB,
    Both,
    Mirror // Band A follows the sweep; Band B follows the inverted (End -> Start) sweep
};

struct SweepEnvelope {
    float start = 0.0f;
    float end = 0.0f;
    float slope = 0.35f;
    float onset = 0.75f;
};

struct AutoSweepSettings {
    bool enabled = false;
    SweepTarget target = SweepTarget::BandA;
    int bars = 8; // 4, 8, 16, 32, 64
    SweepMode mode = SweepMode::BarWindow;
    float cutoffStartHz = 80.0f;
    float cutoffEndHz = 16000.0f;
    float cutoffSlope = 0.0f;     // -1..1 curvature of the cutoff trajectory
    float resonanceStart = 0.15f; // 0..1
    float resonanceEnd = 0.65f;   // 0..1
    float resonanceSlope = 0.0f;  // -1..1 curvature of the resonance trajectory
    SweepEnvelope crush{};
    SweepEnvelope glitch{};
};

struct SweepValue {
    float cutoffHz = 1000.0f;
    float resonance = 0.0f;
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

// Multi-bar tempo/time-signature-synced cutoff and resonance slope generator.
class AutoSweep {
public:
    void reset() noexcept { phase_ = 0.0; }

    // Locks the multi-bar loop phase to the host transport using the active time signature's beats per bar.
    void syncToTransport(double ppqPosition, double beatsPerBar, int bars) noexcept;

    // Returns the current normalized loop phase (0..1), then advances by phaseIncrement (cycles per sample).
    [[nodiscard]] double nextPhase(double phaseIncrement) noexcept;

    [[nodiscard]] double phase() const noexcept { return phase_; }

    [[nodiscard]] static bool appliesToBand(const AutoSweepSettings& settings, int band) noexcept;
    [[nodiscard]] static float progressAt(SweepMode mode, int bars, double phase) noexcept;
    [[nodiscard]] static float applySlope(float progress, float slope) noexcept;
    [[nodiscard]] static float slopeFromMidpoint(float midpointFraction) noexcept;
    [[nodiscard]] static float envelopeAt(const SweepEnvelope& envelope, float progress) noexcept;
    [[nodiscard]] static SweepValue valueAt(const AutoSweepSettings& settings, int band, double phase) noexcept;

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
