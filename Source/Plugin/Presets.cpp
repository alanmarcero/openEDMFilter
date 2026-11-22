#include "Presets.h"
#include "Parameters.h"
#include <algorithm>
#include <array>

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

float barsIdx(int bars) { return static_cast<float>(params::sweepBarIndex(bars)); }

} // namespace

// Choice indices: type 0 LP, 1 HP, 2 BP, 3 Notch. Slope 0 12 dB, 1 24 dB, 2 48 dB, 3 96 dB.
// LFO shape 0 Sine, 1 Triangle, 2 Saw Up, 3 Saw Down, 4 Square. LFO rate 2 = 1/16, 4 = 1/8, 6 = 1/4, 8 = 1 Bar.
// Sweep bars are choice indices from params::kSweepBarCounts: 1, 2, 4, 8, 16, 32, 64.
// Sweep mode: 0 = Bar Window, 1 = Every Bar, 2 = Up & Down, 3 = Bar + Macro.
// Sweep target: 0 = Filter A, 1 = Filter B, 2 = Both A+B, 3 = A/B Mirror.
const std::vector<FactoryPreset>& factoryPresets() {
    static const std::vector<FactoryPreset> presets{
        // Uplifting (24)
        { "Uplifting / Init", {} },
        { "Uplifting / Classic LP Sweep", {
            { "a_cutoff", 900.0f }, { "a_reso", 0.45f }, { "a_lfo_rate", 9.0f }, { "a_lfo_depth", 0.45f } } },
        { "Uplifting / Euphoria Build (16 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(16) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 90.0f },
            { "sweep_cut_end", 19500.0f }, { "sweep_cut_slope", 0.38f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.72f }, { "sweep_res_slope", 0.25f }, { "drive", 0.22f } } },
        { "Uplifting / Epic Ascent (32 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(32) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 75.0f },
            { "sweep_cut_end", 20000.0f }, { "sweep_cut_slope", 0.45f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.30f }, { "drive", 0.28f } } },
        { "Uplifting / 138 Gate (1/16)", {
            { "a_slope", 2.0f }, { "a_cutoff", 350.0f }, { "a_reso", 0.40f }, { "a_lfo_shape", 4.0f },
            { "a_lfo_rate", 2.0f }, { "a_lfo_depth", 0.85f }, { "inertia", 0.0f }, { "drive", 0.20f } } },
        { "Uplifting / Supersaw Flanger", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_cutoff", 1100.0f }, { "a_width", 0.55f },
            { "a_lfo_shape", 0.0f }, { "a_lfo_rate", 9.0f }, { "a_lfo_depth", 0.45f }, { "mix", 0.75f } } },
        { "Uplifting / Anthem Riser (16 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(16) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 30.0f },
            { "sweep_cut_end", 5500.0f }, { "sweep_cut_slope", 0.42f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.76f }, { "sweep_res_slope", 0.25f }, { "drive", 0.22f } } },
        { "Uplifting / Air HP Riser (16 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(16) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 25.0f },
            { "sweep_cut_end", 4500.0f }, { "sweep_cut_slope", 0.45f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.30f } } },
        { "Uplifting / Vocal Ethereal (8 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(8) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 25.0f },
            { "sweep_cut_end", 2200.0f }, { "sweep_cut_slope", 0.32f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.50f }, { "sweep_res_slope", 0.20f } } },
        { "Uplifting / Vocal Sidechain Pump", {
            { "b_type", 1.0f }, { "b_slope", 2.0f }, { "b_cutoff", 45.0f }, { "b_lfo_shape", 3.0f },
            { "b_lfo_rate", 6.0f }, { "b_lfo_depth", 0.70f }, { "inertia", 0.0f } } },
        { "Uplifting / Celestial Lead (8 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(8) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 160.0f },
            { "sweep_cut_end", 18000.0f }, { "sweep_cut_slope", 0.30f }, { "sweep_res_start", 0.25f },
            { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.20f }, { "drive", 0.28f } } },
        { "Uplifting / Shimmer BP (16 Bar)", {
            { "a_type", 2.0f }, { "a_slope", 1.0f }, { "a_width", 0.60f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", barsIdx(16) }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 220.0f }, { "sweep_cut_end", 11000.0f }, { "sweep_cut_slope", 0.28f },
            { "sweep_res_start", 0.30f }, { "sweep_res_end", 0.72f }, { "sweep_res_slope", 0.18f } } },
        { "Uplifting / Mirror Sweep (16 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "b_type", 2.0f }, { "b_slope", 1.0f },
            { "b_width", 0.50f }, { "sweep_on", 1.0f }, { "sweep_target", 3.0f }, { "sweep_bars", barsIdx(16) },
            { "sweep_mode", 0.0f }, { "sweep_cut_start", 100.0f }, { "sweep_cut_end", 18000.0f },
            { "sweep_cut_slope", 0.32f }, { "sweep_res_start", 0.20f }, { "sweep_res_end", 0.68f },
            { "sweep_res_slope", 0.20f } } },
        { "Uplifting / Odyssey (64 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(64) }, { "sweep_mode", 2.0f }, { "sweep_cut_start", 90.0f },
            { "sweep_cut_end", 19000.0f }, { "sweep_cut_slope", 0.22f }, { "sweep_res_start", 0.18f },
            { "sweep_res_end", 0.74f }, { "sweep_res_slope", 0.15f } } },
        { "Uplifting / Hands-Up Stutter (1/8)", {
            { "a_slope", 1.0f }, { "a_cutoff", 1500.0f }, { "a_reso", 0.35f }, { "a_lfo_shape", 4.0f },
            { "a_lfo_rate", 4.0f }, { "a_lfo_depth", 0.80f }, { "inertia", 0.0f } } },
        { "Uplifting / Drop Plunge (4 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(4) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 19000.0f },
            { "sweep_cut_end", 90.0f }, { "sweep_cut_slope", -0.42f }, { "sweep_res_start", 0.65f },
            { "sweep_res_end", 0.15f }, { "sweep_res_slope", -0.25f } } },
        texturedSweep("Uplifting / Silk to Static (8 Bar)", 8, 80.0f, 18000.0f, 0.2f, 0.45f, 0.3f, 8000.0f, 10.0f, 0.72f, 0.85f),
        texturedSweep("Uplifting / Climax Static (16 Bar)", 16, 85.0f, 19500.0f, 0.40f, 0.70f, 0.60f, 6000.0f, 8.0f, 0.75f, 0.85f),
        { "Uplifting / Euphoria B-Riser (16 Bar)", {
            { "b_type", 1.0f }, { "b_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 1.0f },
            { "sweep_bars", barsIdx(16) }, { "sweep_cut_start", 40.0f }, { "sweep_cut_end", 6500.0f },
            { "sweep_cut_slope", 0.45f }, { "sweep_res_start", 0.20f }, { "sweep_res_end", 0.75f },
            { "drive", 0.25f } } },
        { "Uplifting / Morning Horizon (16 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "b_type", 1.0f }, { "b_slope", 0.0f },
            { "b_cutoff", 55.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f }, { "sweep_bars", barsIdx(16) },
            { "sweep_mode", 0.0f }, { "sweep_cut_start", 130.0f }, { "sweep_cut_end", 17500.0f },
            { "sweep_cut_slope", 0.34f }, { "sweep_res_start", 0.22f }, { "sweep_res_end", 0.62f },
            { "drive", 0.16f } } },
        { "Uplifting / Skyline Pluck Open", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "a_cutoff", 520.0f }, { "a_reso", 0.38f },
            { "a_env_source", 480.0f }, { "a_env_attack", 1.0f }, { "a_env_release", 160.0f },
            { "a_env_depth", 0.68f }, { "drive", 0.14f } } },
        { "Uplifting / Halo Air Shelf", {
            { "a_type", 0.0f }, { "a_cutoff", 12000.0f }, { "b_response", 2.0f }, { "b_cutoff", 8500.0f },
            { "b_reso", 0.55f }, { "b_lfo_shape", 0.0f }, { "b_lfo_rate", 9.0f }, { "b_lfo_depth", 0.22f },
            { "mix", 0.85f } } },
        { "Uplifting / Afterglow Swell (8 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(8) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 250.0f },
            { "sweep_cut_end", 17000.0f }, { "sweep_cut_slope", 0.22f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.55f }, { "sweep_res_slope", 0.15f } } },
        { "Uplifting / Parallel Mirror Air", {
            { "routing", 1.0f }, { "parallel_blend", 0.55f }, { "a_type", 0.0f }, { "a_cutoff", 4200.0f },
            { "a_reso", 0.42f }, { "b_type", 2.0f }, { "b_cutoff", 1800.0f }, { "b_width", 0.60f },
            { "a_lfo_rate", 9.0f }, { "a_lfo_depth", 0.35f }, { "b_lfo_rate", 9.0f }, { "b_lfo_depth", -0.30f } } },

        // Rolling (12)
        { "Rolling / Sidechain Pump", {
            { "b_slope", 2.0f }, { "b_cutoff", 60.0f }, { "b_lfo_shape", 3.0f }, { "b_lfo_rate", 6.0f },
            { "b_lfo_depth", 0.60f } } },
        { "Rolling / Acid Bar Pump (4 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(4) }, { "sweep_mode", 1.0f }, { "sweep_cut_start", 110.0f },
            { "sweep_cut_end", 12000.0f }, { "sweep_cut_slope", 0.40f }, { "sweep_res_start", 0.45f },
            { "sweep_res_end", 0.90f }, { "sweep_res_slope", 0.35f }, { "drive", 0.45f }, { "drive_pos", 1.0f } } },
        { "Rolling / Triplet Squelch (1/8T)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "a_cutoff", 420.0f }, { "a_reso", 0.82f },
            { "a_lfo_shape", 3.0f }, { "a_lfo_rate", 3.0f }, { "a_lfo_depth", 0.55f }, { "a_env_source", 300.0f },
            { "a_env_attack", 2.0f }, { "a_env_release", 80.0f }, { "a_env_depth", 0.40f }, { "drive", 0.35f } } },
        { "Rolling / Quarter Duck Punch", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "a_cutoff", 680.0f }, { "a_reso", 0.48f },
            { "a_lfo_shape", 2.0f }, { "a_lfo_rate", 6.0f }, { "a_lfo_depth", 0.52f }, { "inertia", 0.1f },
            { "drive", 0.25f } } },
        { "Rolling / 16th Arp Chug", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "a_cutoff", 320.0f }, { "a_reso", 0.58f },
            { "a_lfo_shape", 3.0f }, { "a_lfo_rate", 2.0f }, { "a_lfo_depth", 0.65f }, { "inertia", 0.0f } } },
        { "Rolling / Sub & Mid Pump", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "a_cutoff", 1600.0f }, { "a_reso", 0.35f },
            { "b_type", 1.0f }, { "b_slope", 2.0f }, { "b_cutoff", 70.0f }, { "b_lfo_shape", 3.0f },
            { "b_lfo_rate", 6.0f }, { "b_lfo_depth", 0.48f }, { "drive", 0.22f } } },
        { "Rolling / Groove Pluck Env", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "a_cutoff", 280.0f }, { "a_reso", 0.52f },
            { "a_env_source", 260.0f }, { "a_env_attack", 1.0f }, { "a_env_release", 95.0f },
            { "a_env_depth", 0.62f }, { "drive", 0.28f } } },
        texturedSweep("Rolling / Neon Staircase (16 Bar)", 16, 100.0f, 19000.0f, 0.38f, 0.75f, 0.65f, 4800.0f, 6.0f, 0.68f, 0.80f),
        { "Rolling / Bassline Pocket (8 Bar)", {
            { "b_type", 1.0f }, { "b_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 1.0f },
            { "sweep_bars", barsIdx(8) }, { "sweep_mode", 1.0f }, { "sweep_cut_start", 40.0f },
            { "sweep_cut_end", 320.0f }, { "sweep_cut_slope", 0.25f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.45f } } },
        { "Rolling / Dotted 1/8 Bounce", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "a_cutoff", 540.0f }, { "a_reso", 0.60f },
            { "a_lfo_shape", 1.0f }, { "a_lfo_rate", 5.0f }, { "a_lfo_depth", 0.50f }, { "drive", 0.30f } } },
        { "Rolling / Resonant Drive Push", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "a_cutoff", 920.0f }, { "a_reso", 0.68f },
            { "drive", 0.48f }, { "drive_pos", 0.0f }, { "a_lfo_rate", 6.0f }, { "a_lfo_depth", 0.30f } } },
        { "Rolling / HP Fill Cleaver (4 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(4) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 35.0f },
            { "sweep_cut_end", 2400.0f }, { "sweep_cut_slope", 0.50f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.58f } } },

        // Progressive (12)
        { "Progressive / Deep Sub Clean", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "a_cutoff", 2800.0f }, { "a_reso", 0.25f },
            { "a_lfo_shape", 0.0f }, { "a_lfo_rate", 8.0f }, { "a_lfo_depth", 0.20f } } },
        { "Progressive / Warm Sweep (16 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 0.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(16) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 120.0f },
            { "sweep_cut_end", 16000.0f }, { "sweep_cut_slope", 0.18f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.38f }, { "sweep_res_slope", 0.10f } } },
        { "Progressive / Pluck Filter", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "a_cutoff", 650.0f }, { "a_reso", 0.32f },
            { "a_env_source", 500.0f }, { "a_env_attack", 1.0f }, { "a_env_release", 140.0f },
            { "a_env_depth", 0.60f }, { "drive", 0.15f } } },
        { "Progressive / Hypnotic BP (16 Bar)", {
            { "a_type", 2.0f }, { "a_slope", 1.0f }, { "a_width", 0.60f }, { "sweep_on", 1.0f },
            { "sweep_bars", barsIdx(16) }, { "sweep_cut_start", 250.0f }, { "sweep_cut_end", 4500.0f },
            { "sweep_cut_slope", 0.20f }, { "sweep_res_start", 0.35f }, { "sweep_res_end", 0.65f } } },
        { "Progressive / LP Sweep Up (8 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(8) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 80.0f },
            { "sweep_cut_end", 18000.0f }, { "sweep_cut_slope", 0.35f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.68f }, { "sweep_res_slope", 0.20f }, { "drive", 0.25f } } },
        { "Progressive / LP Down (8 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(8) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 18000.0f },
            { "sweep_cut_end", 80.0f }, { "sweep_cut_slope", -0.30f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.60f }, { "sweep_res_slope", 0.15f } } },
        { "Progressive / Breakdown Muffle", {
            { "a_slope", 2.0f }, { "a_cutoff", 400.0f }, { "a_reso", 0.30f } } },
        { "Progressive / Twin Sweep", {
            { "a_cutoff", 3000.0f }, { "a_lfo_shape", 1.0f }, { "a_lfo_rate", 10.0f }, { "a_lfo_depth", -0.35f },
            { "b_cutoff", 150.0f }, { "b_lfo_shape", 1.0f }, { "b_lfo_rate", 10.0f }, { "b_lfo_depth", 0.35f } } },
        { "Progressive / Velvet Notch", {
            { "a_type", 3.0f }, { "a_slope", 1.0f }, { "a_cutoff", 850.0f }, { "a_width", 0.65f },
            { "a_lfo_shape", 0.0f }, { "a_lfo_rate", 10.0f }, { "a_lfo_depth", 0.32f }, { "mix", 0.55f } } },
        { "Progressive / Slow Bloom (32 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 0.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(32) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 180.0f },
            { "sweep_cut_end", 14500.0f }, { "sweep_cut_slope", 0.15f }, { "sweep_res_start", 0.12f },
            { "sweep_res_end", 0.32f } } },
        { "Progressive / Odyssey Return (64 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(64) }, { "sweep_mode", 2.0f }, { "sweep_cut_start", 120.0f },
            { "sweep_cut_end", 18500.0f }, { "sweep_cut_slope", 0.25f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.70f }, { "sweep_res_slope", 0.15f } } },
        { "Progressive / Warm Shelf Lift", {
            { "a_type", 0.0f }, { "a_cutoff", 6200.0f }, { "b_response", 1.0f }, { "b_cutoff", 140.0f },
            { "b_reso", 0.45f }, { "drive", 0.18f } } },

        // Psy (12)
        { "Psy / 140 Rolling Squelch", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "a_cutoff", 280.0f }, { "a_reso", 0.78f },
            { "a_env_source", 180.0f }, { "a_env_attack", 1.5f }, { "a_env_release", 55.0f },
            { "a_env_depth", 0.65f }, { "fm_rate", 120.0f }, { "fm_depth", 0.18f }, { "drive", 0.25f } } },
        { "Psy / Comb Reso Riser (16 Bar)", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_width", 0.35f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", barsIdx(16) }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 150.0f }, { "sweep_cut_end", 9500.0f }, { "sweep_cut_slope", 0.35f },
            { "sweep_res_start", 0.40f }, { "sweep_res_end", 0.88f }, { "sweep_res_slope", 0.25f } } },
        { "Psy / Screaming 96dB Acid", {
            { "a_type", 0.0f }, { "a_slope", 3.0f }, { "a_cutoff", 700.0f }, { "a_reso", 0.90f },
            { "fm_rate", 160.0f }, { "fm_depth", 0.30f }, { "drive", 0.50f }, { "drive_pos", 1.0f } } },
        { "Psy / Laser Sweep (8 Bar)", {
            { "a_type", 2.0f }, { "a_slope", 2.0f }, { "a_width", 0.30f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", barsIdx(8) }, { "sweep_cut_start", 200.0f },
            { "sweep_cut_end", 12000.0f }, { "sweep_cut_slope", 0.35f }, { "sweep_res_start", 0.40f },
            { "sweep_res_end", 0.90f }, { "drive", 0.40f } } },
        { "Psy / Stutter Notch", {
            { "a_type", 3.0f }, { "a_slope", 3.0f }, { "a_cutoff", 1200.0f }, { "a_width", 0.30f },
            { "a_lfo_shape", 4.0f }, { "a_lfo_rate", 2.0f }, { "a_lfo_depth", 0.35f }, { "inertia", 0.0f } } },
        { "Psy / Exploration Notch (8 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(8) }, { "sweep_mode", 1.0f }, { "sweep_cut_start", 110.0f },
            { "sweep_cut_end", 15000.0f }, { "sweep_cut_slope", 0.35f }, { "sweep_res_start", 0.35f },
            { "sweep_res_end", 0.80f }, { "sweep_res_slope", 0.22f }, { "drive", 0.25f } } },
        texturedSweep("Psy / Fracture HP (32 Bar)", 32, 30.0f, 4800.0f, 0.35f, 0.9f, 1.0f, 2400.0f, 4.0f, 0.75f, 0.875f, true),
        { "Psy / FM Swarm Band", {
            { "a_type", 2.0f }, { "a_slope", 2.0f }, { "a_cutoff", 1150.0f }, { "a_width", 0.38f },
            { "a_reso", 0.75f }, { "fm_rate", 190.0f }, { "fm_depth", 0.42f }, { "a_lfo_rate", 3.0f },
            { "a_lfo_depth", 0.45f }, { "drive", 0.35f } } },
        { "Psy / Goa Triple Phaser", {
            { "routing", 1.0f }, { "parallel_blend", 0.50f }, { "a_type", 3.0f }, { "a_slope", 2.0f },
            { "a_cutoff", 780.0f }, { "a_reso", 0.72f }, { "b_type", 3.0f }, { "b_slope", 2.0f },
            { "b_cutoff", 2200.0f }, { "b_reso", 0.68f }, { "a_lfo_rate", 4.0f }, { "a_lfo_depth", 0.60f },
            { "b_lfo_rate", 3.0f }, { "b_lfo_depth", -0.55f } } },
        { "Psy / Squelch Envelope Lead", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "a_cutoff", 210.0f }, { "a_reso", 0.86f },
            { "a_env_source", 280.0f }, { "a_env_attack", 0.5f }, { "a_env_release", 45.0f },
            { "a_env_depth", 0.82f }, { "drive", 0.38f } } },
        { "Psy / Alien Ring Sweep", {
            { "a_type", 2.0f }, { "a_slope", 1.0f }, { "a_cutoff", 1600.0f }, { "a_width", 0.45f },
            { "ring_freq", 320.0f }, { "ring_mix", 0.48f }, { "a_lfo_shape", 2.0f },
            { "a_lfo_rate", 2.0f }, { "a_lfo_depth", 0.65f } } },
        { "Psy / Forest Warp (16 Bar)", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "b_type", 2.0f }, { "b_slope", 1.0f },
            { "sweep_on", 1.0f }, { "sweep_target", 3.0f }, { "sweep_bars", barsIdx(16) },
            { "sweep_mode", 2.0f }, { "sweep_cut_start", 180.0f }, { "sweep_cut_end", 11000.0f },
            { "sweep_res_start", 0.45f }, { "sweep_res_end", 0.88f }, { "fm_rate", 85.0f }, { "fm_depth", 0.22f } } },

        // Tech (12)
        { "Tech / House Acid", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "a_cutoff", 450.0f }, { "a_reso", 0.82f },
            { "a_lfo_shape", 2.0f }, { "a_lfo_rate", 2.0f }, { "a_lfo_depth", 0.65f }, { "drive", 0.45f } } },
        { "Tech / 138 Slog Gate", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "a_cutoff", 450.0f }, { "a_reso", 0.55f },
            { "a_lfo_shape", 4.0f }, { "a_lfo_rate", 2.0f }, { "a_lfo_depth", 0.90f }, { "drive", 0.48f },
            { "drive_pos", 1.0f }, { "crush_rate", 14000.0f }, { "crush_bits", 12.0f }, { "crush_mix", 0.25f },
            { "inertia", 0.0f } } },
        { "Tech / Screamer HP (8 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 2.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(8) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 35.0f },
            { "sweep_cut_end", 4500.0f }, { "sweep_cut_slope", 0.40f }, { "sweep_res_start", 0.30f },
            { "sweep_res_end", 0.82f }, { "sweep_res_slope", 0.30f }, { "drive", 0.30f } } },
        { "Tech / Electro Bit-Filter", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "a_cutoff", 3500.0f }, { "a_reso", 0.55f },
            { "crush_rate", 4000.0f }, { "crush_bits", 7.0f }, { "crush_mix", 0.50f }, { "crush_link", 1.0f },
            { "drive", 0.45f } } },
        { "Tech / Big Room Build (32 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(32) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 90.0f },
            { "sweep_cut_end", 19000.0f }, { "sweep_cut_slope", 0.50f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.80f }, { "sweep_res_slope", 0.35f }, { "drive", 0.35f } } },
        { "Tech / Mainstage Sub Top (32 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "b_type", 1.0f }, { "b_slope", 1.0f },
            { "sweep_on", 1.0f }, { "sweep_target", 2.0f }, { "sweep_bars", barsIdx(32) },
            { "sweep_cut_start", 120.0f }, { "sweep_cut_end", 18500.0f }, { "sweep_cut_slope", 0.45f },
            { "sweep_res_start", 0.25f }, { "sweep_res_end", 0.85f }, { "drive", 0.35f } } },
        { "Tech / Hardstyle Reso", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "a_cutoff", 1200.0f }, { "a_reso", 0.88f },
            { "drive", 0.70f }, { "crush_bits", 6.0f }, { "crush_mix", 0.40f } } },
        { "Tech / HP Drop Down (4 Bar)", {
            { "a_type", 1.0f }, { "a_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 0.0f },
            { "sweep_bars", barsIdx(4) }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 3500.0f },
            { "sweep_cut_end", 25.0f }, { "sweep_cut_slope", -0.35f }, { "sweep_res_start", 0.65f },
            { "sweep_res_end", 0.10f }, { "sweep_res_slope", -0.25f } } },
        texturedSweep("Tech / Last Bar Break (8 Bar)", 8, 18000.0f, 350.0f, -0.25f, 0.8f, 0.9f, 3200.0f, 5.0f, 0.875f, 0.875f),
        { "Tech / Ring Metal", {
            { "a_type", 2.0f }, { "a_cutoff", 1800.0f }, { "a_width", 0.70f }, { "ring_freq", 440.0f },
            { "ring_mix", 0.60f } } },
        { "Tech / Telephone", {
            { "a_type", 2.0f }, { "a_cutoff", 1500.0f }, { "a_width", 0.55f }, { "crush_rate", 8000.0f },
            { "crush_bits", 8.0f }, { "crush_mix", 0.40f }, { "drive", 0.30f } } },
        { "Tech / Lo-Fi Crunch", {
            { "a_cutoff", 5000.0f }, { "crush_rate", 6000.0f }, { "crush_bits", 6.0f }, { "crush_mix", 1.0f },
            { "crush_pos", 0.0f } } },

        // Bass & Breaks (12)
        { "Bass & Breaks / Dubstep Wobble", {
            { "a_cutoff", 350.0f }, { "a_reso", 0.70f }, { "a_width", 0.35f }, { "a_lfo_rate", 3.0f },
            { "a_lfo_depth", 0.55f }, { "drive", 0.45f } } },
        { "Bass & Breaks / Formant Notch", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_cutoff", 850.0f }, { "a_width", 0.35f },
            { "a_reso", 0.75f }, { "a_lfo_rate", 4.0f }, { "a_lfo_depth", 0.60f }, { "crush_bits", 8.0f },
            { "crush_mix", 0.35f }, { "drive", 0.50f } } },
        { "Bass & Breaks / Wobble Stomp", {
            { "a_type", 2.0f }, { "a_slope", 2.0f }, { "a_cutoff", 800.0f }, { "a_width", 0.45f },
            { "a_reso", 0.70f }, { "a_lfo_shape", 3.0f }, { "a_lfo_rate", 3.0f }, { "a_lfo_depth", 0.70f },
            { "drive", 0.55f } } },
        { "Bass & Breaks / DnB Stepper Comb", {
            { "a_type", 3.0f }, { "a_slope", 3.0f }, { "a_cutoff", 1400.0f }, { "a_width", 0.25f },
            { "a_lfo_shape", 1.0f }, { "a_lfo_rate", 2.0f }, { "a_lfo_depth", 0.50f }, { "inertia", 0.8f } } },
        { "Bass & Breaks / FM Growl", {
            { "a_cutoff", 600.0f }, { "a_reso", 0.60f }, { "fm_rate", 80.0f }, { "fm_depth", 0.35f },
            { "drive", 0.50f }, { "drive_pos", 1.0f } } },
        { "Bass & Breaks / 808 Clearout (4 Bar)", {
            { "b_type", 1.0f }, { "b_slope", 1.0f }, { "sweep_on", 1.0f }, { "sweep_target", 1.0f },
            { "sweep_bars", barsIdx(4) }, { "sweep_cut_start", 2500.0f }, { "sweep_cut_end", 30.0f },
            { "sweep_cut_slope", -0.40f }, { "sweep_res_start", 0.50f }, { "sweep_res_end", 0.10f } } },
        { "Bass & Breaks / Jungle Telephone", {
            { "a_type", 2.0f }, { "a_cutoff", 1200.0f }, { "a_width", 0.40f }, { "a_lfo_rate", 4.0f },
            { "a_lfo_depth", 0.30f }, { "b_cutoff", 300.0f } } },
        { "Bass & Breaks / Reece Notch Split", {
            { "routing", 1.0f }, { "parallel_blend", 0.50f }, { "a_type", 3.0f }, { "a_slope", 2.0f },
            { "a_cutoff", 520.0f }, { "a_width", 0.42f }, { "a_lfo_rate", 8.0f }, { "a_lfo_depth", 0.50f },
            { "b_type", 0.0f }, { "b_cutoff", 2200.0f }, { "drive", 0.48f } } },
        { "Bass & Breaks / Halftime Crush", {
            { "a_type", 0.0f }, { "a_slope", 2.0f }, { "a_cutoff", 1100.0f }, { "a_reso", 0.42f },
            { "a_lfo_shape", 3.0f }, { "a_lfo_rate", 6.0f }, { "a_lfo_depth", 0.58f },
            { "crush_rate", 5200.0f }, { "crush_bits", 7.0f }, { "crush_mix", 0.42f } } },
        { "Bass & Breaks / Breakbeat Duck HP", {
            { "b_type", 1.0f }, { "b_slope", 2.0f }, { "b_cutoff", 140.0f }, { "b_env_source", 140.0f },
            { "b_env_attack", 2.0f }, { "b_env_release", 180.0f }, { "b_env_depth", 0.55f }, { "drive", 0.24f } } },
        { "Bass & Breaks / Sub Anchor Notch", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_cutoff", 240.0f }, { "a_width", 0.30f },
            { "b_type", 1.0f }, { "b_slope", 1.0f }, { "b_cutoff", 38.0f }, { "drive", 0.35f } } },
        { "Bass & Breaks / Neuro BP Morph", {
            { "a_type", 2.0f }, { "a_slope", 3.0f }, { "a_cutoff", 950.0f }, { "a_width", 0.36f },
            { "a_reso", 0.74f }, { "a_lfo_shape", 1.0f }, { "a_lfo_rate", 4.0f }, { "a_lfo_depth", 0.62f },
            { "fm_rate", 55.0f }, { "fm_depth", 0.20f }, { "drive", 0.46f } } },

        // Classic (12)
        { "Classic / Envelope Funk", {
            { "a_cutoff", 300.0f }, { "a_reso", 0.60f }, { "a_width", 0.30f }, { "a_env_source", 200.0f },
            { "a_env_attack", 3.0f }, { "a_env_release", 120.0f }, { "a_env_depth", 0.55f } } },
        { "Classic / Phaser Notch", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_cutoff", 900.0f }, { "a_width", 0.60f },
            { "a_lfo_shape", 1.0f }, { "a_lfo_rate", 9.0f }, { "a_lfo_depth", 0.40f }, { "mix", 0.60f } } },
        { "Classic / Ducking Highpass", {
            { "b_cutoff", 400.0f }, { "b_env_source", 80.0f }, { "b_env_attack", 1.0f },
            { "b_env_release", 250.0f }, { "b_env_depth", -0.45f } } },
        { "Classic / Linked Crush Sweep", {
            { "a_cutoff", 2000.0f }, { "a_lfo_rate", 9.0f }, { "a_lfo_depth", 0.40f }, { "crush_rate", 3000.0f },
            { "crush_bits", 10.0f }, { "crush_mix", 0.80f }, { "crush_link", 1.0f } } },
        { "Classic / Analog Riser", {
            { "b_cutoff", 30.0f }, { "b_lfo_shape", 2.0f }, { "b_lfo_rate", 11.0f }, { "b_lfo_depth", 0.80f },
            { "a_reso", 0.20f } } },
        { "Classic / BP Build Up (16 Bar)", {
            { "a_type", 2.0f }, { "a_slope", 1.0f }, { "a_width", 0.45f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", barsIdx(16) }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 180.0f }, { "sweep_cut_end", 9000.0f }, { "sweep_cut_slope", 0.30f },
            { "sweep_res_start", 0.30f }, { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.20f },
            { "drive", 0.30f } } },
        { "Classic / BP Sweep Down (8 Bar)", {
            { "a_type", 2.0f }, { "a_slope", 1.0f }, { "a_width", 0.45f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", barsIdx(8) }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 9000.0f }, { "sweep_cut_end", 180.0f }, { "sweep_cut_slope", -0.25f },
            { "sweep_res_start", 0.70f }, { "sweep_res_end", 0.30f }, { "sweep_res_slope", -0.15f } } },
        { "Classic / Notch Up (8 Bar)", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_width", 0.40f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", barsIdx(8) }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 120.0f }, { "sweep_cut_end", 8000.0f }, { "sweep_cut_slope", 0.20f },
            { "sweep_res_start", 0.40f }, { "sweep_res_end", 0.85f }, { "sweep_res_slope", 0.20f } } },
        { "Classic / Notch Down (4 Bar)", {
            { "a_type", 3.0f }, { "a_slope", 2.0f }, { "a_width", 0.40f }, { "sweep_on", 1.0f },
            { "sweep_target", 0.0f }, { "sweep_bars", barsIdx(4) }, { "sweep_mode", 0.0f },
            { "sweep_cut_start", 8000.0f }, { "sweep_cut_end", 120.0f }, { "sweep_cut_slope", -0.20f },
            { "sweep_res_start", 0.80f }, { "sweep_res_end", 0.35f }, { "sweep_res_slope", -0.15f } } },
        { "Classic / Dual Mirror (16 Bar)", {
            { "a_type", 0.0f }, { "a_slope", 1.0f }, { "b_type", 2.0f }, { "b_slope", 1.0f },
            { "b_width", 0.55f }, { "sweep_on", 1.0f }, { "sweep_target", 3.0f }, { "sweep_bars", barsIdx(16) },
            { "sweep_mode", 2.0f }, { "sweep_cut_start", 140.0f }, { "sweep_cut_end", 14000.0f },
            { "sweep_cut_slope", 0.20f }, { "sweep_res_start", 0.25f }, { "sweep_res_end", 0.70f },
            { "sweep_res_slope", 0.20f }, { "drive", 0.25f } } },
        { "Classic / Warm Analog LPF", {
            { "a_type", 0.0f }, { "a_slope", 0.0f }, { "a_cutoff", 2400.0f }, { "a_reso", 0.28f },
            { "drive", 0.34f }, { "drive_pos", 0.0f } } },
        { "Classic / Auto-Wah Vintage", {
            { "a_type", 2.0f }, { "a_slope", 1.0f }, { "a_cutoff", 480.0f }, { "a_width", 0.34f },
            { "a_reso", 0.68f }, { "a_env_source", 240.0f }, { "a_env_attack", 4.0f },
            { "a_env_release", 160.0f }, { "a_env_depth", 0.52f } } },
    };
    return presets;
}

