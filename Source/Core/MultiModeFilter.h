#pragma once

#include "Svf.h"
#include <array>

namespace openedm::core {

enum class FilterType {
    LowPass,
    HighPass,
    BandPass,
    Notch
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

// Cascaded SVF filter with up to 8 stages (96 dB/oct) plus a resonance peak at the cutoff.
class MultiModeFilter {
public:
    static constexpr int kMaxStages = 8;
    static constexpr int kMaxChannels = 2;

    void reset() noexcept;

    // Call once per sample before process(). The coefficients are shared by all channels.
    void update(const FilterShape& shape, double cutoffHz, double sampleRate) noexcept;
    [[nodiscard]] float process(int channel, float input) noexcept;

    // Magnitude response of the digital filter at sampleRate, for drawing the curve in the editor.
    [[nodiscard]] static double magnitudeAt(const FilterShape& shape, double cutoffHz, double frequencyHz,
                                            double sampleRate) noexcept;

private:
    float activeMix_ = -1.0f; // 0 bypassed, 1 active; negative until the first update snaps it
    FilterType type_ = FilterType::LowPass;
    int numStages_ = 2;
    float peakGain_ = 0.0f;
    std::array<SvfCoefficients, kMaxStages> stageCoefficients_{};
    SvfCoefficients peakCoefficients_{};
    std::array<std::array<SvfState, kMaxStages>, kMaxChannels> stages_{};
    std::array<SvfState, kMaxChannels> peaks_{};
};

} // namespace openedm::core
