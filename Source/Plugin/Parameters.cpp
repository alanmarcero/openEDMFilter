#include "Parameters.h"
#include <algorithm>

namespace openedm::params {

namespace {

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using Attributes = juce::AudioParameterFloatAttributes;
using Formatter = std::function<juce::String(float, int)>;

constexpr int kVersion = 1;

const Formatter kFormatHz = [](float hz, int) { return formatHz(hz); };
const Formatter kFormatPercent = [](float value, int) { return juce::String(juce::roundToInt(value * 100.0f)) + "%"; };
const Formatter kFormatBipolar = [](float value, int) {
    const int percent = juce::roundToInt(value * 100.0f);
    return (percent > 0 ? "+" : "") + juce::String(percent) + "%";
};
const Formatter kFormatMs = [](float ms, int) {
    return (ms < 10.0f ? juce::String(ms, 1) : juce::String(juce::roundToInt(ms))) + " ms";
};
const Formatter kFormatDb = [](float db, int) { return juce::String(db, 1) + " dB"; };
const Formatter kFormatBpm = [](float bpm, int) { return juce::String(bpm, 1) + " BPM"; };

juce::NormalisableRange<float> skewedRange(float min, float max, float centre) {
    juce::NormalisableRange<float> range(min, max);
    range.setSkewForCentre(centre);
    return range;
}

void addFloat(Layout& layout, const juce::String& id, const juce::String& name,
              juce::NormalisableRange<float> range, float defaultValue, const Formatter& formatter, int version = kVersion) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ id, version }, name, range, defaultValue, Attributes().withStringFromValueFunction(formatter)));
}

void addChoice(Layout& layout, const juce::String& id, const juce::String& name,
               const juce::StringArray& choices, int defaultIndex, int version = kVersion) {
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id, version }, name, choices, defaultIndex));
}

void addBool(Layout& layout, const juce::String& id, const juce::String& name, bool defaultValue) {
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ id, kVersion }, name, defaultValue));
}

void addBand(Layout& layout, int band) {
    const juce::String prefix = band == 0 ? "A " : "B ";
    const bool isBandA = band == 0;
    const auto id = [band](const char* suffix) { return bandId(band, suffix); };

    addBool(layout, id(kBypass), prefix + "Bypass", false);
    addChoice(layout, id(kType), prefix + "Type", { "Low Pass", "High Pass", "Band Pass", "Notch" }, isBandA ? 0 : 1);
    addChoice(layout, id(kSlope), prefix + "Slope", { "12 dB", "24 dB", "48 dB", "96 dB" }, 1);
    addFloat(layout, id(kCutoff), prefix + "Cutoff", skewedRange(20.0f, 20000.0f, 632.0f), isBandA ? 20000.0f : 20.0f, kFormatHz);
    addFloat(layout, id(kResonance), prefix + "Resonance", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addFloat(layout, id(kWidth), prefix + "Width", { 0.0f, 1.0f }, 0.5f, kFormatPercent);
    addChoice(layout, id(kLfoShape), prefix + "LFO Shape", { "Sine", "Triangle", "Saw Up", "Saw Down", "Square" }, 0);
    addChoice(layout, id(kLfoRate), prefix + "LFO Rate",
              { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars", "8 Bars" }, 8);
    addFloat(layout, id(kLfoDepth), prefix + "LFO Depth", { -1.0f, 1.0f }, 0.0f, kFormatBipolar);
    addFloat(layout, id(kEnvSource), prefix + "Env Source", skewedRange(20.0f, 20000.0f, 632.0f), 1000.0f, kFormatHz);
    addFloat(layout, id(kEnvAttack), prefix + "Env Attack", skewedRange(0.1f, 200.0f, 20.0f), 5.0f, kFormatMs);
    addFloat(layout, id(kEnvRelease), prefix + "Env Release", skewedRange(5.0f, 2000.0f, 200.0f), 150.0f, kFormatMs);
    addFloat(layout, id(kEnvDepth), prefix + "Env Depth", { -1.0f, 1.0f }, 0.0f, kFormatBipolar);
}

} // namespace

double beatsPerBarForSignature(int index) noexcept {
    const auto safeIndex = static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kTimeSignatures.size()) - 1));
    const auto [numerator, denominator] = kTimeSignatures[safeIndex];
    return static_cast<double>(numerator) * (4.0 / static_cast<double>(denominator));
}

