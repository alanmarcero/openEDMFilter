#pragma once

#include <array>
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

// Beats per LFO cycle, matching the order of the "lfo_rate" choices.
inline constexpr std::array<double, 12> kLfoBeats = {
    0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0
};

juce::String bandId(int band, const char* suffix);
juce::String formatHz(float hz);
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace openedm::params
