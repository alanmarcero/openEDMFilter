#include "Presets.h"
#include <algorithm>

namespace openedm::presets {

// Choice indices: type 0 LP, 1 HP, 2 BP, 3 Notch. Slope 0 12 dB, 1 24 dB, 2 48 dB, 3 96 dB.
// LFO shape 0 Sine, 1 Triangle, 2 Saw Up, 3 Saw Down, 4 Square. LFO rate 2 = 1/16, 4 = 1/8, 6 = 1/4, 8 = 1 Bar.
// Sweep bars: 0 = 4 Bars, 1 = 8 Bars, 2 = 16 Bars, 3 = 32 Bars, 4 = 64 Bars.
// Sweep mode: 0 = Bar Window, 1 = Every Bar, 2 = Up & Down, 3 = Bar + Macro.
// Sweep target: 0 = Filter A, 1 = Filter B, 2 = Both A+B, 3 = A/B Mirror.
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
        { "EDM LP Sweep Up (8 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", 1.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 80.0f },
            { "sweep_cut_end", 18000.0f }, { "sweep_cut_slope", 0.35f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.68f }, { "sweep_res_slope", 0.20f }, { "drive", 0.25f } } },
        { "EDM LP Sweep Down (8 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", 1.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 18000.0f },
            { "sweep_cut_end", 80.0f }, { "sweep_cut_slope", -0.30f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.60f }, { "sweep_res_slope", 0.15f } } },
        { "EDM HP Riser Up (16 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", 2.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 25.0f },
            { "sweep_cut_end", 4500.0f }, { "sweep_cut_slope", 0.45f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.30f } } },
        { "EDM HP Drop Down (4 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 3500.0f },
            { "sweep_cut_end", 25.0f }, { "sweep_cut_slope", -0.35f }, { "sweep_res_start", 0.65f },
            { "sweep_res_end", 0.10f }, { "sweep_res_slope", -0.25f } } },
        { "EDM BP Build Up (16 Bar)", {
            { "a_type", 2.0f }, { "a_slope", 1.0f }, { "a_width", 0.45f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", 2.0f }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 180.0f }, { "sweep_cut_end", 9000.0f }, { "sweep_cut_slope", 0.30f },
            { "sweep_res_start", 0.30f }, { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.20f },
            { "drive", 0.30f } } },
        { "EDM BP Sweep Down (8 Bar)", {
            { "a_type", 2.0f }, { "a_slope", 1.0f }, { "a_width", 0.45f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", 1.0f }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 9000.0f }, { "sweep_cut_end", 180.0f }, { "sweep_cut_slope", -0.25f },
            { "sweep_res_start", 0.70f }, { "sweep_res_end", 0.30f }, { "sweep_res_slope", -0.15f } } },
        { "EDM Notch Sweep Up (8 Bar)", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_width", 0.40f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", 1.0f }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 120.0f }, { "sweep_cut_end", 8000.0f }, { "sweep_cut_slope", 0.20f },
            { "sweep_res_start", 0.40f }, { "sweep_res_end", 0.85f }, { "sweep_res_slope", 0.20f } } },
        { "EDM Notch Sweep Down (4 Bar)", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_width", 0.40f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", 0.0f }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 8000.0f }, { "sweep_cut_end", 120.0f }, { "sweep_cut_slope", -0.20f },
            { "sweep_res_start", 0.80f }, { "sweep_res_end", 0.35f }, { "sweep_res_slope", -0.15f } } },
        { "EDM Acid Bar Pump (4 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", 0.0f }, { "sweep_mode", 1.0f }, { "sweep_cut_start", 110.0f },
            { "sweep_cut_end", 12000.0f }, { "sweep_cut_slope", 0.40f }, { "sweep_res_start", 0.45f },
            { "sweep_res_end", 0.90f }, { "sweep_res_slope", 0.35f }, { "drive", 0.45f }, { "drive_pos", 1.0f } } },
        { "EDM Big Room Build (32 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", 3.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 90.0f },
            { "sweep_cut_end", 19000.0f }, { "sweep_cut_slope", 0.50f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.80f }, { "sweep_res_slope", 0.35f }, { "drive", 0.35f } } },
        { "EDM Trance Odyssey (64 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", 4.0f }, { "sweep_mode", 2.0f }, { "sweep_cut_start", 120.0f },
            { "sweep_cut_end", 18500.0f }, { "sweep_cut_slope", 0.25f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.70f }, { "sweep_res_slope", 0.15f } } },
        { "EDM Dual Mirror Sweep (16 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "b_type", 2.0f }, { "b_slope", 1.0f },
            { "b_width", 0.55f }, { "sweep_on", 1.0f }, { "sweep_target", 3.0f }, { "sweep_bars", 2.0f },
            { "sweep_mode", 2.0f }, { "sweep_cut_start", 140.0f }, { "sweep_cut_end", 14000.0f },
            { "sweep_cut_slope", 0.20f }, { "sweep_res_start", 0.25f }, { "sweep_res_end", 0.70f },
            { "sweep_res_slope", 0.20f }, { "drive", 0.25f } } },
    };
    return presets;
}

