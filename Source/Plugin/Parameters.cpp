#include "Parameters.h"

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

juce::NormalisableRange<float> skewedRange(float min, float max, float centre) {
    juce::NormalisableRange<float> range(min, max);
    range.setSkewForCentre(centre);
    return range;
}

void addFloat(Layout& layout, const juce::String& id, const juce::String& name,
              juce::NormalisableRange<float> range, float defaultValue, const Formatter& formatter) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ id, kVersion }, name, range, defaultValue, Attributes().withStringFromValueFunction(formatter)));
}

void addChoice(Layout& layout, const juce::String& id, const juce::String& name,
               const juce::StringArray& choices, int defaultIndex) {
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ id, kVersion }, name, choices, defaultIndex));
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
    return layout;
}

} // namespace openedm::params
