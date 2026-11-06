#include "Presets.h"
#include "Parameters.h"
#include <algorithm>

namespace openedm::presets {

namespace {

FactoryPreset texturedSweep(const char* name, int bars, float startHz, float endHz,
                            float slope, float crush, float glitch, float rate, float bits,
                            float crushOnset, float glitchOnset, bool highPass = false) {
    FactoryPreset preset{ name, {
        { "a_type", highPass ? 1.0f : 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f },
        { "crush_rate", rate }, { "crush_bits", bits }, { "crush_pos", 1.0f }
    } };
    for (int slot = 0; slot < 4; ++slot) {
        const float variation = static_cast<float>(slot);
        const float intensity = 0.4f + 0.2f * variation;
        const std::array<std::pair<const char*, float>, 15> values{{
            { params::kSweepBars, static_cast<float>(params::sweepBarIndex(bars)) }, { params::kSweepCutoffStart, startHz },
            { params::kSweepCutoffEnd, endHz }, { params::kSweepCutoffSlope, slope + 0.08f * variation },
            { params::kSweepResStart, 0.12f }, { params::kSweepResEnd, 0.4f + 0.08f * variation },
            { params::kSweepResSlope, 0.15f + 0.1f * variation },
            { params::kSweepCrushStart, 0.0f }, { params::kSweepCrushEnd, crush * intensity },
            { params::kSweepCrushSlope, 0.15f + 0.1f * variation }, { params::kSweepCrushOnset, crushOnset },
            { params::kSweepGlitchStart, 0.0f }, { params::kSweepGlitchEnd, glitch * intensity },
            { params::kSweepGlitchSlope, 0.2f + 0.1f * variation }, { params::kSweepGlitchOnset, glitchOnset }
        }};
        for (const auto& [id, value] : values) preset.values.emplace_back(params::sweepId(slot, id), value);
    }
    return preset;
}

} // namespace

// Choice indices: type 0 LP, 1 HP, 2 BP, 3 Notch. Slope 0 12 dB, 1 24 dB, 2 48 dB, 3 96 dB.
// LFO shape 0 Sine, 1 Triangle, 2 Saw Up, 3 Saw Down, 4 Square. LFO rate 2 = 1/16, 4 = 1/8, 6 = 1/4, 8 = 1 Bar.
// Sweep bars are choice indices from params::kSweepBarCounts: 1, 2, 4, 8, 16, 32, 64.
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
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(8)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 80.0f },
            { "sweep_cut_end", 18000.0f }, { "sweep_cut_slope", 0.35f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.68f }, { "sweep_res_slope", 0.20f }, { "drive", 0.25f } } },
        { "EDM LP Sweep Down (8 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(8)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 18000.0f },
            { "sweep_cut_end", 80.0f }, { "sweep_cut_slope", -0.30f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.60f }, { "sweep_res_slope", 0.15f } } },
        { "EDM HP Riser Up (16 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(16)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 25.0f },
            { "sweep_cut_end", 4500.0f }, { "sweep_cut_slope", 0.45f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.30f } } },
        { "EDM HP Drop Down (4 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(4)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 3500.0f },
            { "sweep_cut_end", 25.0f }, { "sweep_cut_slope", -0.35f }, { "sweep_res_start", 0.65f },
            { "sweep_res_end", 0.10f }, { "sweep_res_slope", -0.25f } } },
        { "EDM BP Build Up (16 Bar)", {
            { "a_type", 2.0f }, { "a_slope", 1.0f }, { "a_width", 0.45f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", static_cast<float>(params::sweepBarIndex(16)) }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 180.0f }, { "sweep_cut_end", 9000.0f }, { "sweep_cut_slope", 0.30f },
            { "sweep_res_start", 0.30f }, { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.20f },
            { "drive", 0.30f } } },
        { "EDM BP Sweep Down (8 Bar)", {
            { "a_type", 2.0f }, { "a_slope", 1.0f }, { "a_width", 0.45f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", static_cast<float>(params::sweepBarIndex(8)) }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 9000.0f }, { "sweep_cut_end", 180.0f }, { "sweep_cut_slope", -0.25f },
            { "sweep_res_start", 0.70f }, { "sweep_res_end", 0.30f }, { "sweep_res_slope", -0.15f } } },
        { "EDM Notch Sweep Up (8 Bar)", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_width", 0.40f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", static_cast<float>(params::sweepBarIndex(8)) }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 120.0f }, { "sweep_cut_end", 8000.0f }, { "sweep_cut_slope", 0.20f },
            { "sweep_res_start", 0.40f }, { "sweep_res_end", 0.85f }, { "sweep_res_slope", 0.20f } } },
        { "EDM Notch Sweep Down (4 Bar)", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_width", 0.40f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", static_cast<float>(params::sweepBarIndex(4)) }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 8000.0f }, { "sweep_cut_end", 120.0f }, { "sweep_cut_slope", -0.20f },
            { "sweep_res_start", 0.80f }, { "sweep_res_end", 0.35f }, { "sweep_res_slope", -0.15f } } },
        { "EDM Acid Bar Pump (4 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(4)) }, { "sweep_mode", 1.0f }, { "sweep_cut_start", 110.0f },
            { "sweep_cut_end", 12000.0f }, { "sweep_cut_slope", 0.40f }, { "sweep_res_start", 0.45f },
            { "sweep_res_end", 0.90f }, { "sweep_res_slope", 0.35f }, { "drive", 0.45f }, { "drive_pos", 1.0f } } },
        { "EDM Big Room Build (32 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(32)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 90.0f },
            { "sweep_cut_end", 19000.0f }, { "sweep_cut_slope", 0.50f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.80f }, { "sweep_res_slope", 0.35f }, { "drive", 0.35f } } },
        { "EDM Trance Odyssey (64 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(64)) }, { "sweep_mode", 2.0f }, { "sweep_cut_start", 120.0f },
            { "sweep_cut_end", 18500.0f }, { "sweep_cut_slope", 0.25f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.70f }, { "sweep_res_slope", 0.15f } } },
        { "EDM Dual Mirror Sweep (16 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "b_type", 2.0f }, { "b_slope", 1.0f },
            { "b_width", 0.55f }, { "sweep_on", 1.0f }, { "sweep_target", 3.0f }, { "sweep_bars", static_cast<float>(params::sweepBarIndex(16)) },
            { "sweep_mode", 2.0f }, { "sweep_cut_start", 140.0f }, { "sweep_cut_end", 14000.0f },
            { "sweep_cut_slope", 0.20f }, { "sweep_res_start", 0.25f }, { "sweep_res_end", 0.70f },
            { "sweep_res_slope", 0.20f }, { "drive", 0.25f } } },
        texturedSweep("Silk to Static (8 Bar)", 8, 80.0f, 18000.0f, 0.2f, 0.45f, 0.3f, 8000.0f, 10.0f, 0.72f, 0.85f),
        texturedSweep("Neon Staircase (16 Bar)", 16, 100.0f, 19000.0f, 0.38f, 0.75f, 0.65f, 4800.0f, 6.0f, 0.68f, 0.80f),
        texturedSweep("Last Bar Breakdown (8 Bar)", 8, 18000.0f, 350.0f, -0.25f, 0.8f, 0.9f, 3200.0f, 5.0f, 0.875f, 0.875f),
        texturedSweep("Mainstage Fracture (32 Bar)", 32, 30.0f, 4800.0f, 0.35f, 0.9f, 1.0f, 2400.0f, 4.0f, 0.75f, 0.875f, true),
        { "Trance 138 Euphoria Build (16 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(16)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 90.0f },
            { "sweep_cut_end", 19500.0f }, { "sweep_cut_slope", 0.38f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.72f }, { "sweep_res_slope", 0.25f }, { "drive", 0.22f } } },
        { "Trance 138 Epic Ascent (32 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(32)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 75.0f },
            { "sweep_cut_end", 20000.0f }, { "sweep_cut_slope", 0.45f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.30f }, { "drive", 0.28f } } },
        { "Trance Uplifting 138 Gate (1/16)", {
            { "a_slope", 2.0f }, { "a_cutoff", 350.0f }, { "a_reso", 0.40f }, { "a_lfo_shape", 4.0f },
            { "a_lfo_rate", 2.0f }, { "a_lfo_depth", 0.85f }, { "inertia", 0.0f }, { "drive", 0.20f } } },
        { "Trance Triplet Acid Squelch (1/8T)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "a_cutoff", 420.0f }, { "a_reso", 0.82f },
            { "a_lfo_shape", 3.0f }, { "a_lfo_rate", 3.0f }, { "a_lfo_depth", 0.55f }, { "a_env_source", 300.0f },
            { "a_env_attack", 2.0f }, { "a_env_release", 80.0f }, { "a_env_depth", 0.40f }, { "drive", 0.35f } } },
        { "Psytrance 140 Rolling Bass Squelch", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "a_cutoff", 280.0f }, { "a_reso", 0.78f },
            { "a_env_source", 180.0f }, { "a_env_attack", 1.5f }, { "a_env_release", 55.0f },
            { "a_env_depth", 0.65f }, { "fm_rate", 120.0f }, { "fm_depth", 0.18f }, { "drive", 0.25f } } },
        { "Psytrance Comb Reso Riser (16 Bar)", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_width", 0.35f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", static_cast<float>(params::sweepBarIndex(16)) }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 150.0f }, { "sweep_cut_end", 9500.0f }, { "sweep_cut_slope", 0.35f },
            { "sweep_res_start", 0.40f }, { "sweep_res_end", 0.88f }, { "sweep_res_slope", 0.25f } } },
        { "Anjunabeats Pluck Filter", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "a_cutoff", 650.0f }, { "a_reso", 0.32f },
            { "a_env_source", 500.0f }, { "a_env_attack", 1.0f }, { "a_env_release", 140.0f },
            { "a_env_depth", 0.60f }, { "drive", 0.15f } } },
        { "Anjunabeats Warm Progressive Sweep (16 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 0.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(16)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 120.0f },
            { "sweep_cut_end", 16000.0f }, { "sweep_cut_slope", 0.18f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.38f }, { "sweep_res_slope", 0.10f } } },
        { "Tech-Trance 138 Slog Gate", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "a_cutoff", 450.0f }, { "a_reso", 0.55f },
            { "a_lfo_shape", 4.0f }, { "a_lfo_rate", 2.0f }, { "a_lfo_depth", 0.90f }, { "drive", 0.48f },
            { "drive_pos", 1.0f }, { "crush_rate", 14000.0f }, { "crush_bits", 12.0f }, { "crush_mix", 0.25f },
            { "inertia", 0.0f } } },
        { "Tech-Trance Screamer HP (8 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 2.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(8)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 35.0f },
            { "sweep_cut_end", 4500.0f }, { "sweep_cut_slope", 0.40f }, { "sweep_res_start", 0.30f },
            { "sweep_res_end", 0.82f }, { "sweep_res_slope", 0.30f }, { "drive", 0.30f } } },
        { "Vocal Trance Ethereal Highpass (8 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(8)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 25.0f },
            { "sweep_cut_end", 2200.0f }, { "sweep_cut_slope", 0.32f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.50f }, { "sweep_res_slope", 0.20f } } },
        { "Vocal Trance Pumping Sidechain", {
            { "b_type", 1.0f }, { "b_slope", 2.0f }, { "b_cutoff", 45.0f }, { "b_lfo_shape", 3.0f },
            { "b_lfo_rate", 6.0f }, { "b_lfo_depth", 0.70f }, { "inertia", 0.0f } } },
        { "Classic Tiësto 1999 Anthem Riser (16 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(16)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 30.0f },
            { "sweep_cut_end", 5500.0f }, { "sweep_cut_slope", 0.42f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.76f }, { "sweep_res_slope", 0.25f }, { "drive", 0.22f } } },
        { "Paul van Dyk Energy Mirror (16 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "b_type", 2.0f }, { "b_slope", 1.0f },
            { "b_width", 0.50f }, { "sweep_on", 1.0f }, { "sweep_target", 3.0f }, { "sweep_bars", static_cast<float>(params::sweepBarIndex(16)) },
            { "sweep_mode", 0.0f }, { "sweep_cut_start", 100.0f }, { "sweep_cut_end", 18000.0f },
            { "sweep_cut_slope", 0.32f }, { "sweep_res_start", 0.20f }, { "sweep_res_end", 0.68f },
            { "sweep_res_slope", 0.20f } } },
        { "Armin ASOT 64-Bar Breakdown Odyssey", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(64)) }, { "sweep_mode", 2.0f }, { "sweep_cut_start", 90.0f },
            { "sweep_cut_end", 19000.0f }, { "sweep_cut_slope", 0.22f }, { "sweep_res_start", 0.18f },
            { "sweep_res_end", 0.74f }, { "sweep_res_slope", 0.15f } } },
        { "Ferry Corsten Gouryella Lead Sweep (8 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(8)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 160.0f },
            { "sweep_cut_end", 18000.0f }, { "sweep_cut_slope", 0.30f }, { "sweep_res_start", 0.25f },
            { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.20f }, { "drive", 0.28f } } },
        { "Hard Trance Screaming 96dB Acid", {
            { "a_type", 0.0f }, { "a_slope", 3.0f }, { "a_cutoff", 700.0f }, { "a_reso", 0.90f },
            { "fm_rate", 160.0f }, { "fm_depth", 0.30f }, { "drive", 0.50f }, { "drive_pos", 1.0f } } },
        { "Trance Drop Silence Plunge (4 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(4)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 19000.0f },
            { "sweep_cut_end", 90.0f }, { "sweep_cut_slope", -0.42f }, { "sweep_res_start", 0.65f },
            { "sweep_res_end", 0.15f }, { "sweep_res_slope", -0.25f } } },
        { "Solarstone Pure Trance Shimmer (16 Bar)", {
            { "a_type", 2.0f }, { "a_slope", 1.0f }, { "a_width", 0.60f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", static_cast<float>(params::sweepBarIndex(16)) }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 220.0f }, { "sweep_cut_end", 11000.0f }, { "sweep_cut_slope", 0.28f },
            { "sweep_res_start", 0.30f }, { "sweep_res_end", 0.72f }, { "sweep_res_slope", 0.18f } } },
        { "Cosmic Gate Exploration Filter (8 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(8)) }, { "sweep_mode", 1.0f }, { "sweep_cut_start", 110.0f },
            { "sweep_cut_end", 15000.0f }, { "sweep_cut_slope", 0.35f }, { "sweep_res_start", 0.35f },
            { "sweep_res_end", 0.80f }, { "sweep_res_slope", 0.22f }, { "drive", 0.25f } } },
        { "138 BPM Supersaw Flanger Notch", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_cutoff", 1100.0f }, { "a_width", 0.55f },
            { "a_lfo_shape", 0.0f }, { "a_lfo_rate", 9.0f }, { "a_lfo_depth", 0.45f }, { "mix", 0.75f } } },
        { "Trance Breakdown Muffle Swell (8 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", static_cast<float>(params::sweepBarIndex(8)) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 250.0f },
            { "sweep_cut_end", 17000.0f }, { "sweep_cut_slope", 0.22f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.55f }, { "sweep_res_slope", 0.15f } } },
        { "Hands-Up Euro Trance Stutter (1/8)", {
            { "a_slope", 1.0f }, { "a_cutoff", 1500.0f }, { "a_reso", 0.35f }, { "a_lfo_shape", 4.0f },
            { "a_lfo_rate", 4.0f }, { "a_lfo_depth", 0.80f }, { "inertia", 0.0f } } },
        texturedSweep("Trance Climax Static Burst (16 Bar)", 16, 85.0f, 19500.0f, 0.40f, 0.70f, 0.60f, 6000.0f, 8.0f, 0.75f, 0.85f),
    };
    return presets;
}

