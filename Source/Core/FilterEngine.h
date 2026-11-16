#pragma once

#include "Effects.h"
#include "Modulators.h"
#include "MultiModeFilter.h"
#include "Oversampler.h"
#include <array>
#include <optional>

namespace openedm::core {

enum class Placement {
    PreFilter,
    PostFilter
};

enum class ModTarget { Both, BandA, BandB };
enum class MidiSource { Off, Velocity, Aftertouch, CC };
enum class StereoMode { Linked, LeftRight, MidSide };

struct BandSettings {
    FilterShape shape{};
    float cutoffHz = 1000.0f;
    LfoShape lfoShape = LfoShape::Sine;
    double lfoBeats = 4.0; // beats per LFO cycle (in 4/4 quarter notes; >= 4 scales with time signature bars)
    float lfoDepth = 0.0f; // -1..1
    float envSourceHz = 1000.0f;
    float envAttackMs = 5.0f;
    float envReleaseMs = 150.0f;
    float envDepth = 0.0f; // -1..1
    MidiSource midiSource = MidiSource::Off;
    int midiCC = 1;
    float midiDepth = 0.0f;
    StepSettings step{};
    float lane2Depth = 1.0f; // scales modulation on R or S; L/M uses the original depths
    float lane2Phase = 0.0f; // additional LFO and step phase in cycles
    float lane2Offset = 0.0f; // independent R/S cutoff offset in octaves
};

struct EngineSettings {
    std::array<BandSettings, 2> bands{};
    float fmRateHz = 5.0f;
    float fmDepth = 0.0f;
    float drive = 0.0f;
    Placement drivePlacement = Placement::PreFilter;
    float crushRateHz = 12000.0f;
    int crushBits = 8;
    float crushMix = 0.0f;
    Placement crushPlacement = Placement::PostFilter;
    bool crushLinked = false; // the crush rate follows band A's cutoff modulation, unless band A is bypassed
    float ringFrequencyHz = 200.0f;
    float ringMix = 0.0f;
    float mix = 1.0f;
    float outputGain = 1.0f;
    bool limiterEnabled = true;
    float ceiling = 1.0f;
    bool inertia = true; // slows cutoff moves on 48 and 96 dB slopes so they stay click-free
    AutoSweepSettings sweep{};
    bool bypassed = false; // full engine bypass, crossfaded over 10 ms at constant latency
    bool parallel = false;
    float parallelBlend = 0.5f; // A at 0, B at 1
    LfoShape fmShape = LfoShape::Sine;
    ModTarget fmTarget = ModTarget::Both;
    StereoMode stereoMode = StereoMode::Linked;
};

struct Transport {
    double bpm = 120.0;
    std::optional<double> ppqPosition; // host timeline position, including while stopped
    double beatsPerBar = 4.0;          // quarter-note beats per bar for the active time signature
    bool playing = true;               // with a playhead, false holds synced clocks on that position
};

// Full signal chain: drive/crush (pre or post), serial or parallel A/B, ring mod, dry/wet, output, limiter.
// Everything before the limiter runs at twice the host rate, so drive, ring mod and FM alias less.
class FilterEngine {
public:
    static constexpr int kNumBands = 2;
    static constexpr float kModulationOctaves = 6.0f;
    static constexpr float kFmOctaves = 3.0f;
    static constexpr int kLatencySamples = Oversampler::kLatency + PeakLimiter::kLatency;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(const EngineSettings& settings, const Transport& transport,
                 float* const* channels, int numChannels, int numSamples) noexcept;

    // Modulated cutoff and resonance of each band at the end of the last block.
    [[nodiscard]] std::array<float, kNumBands> currentCutoffs() const noexcept;
    [[nodiscard]] std::array<float, kNumBands> currentLaneCutoffs(int lane) const noexcept;
    void setMidiValue(MidiSource source, float value, int controller = 1) noexcept;
    void clearMidi() noexcept;
    [[nodiscard]] std::array<float, kNumBands> currentResonances() const noexcept { return resonances_; }
    [[nodiscard]] float currentSweepPhase() const noexcept { return static_cast<float>(sweep_.phase()); }

private:
    double sampleRate_ = 88200.0; // the oversampled rate
    // Two filters per band. A type or slope change starts the new one clean and crossfades from the old one.
    using FilterLanes = std::array<MultiModeFilter, 2>;
    std::array<std::array<FilterLanes, 2>, kNumBands> filters_{};
    std::array<FilterLanes, 2> parallelB_{};
    std::array<std::array<FilterShape, 2>, kNumBands> slotShapes_{};
    std::array<int, kNumBands> activeSlot_{};
    std::array<int, kNumBands> fadeRemaining_{};
    int fadeSamples_ = 1;
    std::array<Lfo, kNumBands> lfos_{};
    AutoSweep sweep_{};
    std::array<std::array<EnvelopeFollower, 2>, kNumBands> envelopes_{};
    std::array<float, kNumBands> logCutoffs_{}; // smoothed, in log2 Hz
    std::array<float, kNumBands> lane2LogCutoffs_{};
    std::array<std::array<float, kNumBands>, 2> displayedCutoffs_{}; // actual lane cutoff in Hz, including FM
    std::array<float, kNumBands> resonances_{}; // smoothed, 0..1
    BitCrusher crusher_{};
    BeatStutter stutter_{};
    double glitchBeatPhase_ = 0.0;
    float crushMix_ = 0.0f;
    float glitchMix_ = 0.0f;
    Oversampler oversampler_{};
    PeakLimiter limiter_{};
    double fmPhase_ = 0.0;
    std::array<double, kNumBands> stepPhases_{};
    std::array<float, 4> midiValues_{};
    std::array<float, 128> midiCCs_{};
    float routingMix_ = 0.0f;
    float parallelBlend_ = 0.5f;
    float stereoAngle_ = 0.0f;
    double ringPhase_ = 0.0;
    float mix_ = 1.0f;
    float outputGain_ = 1.0f;
    float parameterSmoothing_ = 0.0f;
    bool needsSnap_ = true;
};

} // namespace openedm::core