juce::String formatHz(float hz) {
    if (hz >= 1000.0f) {
        return juce::String(hz / 1000.0f, 2) + " kHz";
    }
    // juce::String(float, 0) prints every digit, so whole numbers go through roundToInt.
    return (hz < 100.0f ? juce::String(hz, 1) : juce::String(juce::roundToInt(hz))) + " Hz";
}

juce::String bandId(int band, const char* suffix) {
    return juce::String(band == 0 ? "a_" : "b_") + suffix;
}

juce::String sweepId(int slot, const juce::String& originalId) {
    if (slot == 0) return originalId;
    return "sweep_" + juce::String::charToString(static_cast<juce::juce_wchar>('a' + slot)) + "_" + originalId.substring(6);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    const juce::StringArray placements{ "Pre", "Post" };
    Layout layout;
    addBand(layout, 0);
    addBand(layout, 1);

    addFloat(layout, kFmRate, "FM Rate", skewedRange(0.1f, 2000.0f, 50.0f), 5.0f, kFormatHz);
    addFloat(layout, kFmDepth, "FM Depth", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addFloat(layout, kDrive, "Drive", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addChoice(layout, kDrivePosition, "Drive Position", placements, 0);
    addFloat(layout, kCrushRate, "Crush Rate", skewedRange(200.0f, 48000.0f, 4000.0f), 12000.0f, kFormatHz);
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ kCrushBits, kVersion }, "Crush Bits", 1, 16, 8));
    addFloat(layout, kCrushMix, "Crush Mix", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addChoice(layout, kCrushPosition, "Crush Position", placements, 1);
    addBool(layout, kCrushLink, "Crush Link", false);
    addFloat(layout, kRingFrequency, "Ring Frequency", skewedRange(1.0f, 5000.0f, 200.0f), 200.0f, kFormatHz);
    addFloat(layout, kRingMix, "Ring Mix", { 0.0f, 1.0f }, 0.0f, kFormatPercent);
    addFloat(layout, kMix, "Dry/Wet", { 0.0f, 1.0f }, 1.0f, kFormatPercent);
    addFloat(layout, kOutput, "Output", { -24.0f, 12.0f }, 0.0f, kFormatDb);
    addBool(layout, kLimiter, "Limiter", true);
    addFloat(layout, kCeiling, "Ceiling", { -24.0f, 0.0f }, -0.3f, kFormatDb);
    addBool(layout, kInertia, "Filter Inertia", true);
    addBool(layout, kMasterBypass, "Master Bypass", false);

    addBool(layout, kClockSync, "Clock Sync", true);
    addFloat(layout, kTempoBpm, "Tempo", { 40.0f, 240.0f }, 128.0f, kFormatBpm);
    addChoice(layout, kTimeSignature, "Time Signature",
              { "4/4", "3/4", "2/4", "5/4", "6/4", "6/8", "7/8", "9/8", "12/8" }, 0);
    addBool(layout, kSweepEnabled, "Auto Sweep", false);
    addBool(layout, kSweepBypass, "Sweep Bypass", false);
    addChoice(layout, kSweepTarget, "Sweep Target", { "Filter A", "Filter B", "Both A+B", "A/B Mirror" }, 0);
    addChoice(layout, kSweepBars, "Sweep Bars", { "4 Bars", "8 Bars", "16 Bars", "32 Bars", "64 Bars" }, 1);
    addChoice(layout, kSweepMode, "Sweep Mode", { "Bar Window", "Every Bar", "Up & Down", "Bar + Macro" }, 0);
    addFloat(layout, kSweepCutoffStart, "Sweep Cutoff Start", skewedRange(20.0f, 20000.0f, 632.0f), 80.0f, kFormatHz);
    addFloat(layout, kSweepCutoffEnd, "Sweep Cutoff End", skewedRange(20.0f, 20000.0f, 632.0f), 16000.0f, kFormatHz);
    addFloat(layout, kSweepCutoffSlope, "Sweep Cutoff Slope", { -1.0f, 1.0f }, 0.0f, kFormatBipolar);
    addFloat(layout, kSweepResStart, "Sweep Res Start", { 0.0f, 1.0f }, 0.15f, kFormatPercent);
    addFloat(layout, kSweepResEnd, "Sweep Res End", { 0.0f, 1.0f }, 0.65f, kFormatPercent);
    addFloat(layout, kSweepResSlope, "Sweep Res Slope", { -1.0f, 1.0f }, 0.0f, kFormatBipolar);
    addChoice(layout, kSweepSlot, "Auto Mode", { "A", "B", "C", "D" }, 0, 2);
    for (int slot = 1; slot < 4; ++slot) {
        const auto id = [slot](const char* key) { return sweepId(slot, key); };
        const auto name = "Auto " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + slot)) + " ";
        addChoice(layout, id(kSweepTarget), name + "Target", { "Filter A", "Filter B", "Both A+B", "A/B Mirror" }, 0, 2);
        addChoice(layout, id(kSweepBars), name + "Bars", { "4 Bars", "8 Bars", "16 Bars", "32 Bars", "64 Bars" }, 1, 2);
        addChoice(layout, id(kSweepMode), name + "Mode", { "Bar Window", "Every Bar", "Up & Down", "Bar + Macro" }, 0, 2);
        addFloat(layout, id(kSweepCutoffStart), name + "Cutoff Start", skewedRange(20.0f, 20000.0f, 632.0f), 80.0f, kFormatHz, 2);
        addFloat(layout, id(kSweepCutoffEnd), name + "Cutoff End", skewedRange(20.0f, 20000.0f, 632.0f), 16000.0f, kFormatHz, 2);
        addFloat(layout, id(kSweepCutoffSlope), name + "Cutoff Slope", { -1.0f, 1.0f }, 0.0f, kFormatBipolar, 2);
        addFloat(layout, id(kSweepResStart), name + "Res Start", { 0.0f, 1.0f }, 0.15f, kFormatPercent, 2);
        addFloat(layout, id(kSweepResEnd), name + "Res End", { 0.0f, 1.0f }, 0.65f, kFormatPercent, 2);
        addFloat(layout, id(kSweepResSlope), name + "Res Slope", { -1.0f, 1.0f }, 0.0f, kFormatBipolar, 2);
    }
    for (int slot = 0; slot < 4; ++slot) {
        const auto name = "Auto " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + slot)) + " ";
        addFloat(layout, sweepId(slot, kSweepCrushStart), name + "Crush Start", { 0.0f, 1.0f }, 0.0f, kFormatPercent, 2);
        addFloat(layout, sweepId(slot, kSweepCrushEnd), name + "Crush End", { 0.0f, 1.0f }, 0.0f, kFormatPercent, 2);
        addFloat(layout, sweepId(slot, kSweepCrushSlope), name + "Crush Slope", { -1.0f, 1.0f }, 0.35f, kFormatBipolar, 2);
        addFloat(layout, sweepId(slot, kSweepCrushOnset), name + "Crush Onset", { 0.0f, 0.99f }, 0.75f, kFormatPercent, 2);
        addFloat(layout, sweepId(slot, kSweepGlitchStart), name + "Glitch Start", { 0.0f, 1.0f }, 0.0f, kFormatPercent, 2);
        addFloat(layout, sweepId(slot, kSweepGlitchEnd), name + "Glitch End", { 0.0f, 1.0f }, 0.0f, kFormatPercent, 2);
        addFloat(layout, sweepId(slot, kSweepGlitchSlope), name + "Glitch Slope", { -1.0f, 1.0f }, 0.35f, kFormatBipolar, 2);
        addFloat(layout, sweepId(slot, kSweepGlitchOnset), name + "Glitch Onset", { 0.0f, 0.99f }, 0.75f, kFormatPercent, 2);
    }
    return layout;
}

} // namespace openedm::params
