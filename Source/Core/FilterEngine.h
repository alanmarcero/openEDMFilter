#pragma once

#include "Effects.h"
#include "Modulators.h"
#include "MultiModeFilter.h"
#include <array>
#include <optional>

namespace openedm::core {

enum class Placement {
    PreFilter,
    PostFilter
};

struct BandSettings {
    FilterShape shape{};
    float cutoffHz = 1000.0f;
    LfoShape lfoShape = LfoShape::Sine;
    double lfoBeats = 4.0; // beats per LFO cycle
    float lfoDepth = 0.0f; // -1..1
    float envSourceHz = 1000.0f;
    float envAttackMs = 5.0f;
    float envReleaseMs = 150.0f;
    float envDepth = 0.0f; // -1..1
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
};

struct Transport {
    double bpm = 120.0;
    std::optional<double> ppqPosition; // set only while the host is playing
};

// Full signal chain: drive and crush (pre or post), band A into band B, ring mod, dry/wet, output, limiter.
class FilterEngine {
public:
    static constexpr int kNumBands = 2;
    static constexpr float kModulationOctaves = 6.0f;
    static constexpr float kFmOctaves = 3.0f;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(const EngineSettings& settings, const Transport& transport,
                 float* const* channels, int numChannels, int numSamples) noexcept;

    // Modulated cutoff of each band at the end of the last block.
    [[nodiscard]] std::array<float, kNumBands> currentCutoffs() const noexcept;

private:
    double sampleRate_ = 44100.0;
    std::array<MultiModeFilter, kNumBands> filters_{};
    std::array<Lfo, kNumBands> lfos_{};
    std::array<EnvelopeFollower, kNumBands> envelopes_{};
    std::array<float, kNumBands> logCutoffs_{}; // smoothed, in log2 Hz
    BitCrusher crusher_{};
    PeakLimiter limiter_{};
    double fmPhase_ = 0.0;
    double ringPhase_ = 0.0;
    float mix_ = 1.0f;
    float outputGain_ = 1.0f;
    float parameterSmoothing_ = 0.0f;
    bool needsSnap_ = true;
};

} // namespace openedm::core
