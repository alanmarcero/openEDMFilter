#pragma once

#include <array>
#include <utility>
#include <juce_audio_processors/juce_audio_processors.h>

namespace openedm::params {

// Per-band parameter suffixes. Use bandId() to build the full ID ("a_cutoff", "b_bypass").
inline constexpr const char* kBypass = "bypass";
inline constexpr const char* kType = "type";
inline constexpr const char* kSlope = "slope";
inline constexpr const char* kCutoff = "cutoff";
inline constexpr const char* kResonance = "reso";
inline constexpr const char* kWidth = "width";
inline constexpr const char* kLfoShape = "lfo_shape";
inline constexpr const char* kLfoRate = "lfo_rate";
inline constexpr const char* kLfoDepth = "lfo_depth";
inline constexpr const char* kEnvSource = "env_source";
inline constexpr const char* kEnvAttack = "env_attack";
inline constexpr const char* kEnvRelease = "env_release";
inline constexpr const char* kEnvDepth = "env_depth";

inline constexpr const char* kFmRate = "fm_rate";
inline constexpr const char* kFmDepth = "fm_depth";
inline constexpr const char* kDrive = "drive";
inline constexpr const char* kDrivePosition = "drive_pos";
inline constexpr const char* kCrushRate = "crush_rate";
inline constexpr const char* kCrushBits = "crush_bits";
inline constexpr const char* kCrushMix = "crush_mix";
inline constexpr const char* kCrushPosition = "crush_pos";
inline constexpr const char* kCrushLink = "crush_link";
inline constexpr const char* kRingFrequency = "ring_freq";
inline constexpr const char* kRingMix = "ring_mix";
inline constexpr const char* kMix = "mix";
inline constexpr const char* kOutput = "output";
inline constexpr const char* kLimiter = "limiter";
inline constexpr const char* kCeiling = "ceiling";
inline constexpr const char* kInertia = "inertia";

// Tempo / clock sync and multi-bar auto-sweep parameters.
inline constexpr const char* kClockSync = "clock_sync";
inline constexpr const char* kTempoBpm = "tempo_bpm";
inline constexpr const char* kTimeSignature = "time_sig";
inline constexpr const char* kSweepEnabled = "sweep_on";
inline constexpr const char* kSweepTarget = "sweep_target";
inline constexpr const char* kSweepBars = "sweep_bars";
inline constexpr const char* kSweepMode = "sweep_mode";
inline constexpr const char* kSweepCutoffStart = "sweep_cut_start";
inline constexpr const char* kSweepCutoffEnd = "sweep_cut_end";
inline constexpr const char* kSweepCutoffSlope = "sweep_cut_slope";
inline constexpr const char* kSweepResStart = "sweep_res_start";
inline constexpr const char* kSweepResEnd = "sweep_res_end";
inline constexpr const char* kSweepResSlope = "sweep_res_slope";

// Beats per LFO cycle, matching the order of the "lfo_rate" choices.
inline constexpr std::array<double, 12> kLfoBeats = {
    0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0
};

// Selectable multi-bar loop lengths for the auto-sweep, matching "sweep_bars" choices.
inline constexpr std::array<int, 5> kSweepBarCounts = { 4, 8, 16, 32, 64 };

// Time signatures (numerator, denominator), matching "time_sig" choices.
inline constexpr std::array<std::pair<int, int>, 9> kTimeSignatures = { {
    { 4, 4 }, { 3, 4 }, { 2, 4 }, { 5, 4 }, { 6, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 }
} };

double beatsPerBarForSignature(int index) noexcept;
juce::String bandId(int band, const char* suffix);
juce::String formatHz(float hz);
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace openedm::params
