#pragma once

#include <array>
#include <cstddef>
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
inline constexpr const char* kMasterBypass = "master_bypass";

// Tempo / clock sync and multi-bar auto-sweep parameters.
inline constexpr const char* kClockSync = "clock_sync";
inline constexpr const char* kTempoBpm = "tempo_bpm";
inline constexpr const char* kTimeSignature = "time_sig";
inline constexpr const char* kSweepEnabled = "sweep_on";
inline constexpr const char* kSweepBypass = "sweep_bypass";
inline constexpr const char* kSweepTarget = "sweep_target";
inline constexpr const char* kSweepBars = "sweep_bars";
inline constexpr const char* kSweepMode = "sweep_mode";
inline constexpr const char* kSweepCutoffStart = "sweep_cut_start";
inline constexpr const char* kSweepCutoffEnd = "sweep_cut_end";
inline constexpr const char* kSweepCutoffSlope = "sweep_cut_slope";
inline constexpr const char* kSweepResStart = "sweep_res_start";
inline constexpr const char* kSweepResEnd = "sweep_res_end";
inline constexpr const char* kSweepResSlope = "sweep_res_slope";

inline constexpr const char* kSweepSlot = "sweep_slot";
inline constexpr const char* kSweepCrushStart = "sweep_crush_start";
inline constexpr const char* kSweepCrushEnd = "sweep_crush_end";
inline constexpr const char* kSweepCrushSlope = "sweep_crush_slope";
inline constexpr const char* kSweepCrushOnset = "sweep_crush_onset";
inline constexpr const char* kSweepGlitchStart = "sweep_glitch_start";
inline constexpr const char* kSweepGlitchEnd = "sweep_glitch_end";
inline constexpr const char* kSweepGlitchSlope = "sweep_glitch_slope";
inline constexpr const char* kSweepGlitchOnset = "sweep_glitch_onset";
inline constexpr std::array<const char*, 9> kSweepFilterIds{
    kSweepTarget, kSweepBars, kSweepMode, kSweepCutoffStart, kSweepCutoffEnd,
    kSweepCutoffSlope, kSweepResStart, kSweepResEnd, kSweepResSlope
};
inline constexpr std::array<const char*, 8> kSweepEffectIds{
    kSweepCrushStart, kSweepCrushEnd, kSweepCrushSlope, kSweepCrushOnset,
    kSweepGlitchStart, kSweepGlitchEnd, kSweepGlitchSlope, kSweepGlitchOnset
};
juce::String sweepId(int slot, const juce::String& originalId);

// Beats per LFO cycle, matching the order of the "lfo_rate" choices.
inline constexpr std::array<double, 12> kLfoBeats = {
    0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0
};

// Selectable auto-sweep lengths, matching every "sweep_bars" choice.
// 1 Bar and 2 Bars were inserted in front of the original 4..64 list.
// Saved sessions from that older list are shifted by kLegacySweepBarShift.
inline constexpr std::array<int, 7> kSweepBarCounts = { 1, 2, 4, 8, 16, 32, 64 };
inline constexpr int kDefaultSweepBarIndex = 3; // 8 bars, the historical default
inline constexpr int kSweepBarsLayout = 2;
inline constexpr int kLegacySweepBarShift = 2;
inline constexpr int kSweepBarsParameterVersion = 3;

static_assert(kSweepBarCounts[kDefaultSweepBarIndex] == 8);
static_assert(kSweepBarCounts[0 + kLegacySweepBarShift] == 4);
static_assert(kSweepBarCounts[1 + kLegacySweepBarShift] == 8);
static_assert(kSweepBarCounts[4 + kLegacySweepBarShift] == 64);

inline int sweepBarIndex(int bars) noexcept {
    for (size_t index = 0; index < kSweepBarCounts.size(); ++index) {
        if (kSweepBarCounts[index] == bars) {
            return static_cast<int>(index);
        }
    }
    return kDefaultSweepBarIndex;
}

// Rewrites pre-layout sweep bar indices in a saved state. New states are left unchanged.
void migrateSweepBarState(juce::ValueTree& state);

// Time signatures (numerator, denominator), matching "time_sig" choices.
inline constexpr std::array<std::pair<int, int>, 9> kTimeSignatures = { {
    { 4, 4 }, { 3, 4 }, { 2, 4 }, { 5, 4 }, { 6, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 }
} };

double beatsPerBarForSignature(int index) noexcept;
juce::String bandId(int band, const char* suffix);
juce::String formatHz(float hz);
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace openedm::params
