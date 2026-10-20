#include "Presets.h"
#include <algorithm>

namespace openedm::presets {

// Choice indices: type 0 LP, 1 HP, 2 BP, 3 Notch. Slope 0 12 dB, 1 24 dB, 2 48 dB, 3 96 dB.
// LFO shape 0 Sine, 1 Triangle, 2 Saw Up, 3 Saw Down, 4 Square. LFO rate 2 = 1/16, 4 = 1/8, 6 = 1/4, 8 = 1 Bar.
const std::vector<FactoryPreset>& factoryPresets() {
    static const std::vector<FactoryPreset> presets{
        { "Init", {} },
        { "Classic LP Sweep", {
            { "a_cutoff", 900.0f }, { "a_reso", 0.45f }, { "a_lfo_rate", 9.0f }, { "a_lfo_depth", 0.45f } } },
        { "Dubstep Wobble", {
            { "a_cutoff", 350.0f }, { "a_reso", 0.7f }, { "a_width", 0.35f }, { "a_lfo_rate", 3.0f },
            { "a_lfo_depth", 0.55f }, { "drive", 0.45f } } },
        { "Sidechain Pump", {
            { "b_slope", 2.0f }, { "b_cutoff", 60.0f }, { "b_lfo_shape", 3.0f }, { "b_lfo_rate", 6.0f },
            { "b_lfo_depth", 0.6f } } },
        { "Trance Gate", {
            { "a_slope", 3.0f }, { "a_cutoff", 250.0f }, { "a_lfo_shape", 4.0f }, { "a_lfo_rate", 2.0f },
            { "a_lfo_depth", 0.75f }, { "inertia", 0.0f } } },
        { "Phaser Notch", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_cutoff", 900.0f }, { "a_width", 0.6f },
            { "a_lfo_shape", 1.0f }, { "a_lfo_rate", 9.0f }, { "a_lfo_depth", 0.4f }, { "mix", 0.6f } } },
        { "Stutter Notch", {
            { "a_type", 3.0f }, { "a_slope", 3.0f }, { "a_cutoff", 1200.0f }, { "a_width", 0.3f },
            { "a_lfo_shape", 4.0f }, { "a_lfo_rate", 2.0f }, { "a_lfo_depth", 0.35f }, { "inertia", 0.0f } } },
        { "Telephone", {
            { "a_type", 2.0f }, { "a_cutoff", 1500.0f }, { "a_width", 0.55f }, { "crush_rate", 8000.0f },
            { "crush_bits", 8.0f }, { "crush_mix", 0.4f }, { "drive", 0.3f } } },
        { "Envelope Funk", {
            { "a_cutoff", 300.0f }, { "a_reso", 0.6f }, { "a_width", 0.3f }, { "a_env_source", 200.0f },
            { "a_env_attack", 3.0f }, { "a_env_release", 120.0f }, { "a_env_depth", 0.55f } } },
        { "Ducking Highpass", {
            { "b_cutoff", 400.0f }, { "b_env_source", 80.0f }, { "b_env_attack", 1.0f },
            { "b_env_release", 250.0f }, { "b_env_depth", -0.45f } } },
        { "Lo-Fi Crunch", {
            { "a_cutoff", 5000.0f }, { "crush_rate", 6000.0f }, { "crush_bits", 6.0f }, { "crush_mix", 1.0f },
            { "crush_pos", 0.0f } } },
        { "Linked Crush Sweep", {
            { "a_cutoff", 2000.0f }, { "a_lfo_rate", 9.0f }, { "a_lfo_depth", 0.4f }, { "crush_rate", 3000.0f },
            { "crush_bits", 10.0f }, { "crush_mix", 0.8f }, { "crush_link", 1.0f } } },
        { "Ring Metal", {
            { "a_type", 2.0f }, { "a_cutoff", 1800.0f }, { "a_width", 0.7f }, { "ring_freq", 440.0f },
            { "ring_mix", 0.6f } } },
        { "FM Growl", {
            { "a_cutoff", 600.0f }, { "a_reso", 0.6f }, { "fm_rate", 80.0f }, { "fm_depth", 0.35f },
            { "drive", 0.5f }, { "drive_pos", 1.0f } } },
        { "Riser", {
            { "b_cutoff", 30.0f }, { "b_lfo_shape", 2.0f }, { "b_lfo_rate", 11.0f }, { "b_lfo_depth", 0.8f },
            { "a_reso", 0.2f } } },
        { "Breakdown Muffle", {
            { "a_slope", 2.0f }, { "a_cutoff", 400.0f }, { "a_reso", 0.3f } } },
        { "Telephone Wobble", {
            { "a_type", 2.0f }, { "a_cutoff", 1200.0f }, { "a_width", 0.4f }, { "a_lfo_rate", 4.0f },
            { "a_lfo_depth", 0.3f }, { "b_cutoff", 300.0f } } },
        { "Twin Sweep", {
            { "a_cutoff", 3000.0f }, { "a_lfo_shape", 1.0f }, { "a_lfo_rate", 10.0f }, { "a_lfo_depth", -0.35f },
            { "b_cutoff", 150.0f }, { "b_lfo_shape", 1.0f }, { "b_lfo_rate", 10.0f }, { "b_lfo_depth", 0.35f } } },
    };
    return presets;
}

void apply(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state) {
    std::ranges::for_each(state.processor.getParameters(), [&](juce::AudioProcessorParameter* parameter) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr) {
            return;
        }
        const auto match = std::ranges::find_if(preset.values,
            [&](const auto& value) { return ranged->paramID == value.first; });
        if (match == preset.values.end()) {
            ranged->setValueNotifyingHost(ranged->getDefaultValue());
            return;
        }
        ranged->setValueNotifyingHost(ranged->convertTo0to1(match->second));
    });
}

} // namespace openedm::presets