const std::vector<FactoryPreset>& sweepSlopePresets() {
    static const std::vector<FactoryPreset> presets{
        // Uplifting (8) - Keep index 0 sweep_cut_start = 80.0f for unit tests
        { "Uplifting / Sweep Up (Exp)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 80.0f },
            { "sweep_cut_end", 18000.0f }, { "sweep_cut_slope", 0.35f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.68f }, { "sweep_res_slope", 0.20f } } },
        { "Uplifting / 138 Climax (16 Bar)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(16) },
            { "sweep_cut_start", 60.0f }, { "sweep_cut_end", 18500.0f }, { "sweep_cut_slope", 0.42f },
            { "sweep_res_start", 0.22f }, { "sweep_res_end", 0.85f }, { "sweep_res_slope", 0.28f },
            { "sweep_crush_start", 0.0f }, { "sweep_crush_end", 0.35f }, { "sweep_crush_onset", 0.80f },
            { "sweep_crush_slope", 0.20f } } },
        { "Uplifting / Lead Odyssey (32 Bar)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(32) },
            { "sweep_cut_start", 90.0f }, { "sweep_cut_end", 19000.0f }, { "sweep_cut_slope", 0.25f },
            { "sweep_res_start", 0.18f }, { "sweep_res_end", 0.72f }, { "sweep_res_slope", 0.15f } } },
        { "Uplifting / Euphoric Peak (8 Bar)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(8) },
            { "sweep_cut_start", 30.0f }, { "sweep_cut_end", 4800.0f }, { "sweep_cut_slope", 0.45f },
            { "sweep_res_start", 0.20f }, { "sweep_res_end", 0.78f }, { "sweep_res_slope", 0.25f },
            { "sweep_glitch_start", 0.0f }, { "sweep_glitch_end", 0.40f }, { "sweep_glitch_onset", 0.875f },
            { "sweep_glitch_slope", 0.30f } } },
        { "Uplifting / Hands-Up Pyramid (4B)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 2.0f }, { "sweep_bars", barsIdx(4) },
            { "sweep_cut_start", 110.0f }, { "sweep_cut_end", 16500.0f }, { "sweep_cut_slope", 0.25f },
            { "sweep_res_start", 0.20f }, { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.20f } } },
        { "Uplifting / Vocal Air (8 Bar)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(8) },
            { "sweep_cut_start", 150.0f }, { "sweep_cut_end", 16000.0f }, { "sweep_cut_slope", 0.30f },
            { "sweep_res_start", 0.15f }, { "sweep_res_end", 0.55f }, { "sweep_res_slope", 0.18f } } },
        { "Uplifting / HP Tension Riser", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 25.0f },
            { "sweep_cut_end", 4500.0f }, { "sweep_cut_slope", 0.45f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.78f }, { "sweep_res_slope", 0.30f } } },
        { "Uplifting / Breakdown Stutter (8B)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(8) },
            { "sweep_cut_start", 200.0f }, { "sweep_cut_end", 15000.0f }, { "sweep_cut_slope", 0.28f },
            { "sweep_res_start", 0.25f }, { "sweep_res_end", 0.65f }, { "sweep_res_slope", 0.15f },
            { "sweep_glitch_start", 0.0f }, { "sweep_glitch_end", 0.50f }, { "sweep_glitch_onset", 0.50f },
            { "sweep_glitch_slope", 0.20f } } },

        // Rolling (4)
        { "Rolling / Every-Bar Pump Up", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 1.0f }, { "sweep_cut_start", 100.0f },
            { "sweep_cut_end", 15000.0f }, { "sweep_cut_slope", 0.35f }, { "sweep_res_start", 0.25f },
            { "sweep_res_end", 0.75f }, { "sweep_res_slope", 0.20f } } },
        { "Rolling / Every-Bar Plunge", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 1.0f }, { "sweep_cut_start", 15000.0f },
            { "sweep_cut_end", 90.0f }, { "sweep_cut_slope", -0.35f }, { "sweep_res_start", 0.70f },
            { "sweep_res_end", 0.20f }, { "sweep_res_slope", -0.20f } } },
        { "Rolling / Bar + Macro Builder", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 3.0f }, { "sweep_cut_start", 80.0f },
            { "sweep_cut_end", 18500.0f }, { "sweep_cut_slope", 0.30f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.80f }, { "sweep_res_slope", 0.25f } } },
        { "Rolling / Up & Down Pyramid", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 2.0f }, { "sweep_cut_start", 90.0f },
            { "sweep_cut_end", 17000.0f }, { "sweep_cut_slope", 0.20f }, { "sweep_res_start", 0.15f },
            { "sweep_res_end", 0.70f }, { "sweep_res_slope", 0.15f } } },

        // Progressive (4)
        { "Progressive / Inversion (16 Bar)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(16) },
            { "sweep_cut_start", 16000.0f }, { "sweep_cut_end", 350.0f }, { "sweep_cut_slope", -0.32f },
            { "sweep_res_start", 0.40f }, { "sweep_res_end", 0.15f }, { "sweep_res_slope", -0.15f } } },
        { "Progressive / Clean Sweep FX", {
            { "sweep_crush_start", 0.0f }, { "sweep_crush_end", 0.0f },
            { "sweep_glitch_start", 0.0f }, { "sweep_glitch_end", 0.0f } } },
        { "Progressive / Slow Warm (16B)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(16) },
            { "sweep_cut_start", 140.0f }, { "sweep_cut_end", 14000.0f }, { "sweep_cut_slope", 0.16f },
            { "sweep_res_start", 0.12f }, { "sweep_res_end", 0.36f }, { "sweep_res_slope", 0.10f } } },
        { "Progressive / Smooth Tame (32B)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 2.0f }, { "sweep_bars", barsIdx(32) },
            { "sweep_cut_start", 180.0f }, { "sweep_cut_end", 15500.0f }, { "sweep_cut_slope", 0.12f },
            { "sweep_res_start", 0.15f }, { "sweep_res_end", 0.42f }, { "sweep_res_slope", 0.10f } } },

        // Psy (4)
        { "Psy / Acid Laser (4 Bar)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(4) },
            { "sweep_cut_start", 120.0f }, { "sweep_cut_end", 14500.0f }, { "sweep_cut_slope", 0.45f },
            { "sweep_res_start", 0.50f }, { "sweep_res_end", 0.94f }, { "sweep_res_slope", 0.35f } } },
        { "Psy / Acid Res Climb", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 110.0f },
            { "sweep_cut_end", 14000.0f }, { "sweep_cut_slope", 0.40f }, { "sweep_res_start", 0.45f },
            { "sweep_res_end", 0.92f }, { "sweep_res_slope", 0.35f } } },
        { "Psy / Final Quarter Fracture", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_crush_start", 0.0f },
            { "sweep_crush_end", 0.9f }, { "sweep_crush_onset", 0.75f }, { "sweep_crush_slope", 0.1f },
            { "sweep_glitch_start", 0.0f }, { "sweep_glitch_end", 1.0f }, { "sweep_glitch_onset", 0.80f },
            { "sweep_glitch_slope", 0.2f } } },
        { "Psy / Warp Reso Peak (8 Bar)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 2.0f }, { "sweep_bars", barsIdx(8) },
            { "sweep_cut_start", 180.0f }, { "sweep_cut_end", 11000.0f }, { "sweep_cut_slope", 0.38f },
            { "sweep_res_start", 0.55f }, { "sweep_res_end", 0.92f }, { "sweep_res_slope", 0.30f } } },

        // Tech (4)
        { "Tech / Slog Gate Sweep", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 1.0f }, { "sweep_bars", barsIdx(1) },
            { "sweep_cut_start", 150.0f }, { "sweep_cut_end", 13000.0f }, { "sweep_cut_slope", 0.38f },
            { "sweep_res_start", 0.30f }, { "sweep_res_end", 0.80f }, { "sweep_res_slope", 0.22f } } },
        { "Tech / Hardstyle Build (4 Bar)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(4) },
            { "sweep_cut_start", 80.0f }, { "sweep_cut_end", 17000.0f }, { "sweep_cut_slope", 0.45f },
            { "sweep_res_start", 0.45f }, { "sweep_res_end", 0.95f }, { "sweep_res_slope", 0.35f },
            { "sweep_crush_start", 0.0f }, { "sweep_crush_end", 0.65f }, { "sweep_crush_onset", 0.75f },
            { "sweep_crush_slope", 0.25f } } },
        { "Tech / Late Texture + Glitch", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_crush_start", 0.0f },
            { "sweep_crush_end", 0.45f }, { "sweep_crush_onset", 0.75f }, { "sweep_crush_slope", 0.25f },
            { "sweep_glitch_start", 0.0f }, { "sweep_glitch_end", 0.45f }, { "sweep_glitch_onset", 0.875f },
            { "sweep_glitch_slope", 0.35f } } },
        { "Tech / HP Bass Drop Down", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 3800.0f },
            { "sweep_cut_end", 25.0f }, { "sweep_cut_slope", -0.40f }, { "sweep_res_start", 0.65f },
            { "sweep_res_end", 0.10f }, { "sweep_res_slope", -0.25f } } },

        // Bass & Breaks (4)
        { "Bass & Breaks / Sub Reset (1 Bar)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(1) },
            { "sweep_cut_start", 18500.0f }, { "sweep_cut_end", 70.0f }, { "sweep_cut_slope", -0.48f },
            { "sweep_res_start", 0.70f }, { "sweep_res_end", 0.10f }, { "sweep_res_slope", -0.30f } } },
        { "Bass & Breaks / Drop Plunge (2B)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(2) },
            { "sweep_cut_start", 18000.0f }, { "sweep_cut_end", 90.0f }, { "sweep_cut_slope", -0.40f },
            { "sweep_res_start", 0.65f }, { "sweep_res_end", 0.15f }, { "sweep_res_slope", -0.20f } } },
        { "Bass & Breaks / Glitch Drop (4B)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(4) },
            { "sweep_cut_start", 12000.0f }, { "sweep_cut_end", 120.0f }, { "sweep_cut_slope", -0.35f },
            { "sweep_res_start", 0.55f }, { "sweep_res_end", 0.20f }, { "sweep_glitch_start", 0.0f },
            { "sweep_glitch_end", 0.75f }, { "sweep_glitch_onset", 0.75f }, { "sweep_glitch_slope", 0.30f } } },
        { "Bass & Breaks / Snare Wash (2B)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 1.0f }, { "sweep_bars", barsIdx(2) },
            { "sweep_cut_start", 120.0f }, { "sweep_cut_end", 8500.0f }, { "sweep_cut_slope", 0.42f },
            { "sweep_res_start", 0.35f }, { "sweep_res_end", 0.82f }, { "sweep_crush_start", 0.0f },
            { "sweep_crush_end", 0.50f }, { "sweep_crush_onset", 0.50f } } },

        // Classic (4)
        { "Classic / Sweep Down (Drop)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_cut_start", 18000.0f },
            { "sweep_cut_end", 80.0f }, { "sweep_cut_slope", -0.30f }, { "sweep_res_start", 0.20f },
            { "sweep_res_end", 0.60f }, { "sweep_res_slope", 0.15f } } },
        { "Classic / Linear Climb (8 Bar)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(8) },
            { "sweep_cut_start", 90.0f }, { "sweep_cut_end", 17500.0f }, { "sweep_cut_slope", 0.0f },
            { "sweep_res_start", 0.20f }, { "sweep_res_end", 0.55f }, { "sweep_res_slope", 0.0f } } },
        { "Classic / Linear Fade (8 Bar)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 0.0f }, { "sweep_bars", barsIdx(8) },
            { "sweep_cut_start", 17500.0f }, { "sweep_cut_end", 90.0f }, { "sweep_cut_slope", 0.0f },
            { "sweep_res_start", 0.50f }, { "sweep_res_end", 0.15f }, { "sweep_res_slope", 0.0f } } },
        { "Classic / Mirror Pendulum (16B)", {
            { "sweep_on", 1.0f }, { "sweep_bypass", 0.0f }, { "sweep_mode", 2.0f }, { "sweep_bars", barsIdx(16) },
            { "sweep_cut_start", 150.0f }, { "sweep_cut_end", 15000.0f }, { "sweep_cut_slope", 0.22f },
            { "sweep_res_start", 0.25f }, { "sweep_res_end", 0.72f }, { "sweep_res_slope", 0.18f } } },
    };
    return presets;
}

