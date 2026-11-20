#pragma once

#include "Svf.h"
#include <array>
#include <complex>

namespace openedm::core {

enum class FilterType {
    LowPass,
    HighPass,
    BandPass,
    Notch, AllPass, LowShelf, HighShelf, BellCut
};

enum class FilterSlope {
    Slope12dB,
    Slope24dB,
    Slope48dB,
    Slope96dB
};

struct FilterShape {
    bool enabled = true; // false is a full bypass, crossfaded so it never clicks
    FilterType type = FilterType::LowPass;
    FilterSlope slope = FilterSlope::Slope24dB;
    float resonance = 0.0f; // 0..1, height of the peak at the cutoff
    float width = 0.5f;     // 0..1, narrow to wide peak (and band for band-pass and notch)
};

// Pole-staggered Cytomic SVF cascade coupled with a 4-pole Newton-Raphson ZDF transistor ladder.
class MultiModeFilter {
public:
    static constexpr int kMaxStages = 8;
    static constexpr int kMaxChannels = 2;

    void reset() noexcept;

    // Call once per sample before process(). fmOctaves uses a dedicated zero-delay FM path.
    void update(const FilterShape& shape, double cutoffHz, double sampleRate, float fmOctaves = 0.0f) noexcept;
    [[nodiscard]] float process(int channel, float input) noexcept;

    // Magnitude response of the digital filter at sampleRate, for drawing the curve in the editor.
    [[nodiscard]] static double magnitudeAt(const FilterShape& shape, double cutoffHz, double frequencyHz,
                                            double sampleRate) noexcept;

    [[nodiscard]] static std::complex<double> responseAt(const FilterShape& shape, double cutoffHz,
                                                       double frequencyHz, double sampleRate) noexcept;

private:
    struct LadderState {
        std::array<float, 4> s{};
        [[nodiscard]] float process(float input, float gOnePole, float feedbackK) noexcept;
    };

    float activeMix_ = -1.0f; // 0 bypassed, 1 active; negative until the first update snaps it
    std::array<float, 4> stageMix_{};
    int numStages_ = 2;
    float ladderG_ = 0.0f;
    float smoothedLadderK_ = 0.0f;
    float targetLadderK_ = 0.0f;
    double smoothedLogCutoff_ = 0.0;
    bool hasSmoothed_ = false;
    std::array<float, kMaxStages> targetKs_{};
    std::array<float, kMaxStages> smoothedKs_{};
    std::array<SvfCoefficients, kMaxStages> stageCoefficients_{};
    std::array<std::array<SvfState, kMaxStages>, kMaxChannels> stages_{};
    std::array<LadderState, kMaxChannels> ladders_{};
    bool hasCached_ = false;
    FilterShape cachedShape_{};
};

} // namespace openedm::core