const std::vector<FactoryPreset>& sweepSlopePresets() {
    static const std::vector<FactoryPreset> presets{
        { "EDM Sweep Up (Exp)", {
            { "sweep_on", 1.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 80.0f },
            { "sweep_cut_end", 18000.0f }, { "sweep_cut_slope", 0.35f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.68f }, { "sweep_res_slope", 0.20f } } },
        { "EDM Sweep Down (Drop)", {
            { "sweep_on", 1.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 18000.0f },
            { "sweep_cut_end", 80.0f }, { "sweep_cut_slope", -0.30f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.60f }, { "sweep_res_slope", 0.15f } } },
        { "HP Tension Riser Up", {
            { "sweep_on", 1.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 25.0f },
            { "sweep_cut_end", 4500.0f }, { "sweep_cut_slope", 0.45f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.78f }, { "sweep_res_slope", 0.30f } } },
        { "HP Bass Drop Down", {
            { "sweep_on", 1.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 3800.0f },
            { "sweep_cut_end", 25.0f }, { "sweep_cut_slope", -0.40f }, { "sweep_res_start", 0.65f },
            { "sweep_res_end", 0.10f }, { "sweep_res_slope", -0.25f } } },
        { "Acid Res Climb Up", {
            { "sweep_on", 1.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 110.0f },
            { "sweep_cut_end", 14000.0f }, { "sweep_cut_slope", 0.40f }, { "sweep_res_start", 0.45f },
            { "sweep_res_end", 0.92f }, { "sweep_res_slope", 0.35f } } },
        { "Every-Bar Pump Up", {
            { "sweep_on", 1.0f }, { "sweep_mode", 1.0f }, { "sweep_cut_start", 100.0f },
            { "sweep_cut_end", 15000.0f }, { "sweep_cut_slope", 0.35f }, { "sweep_res_start", 0.25f },
            { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.20f } } },
        { "Every-Bar Plunge Down", {
            { "sweep_on", 1.0f }, { "sweep_mode", 1.0f }, { "sweep_cut_start", 15000.0f },
            { "sweep_cut_end", 90.0f }, { "sweep_cut_slope", -0.35f }, { "sweep_res_start", 0.70f },
            { "sweep_res_end", 0.20f }, { "sweep_res_slope", -0.20f } } },
        { "Up & Down Pyramid", {
            { "sweep_on", 1.0f }, { "sweep_mode", 2.0f }, { "sweep_cut_start", 90.0f },
            { "sweep_cut_end", 17000.0f }, { "sweep_cut_slope", 0.20f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.70f }, { "sweep_res_slope", 0.15f } } },
        { "Bar + Macro Builder", {
            { "sweep_on", 1.0f }, { "sweep_mode", 3.0f }, { "sweep_cut_start", 80.0f },
            { "sweep_cut_end", 18500.0f }, { "sweep_cut_slope", 0.30f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.80f }, { "sweep_res_slope", 0.25f } } },
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

void applyPartial(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state) {
    std::ranges::for_each(preset.values, [&](const auto& entry) {
        if (auto* ranged = state.getParameter(entry.first)) {
            ranged->setValueNotifyingHost(ranged->convertTo0to1(entry.second));
        }
    });
}

} // namespace openedm::presets