void populateComboMenu(juce::ComboBox& box, const std::vector<FactoryPreset>& items, bool fullItemText) {
    box.clear(juce::dontSendNotification);
    std::vector<juce::String> groups;
    std::vector<juce::PopupMenu> menus;
    std::ranges::for_each(items, [&, id = 1](const FactoryPreset& preset) mutable {
        const juce::String full(preset.name);
        const auto group = full.upToFirstOccurrenceOf(" / ", false, false);
        const auto label = full.fromFirstOccurrenceOf(" / ", false, false);
        if (label.isEmpty()) {
            box.addItem(full, id++);
            return;
        }
        const auto it = std::ranges::find(groups, group);
        const auto idx = static_cast<size_t>(std::distance(groups.begin(), it));
        if (it == groups.end()) {
            groups.push_back(group);
            menus.emplace_back();
        }
        menus[idx].addItem(id++, fullItemText ? full : label);
    });
    auto* root = box.getRootMenu();
    std::ranges::for_each(groups, [&, i = size_t{ 0 }](const juce::String& group) mutable {
        root->addSubMenu(group, menus[i++]);
    });
}

void apply(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state,
           const std::function<bool(const juce::String&)>& isLocked) {
    std::ranges::for_each(state.processor.getParameters(), [&](juce::AudioProcessorParameter* parameter) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr || (isLocked && isLocked(ranged->paramID))) {
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

void applyPartial(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state,
                  const std::function<bool(const juce::String&)>& isLocked) {
    const int slot = static_cast<int>(state.getRawParameterValue(params::kSweepSlot)->load());
    std::ranges::for_each(preset.values, [&](const auto& entry) {
        const bool perSlot = std::ranges::any_of(params::kSweepFilterIds, [&](const char* id) { return juce::String(id) == entry.first; })
            || std::ranges::any_of(params::kSweepEffectIds, [&](const char* id) { return juce::String(id) == entry.first; });
        const auto id = perSlot ? params::sweepId(slot, entry.first) : juce::String(entry.first);
        if (isLocked && isLocked(id)) {
            return;
        }
        if (auto* ranged = state.getParameter(id)) {
            ranged->setValueNotifyingHost(ranged->convertTo0to1(entry.second));
        }
    });
}

} // namespace openedm::presets