const std::vector<FactoryPreset>& sweepSlopePresets() {
    static const std::vector<FactoryPreset> presets{
        { "EDM Sweep Up (Exp)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 80.0f },
            { "sweep_cut_end", 18000.0f }, { "sweep_cut_slope", 0.35f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.68f }, { "sweep_res_slope", 0.20f } } },
        { "EDM Sweep Down (Drop)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 18000.0f },
            { "sweep_cut_end", 80.0f }, { "sweep_cut_slope", -0.30f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.60f }, { "sweep_res_slope", 0.15f } } },
        { "HP Tension Riser Up", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 25.0f },
            { "sweep_cut_end", 4500.0f }, { "sweep_cut_slope", 0.45f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.78f }, { "sweep_res_slope", 0.30f } } },
        { "HP Bass Drop Down", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 3800.0f },
            { "sweep_cut_end", 25.0f }, { "sweep_cut_slope", -0.40f }, { "sweep_res_start", 0.65f },
            { "sweep_res_end", 0.10f }, { "sweep_res_slope", -0.25f } } },
        { "Acid Res Climb Up", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 110.0f },
            { "sweep_cut_end", 14000.0f }, { "sweep_cut_slope", 0.40f }, { "sweep_res_start", 0.45f },
            { "sweep_res_end", 0.92f }, { "sweep_res_slope", 0.35f } } },
        { "Every-Bar Pump Up", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 1.0f }, { "sweep_cut_start", 100.0f },
            { "sweep_cut_end", 15000.0f }, { "sweep_cut_slope", 0.35f }, { "sweep_res_start", 0.25f },
            { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.20f } } },
        { "Every-Bar Plunge Down", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 1.0f }, { "sweep_cut_start", 15000.0f },
            { "sweep_cut_end", 90.0f }, { "sweep_cut_slope", -0.35f }, { "sweep_res_start", 0.70f },
            { "sweep_res_end", 0.20f }, { "sweep_res_slope", -0.20f } } },
        { "Up & Down Pyramid", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 2.0f }, { "sweep_cut_start", 90.0f },
            { "sweep_cut_end", 17000.0f }, { "sweep_cut_slope", 0.20f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.70f }, { "sweep_res_slope", 0.15f } } },
        { "Bar + Macro Builder", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 3.0f }, { "sweep_cut_start", 80.0f },
            { "sweep_cut_end", 18500.0f }, { "sweep_cut_slope", 0.30f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.80f }, { "sweep_res_slope", 0.25f } } },
        { "Late Texture + Glitch", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_crush_start", 0.0f },
            { "sweep_crush_end", 0.45f }, { "sweep_crush_onset", 0.75f }, { "sweep_crush_slope", 0.25f },
            { "sweep_glitch_start", 0.0f }, { "sweep_glitch_end", 0.45f }, { "sweep_glitch_onset", 0.875f },
            { "sweep_glitch_slope", 0.35f } } },
        { "Final Quarter Fracture", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_crush_start", 0.0f },
            { "sweep_crush_end", 0.9f }, { "sweep_crush_onset", 0.75f }, { "sweep_crush_slope", 0.1f },
            { "sweep_glitch_start", 0.0f }, { "sweep_glitch_end", 1.0f }, { "sweep_glitch_onset", 0.80f },
            { "sweep_glitch_slope", 0.2f } } },
        { "Clean Sweep FX", {
            { "sweep_crush_start", 0.0f }, { "sweep_crush_end", 0.0f },
            { "sweep_glitch_start", 0.0f }, { "sweep_glitch_end", 0.0f } } },
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
    const int slot = static_cast<int>(state.getRawParameterValue(params::kSweepSlot)->load());
    std::ranges::for_each(preset.values, [&](const auto& entry) {
        const bool perSlot = std::ranges::any_of(params::kSweepFilterIds, [&](const char* id) { return juce::String(id) == entry.first; })
            || std::ranges::any_of(params::kSweepEffectIds, [&](const char* id) { return juce::String(id) == entry.first; });
        const auto id = perSlot ? params::sweepId(slot, entry.first) : juce::String(entry.first);
        if (auto* ranged = state.getParameter(id)) {
            ranged->setValueNotifyingHost(ranged->convertTo0to1(entry.second));
        }
    });
}

} // namespace openedm::presets
