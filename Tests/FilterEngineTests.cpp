#include "FilterEngine.h"
#include "TestSignals.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace openedm::core;
using namespace openedm::test;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr int kNumSamples = 4800;

// Both bands bypassed and the limiter off: the chain should change nothing.
EngineSettings transparentSettings() {
    EngineSettings settings;
    settings.bands[0].shape.enabled = false;
    settings.bands[1].shape.enabled = false;
    settings.limiterEnabled = false;
    return settings;
}

std::vector<float> runEngine(const EngineSettings& settings, std::vector<float> samples) {
    FilterEngine engine;
    engine.prepare(kSampleRate);
    float* channels[] = { samples.data() };
    engine.process(settings, Transport{}, channels, 1, static_cast<int>(samples.size()));
    return samples;
}

// The oversampler delays the whole chain by kLatencySamples. The first samples ring from the sine's abrupt start.
void requireDelayedCopy(const std::vector<float>& output, const std::vector<float>& input) {
    for (size_t i = FilterEngine::kLatencySamples; i + FilterEngine::kLatencySamples < output.size(); ++i) {
        REQUIRE_THAT(output[i + FilterEngine::kLatencySamples], WithinAbs(input[i], 1e-3));
    }
}

} // namespace

TEST_CASE("Engine with every stage neutral returns the input delayed by its latency", "[engine]") {
    const auto input = sine(440.0, kNumSamples);
    requireDelayedCopy(runEngine(transparentSettings(), input), input);
}

TEST_CASE("Input gain is applied before the effects and ignored during master bypass", "[engine]") {
    const auto input = sine(440.0, kNumSamples);
    auto settings = transparentSettings();
    settings.inputGain = 2.0f;
    const auto gained = runEngine(settings, input);
    for (size_t i = FilterEngine::kLatencySamples + 100; i + FilterEngine::kLatencySamples < input.size(); ++i)
        REQUIRE_THAT(gained[i], WithinAbs(input[i - FilterEngine::kLatencySamples] * 2.0f, 1e-3));

    settings.bypassed = true;
    const auto bypassed = runEngine(settings, input);
    requireDelayedCopy(bypassed, input);
}

TEST_CASE("Engine at zero mix returns the delayed dry signal", "[engine]") {
    EngineSettings settings = transparentSettings();
    settings.bands[0].shape = FilterShape{ true, FilterType::LowPass, FilterSlope::Slope96dB, 1.0f, 0.0f };
    settings.bands[0].cutoffHz = 100.0f;
    settings.drive = 1.0f;
    settings.mix = 0.0f;
    const auto input = sine(3000.0, kNumSamples);
    requireDelayedCopy(runEngine(settings, input), input);
}

TEST_CASE("Engine master bypass returns the delayed dry signal at unity gain", "[engine]") {
    EngineSettings settings = transparentSettings();
    settings.bands[0].shape = FilterShape{ true, FilterType::LowPass, FilterSlope::Slope96dB, 1.0f, 0.0f };
    settings.bands[0].cutoffHz = 100.0f;
    settings.drive = 1.0f;
    settings.outputGain = 0.25f;
    settings.limiterEnabled = true;
    settings.ceiling = 0.2f;
    settings.bypassed = true;
    const auto input = sine(3000.0, kNumSamples);
    requireDelayedCopy(runEngine(settings, input), input);
}

TEST_CASE("Engine output gain scales the signal", "[engine]") {
    EngineSettings settings = transparentSettings();
    settings.outputGain = 0.5f;
    const auto input = sine(440.0, kNumSamples);
    REQUIRE_THAT(settledRms(runEngine(settings, input)), WithinRel(settledRms(input) * 0.5f, 1e-5f));
}

TEST_CASE("Engine limiter holds a hot signal under the ceiling", "[engine]") {
    EngineSettings settings = transparentSettings();
    settings.limiterEnabled = true;
    settings.ceiling = 0.5f;
    settings.outputGain = 4.0f;
    REQUIRE(peak(runEngine(settings, sine(440.0, kNumSamples))) <= 0.5f);
}

TEST_CASE("Engine LFO synced to the transport sets the cutoff from the bar position", "[engine]") {
    EngineSettings settings = transparentSettings();
    settings.bands[0].shape.enabled = true;
    settings.bands[0].cutoffHz = 1000.0f;
    settings.bands[0].lfoBeats = 32.0;
    settings.bands[0].lfoDepth = 1.0f / FilterEngine::kModulationOctaves;
    std::vector<float> samples(kNumSamples, 0.0f);
    float* channels[] = { samples.data() };

    FilterEngine engine;
    engine.prepare(kSampleRate);
    engine.process(settings, Transport{ 120.0, 8.0 }, channels, 1, kNumSamples);

    REQUIRE_THAT(engine.currentCutoffs()[0], WithinRel(2000.0f, 0.02f));
}

TEST_CASE("Engine AutoSweep sweeps cutoff and resonance across filter types and time signatures", "[engine]") {
    for (FilterType type : { FilterType::LowPass, FilterType::HighPass, FilterType::BandPass, FilterType::Notch }) {
        for (double beatsPerBar : { 4.0, 3.0, 3.5 }) {
            EngineSettings settings = transparentSettings();
            settings.bands[0].shape = FilterShape{ true, type, FilterSlope::Slope24dB, 0.1f, 0.5f };
            settings.sweep.enabled = true;
            settings.sweep.target = SweepTarget::BandA;
            settings.sweep.bars = 8;
            settings.sweep.mode = SweepMode::BarWindow;
            settings.sweep.cutoffStartHz = 200.0f;
            settings.sweep.cutoffEndHz = 3200.0f;
            settings.sweep.cutoffSlope = 0.0f;
            settings.sweep.resonanceStart = 0.2f;
            settings.sweep.resonanceEnd = 0.8f;
            settings.sweep.resonanceSlope = 0.0f;

            std::vector<float> samples(kNumSamples, 0.0f);
            float* channels[] = { samples.data() };
            FilterEngine engine;
            engine.prepare(kSampleRate);

            // Halfway through an 8-bar window in the active time signature
            const double halfCyclePpq = 4.0 * beatsPerBar;
            engine.process(settings, Transport{ 120.0, halfCyclePpq, beatsPerBar }, channels, 1, kNumSamples);

            REQUIRE_THAT(engine.currentCutoffs()[0], WithinRel(800.0f, 0.03f));
            REQUIRE_THAT(engine.currentResonances()[0], WithinAbs(0.5f, 0.02f));

            // Bypassing the auto-sweep returns the band to its manual cutoff and resonance
            settings.sweep.enabled = false;
            settings.bands[0].cutoffHz = 1500.0f;
            settings.bands[0].shape.resonance = 0.1f;
            engine.process(settings, Transport{ 120.0, halfCyclePpq, beatsPerBar }, channels, 1, kNumSamples);
            REQUIRE_THAT(engine.currentCutoffs()[0], WithinRel(1500.0f, 0.02f));
            REQUIRE_THAT(engine.currentResonances()[0], WithinAbs(0.1f, 0.02f));
        }
    }
}

TEST_CASE("Engine cutoff FM reaches audio rate", "[engine]") {
    EngineSettings settings = transparentSettings();
    settings.bands[0].shape = FilterShape{ true, FilterType::LowPass, FilterSlope::Slope12dB, 0.0f, 0.5f };
    settings.bands[0].cutoffHz = 1000.0f;
    settings.fmRateHz = 1000.0f;
    const auto input = sine(4000.0, kNumSamples);
    const float unmodulated = settledRms(runEngine(settings, input));

    settings.fmDepth = 1.0f;
    REQUIRE(settledRms(runEngine(settings, input)) > 4.0f * unmodulated);
}

TEST_CASE("Engine oversampling keeps drive harmonics from folding back", "[engine]") {
    EngineSettings settings = transparentSettings();
    settings.drive = 0.3f;
    const auto output = runEngine(settings, sine(7000.0, kNumSamples));

    // At the host rate, the 5th harmonic (35 kHz) would fold to 13 kHz.
    const double fundamental = toneLevel(output, 7000.0);
    REQUIRE(toneLevel(output, 13000.0) < fundamental * 1e-3);
}

TEST_CASE("Engine crossfades a filter type change instead of jumping", "[engine]") {
    EngineSettings settings = transparentSettings();
    settings.bands[0].shape = FilterShape{ true, FilterType::LowPass, FilterSlope::Slope24dB, 0.0f, 0.5f };
    settings.bands[0].cutoffHz = 1000.0f;
    auto samples = sine(100.0, kNumSamples);
    float* channels[] = { samples.data() };

    FilterEngine engine;
    engine.prepare(kSampleRate);
    engine.process(settings, Transport{}, channels, 1, kNumSamples / 2);
    settings.bands[0].shape.type = FilterType::HighPass;
    settings.bands[0].shape.slope = FilterSlope::Slope96dB;
    float* secondHalf[] = { samples.data() + kNumSamples / 2 };
    engine.process(settings, Transport{}, secondHalf, 1, kNumSamples / 2);

    // A 0.5 amplitude 100 Hz sine moves at most 0.0066 per sample at 48 kHz.
    float largestStep = 0.0f;
    for (size_t i = kNumSamples / 4; i + 1 < samples.size(); ++i) {
        largestStep = std::max(largestStep, std::abs(samples[i + 1] - samples[i]));
    }
    REQUIRE(largestStep < 0.01f);
}

TEST_CASE("Engine smooths output gain over 10 ms at the host rate", "[engine]") {
    EngineSettings settings = transparentSettings();
    std::vector<float> samples(4000, 1.0f);
    float* channels[] = { samples.data() };
    FilterEngine engine;
    engine.prepare(kSampleRate);
    engine.process(settings, Transport{}, channels, 1, 1000);
    settings.outputGain = 0.0f;
    float* rest[] = { samples.data() + 1000 };
    engine.process(settings, Transport{}, rest, 1, 3000);

    // One time constant (480 samples at 48 kHz) after the change, a one-pole smoother sits at 1/e.
    const auto afterOneTimeConstant = samples[1000 + FilterEngine::kLatencySamples + 480];
    REQUIRE_THAT(afterOneTimeConstant, WithinAbs(std::exp(-1.0), 0.03));
}

namespace {

void requireWrappedPhase(float actual, double expected) {
    const double wrapped = expected - std::floor(expected);
    const double delta = std::abs(static_cast<double>(actual) - wrapped);
    REQUIRE(std::min(delta, 1.0 - delta) < 1e-5);
}

int samplesForBeats(double beats, double bpm) {
    return static_cast<int>(std::llround(beats * 60.0 / bpm * kSampleRate));
}

void processSilence(FilterEngine& engine, const EngineSettings& settings, const Transport& transport, int numSamples) {
    std::vector<float> samples(static_cast<size_t>(numSamples), 0.0f);
    float* channels[] = { samples.data() };
    engine.process(settings, transport, channels, 1, numSamples);
}

} // namespace

TEST_CASE("Engine bar frames advance with tempo and hold or snap with the host timeline", "[engine][autosweep]") {
    for (int bars : { 1, 2, 4 }) {
        EngineSettings settings = transparentSettings();
        settings.sweep.enabled = true;
        settings.sweep.target = SweepTarget::BandA;
        settings.sweep.bars = bars;
        settings.sweep.mode = SweepMode::BarWindow;
        settings.sweep.cutoffStartHz = 200.0f;
        settings.sweep.cutoffEndHz = 3200.0f;
        settings.bands[0].shape = FilterShape{ true, FilterType::LowPass, FilterSlope::Slope24dB, 0.2f, 0.5f };

        const double beatsPerBar = 4.0;
        const double totalBeats = beatsPerBar * static_cast<double>(bars);
        FilterEngine engine;
        engine.prepare(kSampleRate);

        const int oneSecond = samplesForBeats(2.0, 120.0);
        processSilence(engine, settings, Transport{ 120.0, std::nullopt, beatsPerBar, true }, oneSecond);
        requireWrappedPhase(engine.currentSweepPhase(), 2.0 / totalBeats);

        processSilence(engine, settings, Transport{ 240.0, std::nullopt, beatsPerBar, true }, oneSecond);
        requireWrappedPhase(engine.currentSweepPhase(), 2.0 / totalBeats + 4.0 / totalBeats);

        processSilence(engine, settings, Transport{ 60.0, std::nullopt, beatsPerBar, true }, oneSecond);
        requireWrappedPhase(engine.currentSweepPhase(), 2.0 / totalBeats + 4.0 / totalBeats + 1.0 / totalBeats);

        // A stopped playhead holds the phase for the whole block. The internal clock must not keep drifting.
        const double heldPpq = 0.5 * beatsPerBar;
        processSilence(engine, settings, Transport{ 120.0, heldPpq, beatsPerBar, false }, oneSecond);
        requireWrappedPhase(engine.currentSweepPhase(), heldPpq / totalBeats);
        const float held = engine.currentSweepPhase();
        processSilence(engine, settings, Transport{ 240.0, heldPpq, beatsPerBar, false }, oneSecond);
        REQUIRE_THAT(engine.currentSweepPhase(), WithinAbs(held, 1e-6));

        // Starting or relocating snaps to the playhead, then advances through the block.
        const int halfBar = samplesForBeats(beatsPerBar * 0.5, 120.0);
        processSilence(engine, settings, Transport{ 120.0, 0.0, beatsPerBar, true }, halfBar);
        requireWrappedPhase(engine.currentSweepPhase(), 0.5 / static_cast<double>(bars));

        // Changing the frame while the host is stopped re-grids to the new length.
        settings.sweep.bars = 1;
        processSilence(engine, settings, Transport{ 120.0, beatsPerBar, beatsPerBar, false }, 64);
        requireWrappedPhase(engine.currentSweepPhase(), 0.0);
        settings.sweep.bars = bars;
    }
}

TEST_CASE("Engine 1, 2 and 4 bar modes land on the musical cutoff while the host is stopped", "[engine][autosweep]") {
    const auto run = [](int bars, SweepMode mode, double ppq, double beatsPerBar) {
        EngineSettings settings = transparentSettings();
        settings.bands[0].shape = FilterShape{ true, FilterType::LowPass, FilterSlope::Slope24dB, 0.1f, 0.5f };
        settings.sweep.enabled = true;
        settings.sweep.target = SweepTarget::BandA;
        settings.sweep.bars = bars;
        settings.sweep.mode = mode;
        settings.sweep.cutoffStartHz = 100.0f;
        settings.sweep.cutoffEndHz = 1600.0f;
        settings.sweep.resonanceStart = 0.2f;
        settings.sweep.resonanceEnd = 0.8f;
        FilterEngine engine;
        engine.prepare(kSampleRate);
        processSilence(engine, settings, Transport{ 100.0, ppq, beatsPerBar, false }, 2000);
        return engine;
    };

    // 4/4, halfway through a 1-bar window.
    {
        const auto engine = run(1, SweepMode::BarWindow, 2.0, 4.0);
        REQUIRE_THAT(engine.currentCutoffs()[0], WithinRel(400.0f, 1e-3f));
        REQUIRE_THAT(engine.currentResonances()[0], WithinAbs(0.5f, 1e-3f));
    }
    // 3/4, end of the outward half of a 1-bar up/down.
    {
        const auto engine = run(1, SweepMode::UpDown, 1.5, 3.0);
        REQUIRE_THAT(engine.currentCutoffs()[0], WithinRel(1600.0f, 1e-3f));
    }
    // 6/8 is 3 quarter notes. Halfway through bar 1 of a 2-bar Every Bar loop.
    {
        const auto engine = run(2, SweepMode::EveryBar, 1.5, 3.0);
        REQUIRE_THAT(engine.currentCutoffs()[0], WithinRel(400.0f, 1e-3f));
        REQUIRE_THAT(engine.currentSweepPhase(), WithinAbs(0.25f, 1e-5f));
    }
    // 4 bars, 5/4: two beats is 2/20 of the window.
    {
        const auto engine = run(4, SweepMode::BarWindow, 2.0, 5.0);
        REQUIRE_THAT(engine.currentSweepPhase(), WithinAbs(0.1f, 1e-5f));
    }
    // No playhead still runs, even if the playing flag is false.
    {
        EngineSettings settings = transparentSettings();
        settings.sweep.bars = 1;
        FilterEngine engine;
        engine.prepare(kSampleRate);
        processSilence(engine, settings, Transport{ 120.0, std::nullopt, 4.0, false }, samplesForBeats(2.0, 120.0));
        requireWrappedPhase(engine.currentSweepPhase(), 0.5);
    }
}

TEST_CASE("Parallel routing blends independently filtered input", "[approved][engine]") {
    auto settings = transparentSettings();
    settings.bands[0].shape.enabled = true;
    settings.bands[0].cutoffHz = 100.0f;
    settings.parallel = true;
    settings.parallelBlend = 1.0f;
    const auto input = sine(3000.0, kNumSamples);
    requireDelayedCopy(runEngine(settings, input), input);
    settings.parallelBlend = 0.0f;
    REQUIRE(settledRms(runEngine(settings, input)) < 0.01f);
    settings.parallelBlend = 0.5f;
    REQUIRE_THAT(settledRms(runEngine(settings, input)), WithinAbs(settledRms(input) * 0.5f, 0.005f));
}

TEST_CASE("FM shapes target either cutoff without changing the other", "[approved][engine]") {
    auto settings = transparentSettings();
    settings.fmDepth = 1.0f;
    settings.fmShape = LfoShape::Square;
    settings.fmTarget = ModTarget::BandA;
    auto input = sine(440.0, kNumSamples);
    FilterEngine engine;
    engine.prepare(kSampleRate);
    float* channels[]{input.data()};
    engine.process(settings, {}, channels, 1, kNumSamples);
    REQUIRE_THAT(engine.currentLaneCutoffs(0)[0], WithinRel(8000.0f, 0.01f));
    REQUIRE_THAT(engine.currentLaneCutoffs(0)[1], WithinRel(1000.0f, 0.01f));
}

TEST_CASE("MIDI and step sources modulate cutoff with bounded independent stereo depths", "[approved][engine]") {
    auto settings = transparentSettings();
    settings.stereoMode = StereoMode::LeftRight;
    settings.bands[0].midiSource = MidiSource::Velocity;
    settings.bands[0].midiDepth = 1.0f / 6.0f;
    settings.bands[0].lane2Depth = -1.0f;
    FilterEngine engine;
    engine.prepare(kSampleRate);
    engine.setMidiValue(MidiSource::Velocity, 1.0f);
    auto left = sine(440.0, kNumSamples), right = left;
    float* channels[]{left.data(), right.data()};
    engine.process(settings, {}, channels, 2, kNumSamples);
    REQUIRE_THAT(engine.currentLaneCutoffs(0)[0], WithinRel(2000.0f, 0.01f));
    REQUIRE_THAT(engine.currentLaneCutoffs(1)[0], WithinRel(500.0f, 0.01f));
    engine.reset();
    engine.process(settings, {}, channels, 2, kNumSamples);
    REQUIRE_THAT(engine.currentLaneCutoffs(0)[0], WithinRel(1000.0f, 0.01f));
    settings.bands[0].step.mode = StepMode::Steps;
    settings.bands[0].step.values.fill(1.0f);
    settings.bands[0].step.depth = 1.0f / 6.0f;
    engine.process(settings, {120.0, 0.0, 4.0, false}, channels, 2, kNumSamples);
    REQUIRE_THAT(engine.currentLaneCutoffs(0)[0], WithinRel(2000.0f, 0.01f));
}

TEST_CASE("M/S processing preserves mono when side modulation differs", "[approved][engine]") {
    auto settings = transparentSettings();
    settings.stereoMode = StereoMode::MidSide;
    settings.bands[0].shape.enabled = true;
    settings.bands[0].lfoShape = LfoShape::Square;
    settings.bands[0].lfoDepth = 0.2f;
    settings.bands[0].lane2Depth = -1.0f;
    auto left = sine(440.0, kNumSamples), right = left;
    float* channels[]{left.data(), right.data()};
    FilterEngine engine;
    engine.prepare(kSampleRate);
    engine.process(settings, {}, channels, 2, kNumSamples);
    for (size_t i = 0; i < left.size(); ++i) REQUIRE_THAT(left[i], WithinAbs(right[i], 1e-5f));
}

TEST_CASE("Step modulation locks to transport, holds stopped and is block independent", "[approved][engine]") {
    auto settings = transparentSettings();
    settings.bands[0].step.mode = StepMode::Steps;
    settings.bands[0].step.length = 4;
    settings.bands[0].step.beatsPerStep = 0.25;
    settings.bands[0].step.values = { -1, 0, 1, 0 };
    settings.bands[0].step.depth = 1.0f / 6.0f;
    const auto render = [&](int blockSize) {
        auto audio = sine(1000.0, 24000);
        FilterEngine engine;
        engine.prepare(kSampleRate);
        for (int offset = 0; offset < static_cast<int>(audio.size()); offset += blockSize) {
            float* channels[]{audio.data() + offset};
            engine.process(settings, {120.0, offset / kSampleRate * 2.0}, channels, 1,
                std::min(blockSize, static_cast<int>(audio.size()) - offset));
        }
        return audio;
    };
    const auto whole = render(24000), split = render(127);
    for (size_t i = 0; i < whole.size(); ++i) REQUIRE_THAT(split[i], WithinAbs(whole[i], 1e-6f));
    auto audio = sine(440, kNumSamples);
    float* channels[]{audio.data()};
    FilterEngine engine;
    engine.prepare(kSampleRate);
    engine.process(settings, {120, 0.5, 4, false}, channels, 1, kNumSamples);
    REQUIRE_THAT(engine.currentCutoffs()[0], WithinRel(2000.0f, 0.01f));
    engine.process(settings, {120, -1.0, 4, false}, channels, 1, kNumSamples);
    REQUIRE_THAT(engine.currentCutoffs()[0], WithinRel(500.0f, 0.01f));
}

TEST_CASE("Independent stereo envelope followers listen to their own channel", "[approved][engine]") {
    auto settings = transparentSettings();
    settings.stereoMode = StereoMode::LeftRight;
    settings.bands[0].envDepth = 0.2f;
    auto left = sine(1000.0, 24000);
    std::vector<float> right(left.size());
    float* channels[]{left.data(), right.data()};
    FilterEngine engine;
    engine.prepare(kSampleRate);
    engine.process(settings, {}, channels, 2, static_cast<int>(left.size()));
    REQUIRE(engine.currentLaneCutoffs(0)[0] > 1100.0f);
    REQUIRE_THAT(engine.currentLaneCutoffs(1)[0], WithinAbs(1000.0f, 0.1f));
}

TEST_CASE("Independent L/R and M/S cutoffs change the intended audio lane", "[approved][engine]") {
    auto settings = transparentSettings();
    settings.bands[0].shape.enabled = true;
    settings.bands[0].cutoffHz = 1000.0f;
    settings.bands[0].lane2Offset = 3.0f;
    const auto renderStereo = [&](StereoMode mode, bool opposite) {
        auto left = sine(3000.0, kNumSamples), right = left;
        if (opposite) std::ranges::transform(right, right.begin(), [](float x) { return -x; });
        float* channels[]{left.data(), right.data()};
        FilterEngine engine;
        engine.prepare(kSampleRate);
        settings.stereoMode = mode;
        engine.process(settings, {}, channels, 2, kNumSamples);
        return std::array{settledRms(left), settledRms(right)};
    };
    const auto lr = renderStereo(StereoMode::LeftRight, false);
    REQUIRE(lr[1] > lr[0] * 5.0f);
    const auto mid = renderStereo(StereoMode::MidSide, false);
    const auto side = renderStereo(StereoMode::MidSide, true);
    REQUIRE(side[0] > mid[0] * 5.0f);
    REQUIRE_THAT(mid[0], WithinAbs(mid[1], 1e-6f));
    REQUIRE_THAT(side[0], WithinAbs(side[1], 1e-6f));
}

TEST_CASE("Routing and stereo automation stay finite under combined modulation", "[approved][engine]") {
    EngineSettings settings;
    settings.fmDepth = 0.5f;
    settings.fmRateHz = 2000.0f;
    settings.ceiling = 0.8f;
    settings.bands[0].lfoDepth = 0.1f;
    settings.bands[0].midiSource = MidiSource::CC;
    settings.bands[0].midiDepth = 0.1f;
    settings.bands[0].step.mode = StepMode::Euclidean;
    settings.bands[0].step.depth = 0.1f;
    settings.bands[0].lane2Depth = -1.0f;
    settings.bands[0].lane2Offset = 3.0f;
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        FilterEngine engine;
        engine.prepare(rate);
        for (int block = 0; block < 40; ++block) {
            settings.parallel = block % 2 == 0;
            settings.parallelBlend = block % 3 == 0 ? 0.0f : 1.0f;
            settings.stereoMode = static_cast<StereoMode>(block % 3);
            settings.fmShape = static_cast<LfoShape>(block % 5);
            settings.fmTarget = static_cast<ModTarget>(block % 3);
            engine.setMidiValue(MidiSource::CC, block % 2 == 0 ? 0.0f : 1.0f);
            auto left = sine(2000.0, 128), right = sine(3000.0, 128);
            float* channels[]{left.data(), right.data()};
            engine.process(settings, {}, channels, 2, 128);
            for (const auto& audio : {left, right})
                REQUIRE(std::ranges::all_of(audio, [&](float value) { return std::isfinite(value) && std::abs(value) <= settings.ceiling; }));
        }
    }
}

TEST_CASE("Sidechain envelope follows the sidechain and falls back when it is absent", "[engine]") {
    auto settings = transparentSettings();
    settings.bands[0].cutoffHz = 1000.0f;
    settings.bands[0].envDepth = 0.2f;
    settings.bands[0].envListen = EnvelopeListen::Sidechain;
    constexpr int length = 12000;

    auto silent = std::vector<float>(static_cast<size_t>(length), 0.0f);
    auto loud = sine(1000.0, length);
    float* loudSide[] { loud.data() };
    float* silentSide[] { silent.data() };

    FilterEngine fromSidechain;
    fromSidechain.prepare(kSampleRate);
    auto main = silent;
    float* mainChannels[] { main.data() };
    fromSidechain.process(settings, {}, mainChannels, 1, length, loudSide, 1);
    REQUIRE(fromSidechain.currentCutoffs()[0] > 1100.0f);

    FilterEngine fallback;
    fallback.prepare(kSampleRate);
    auto fallbackMain = loud;
    float* fallbackChannels[] { fallbackMain.data() };
    fallback.process(settings, {}, fallbackChannels, 1, length, nullptr, 0);
    REQUIRE(fallback.currentCutoffs()[0] > 1100.0f);

    FilterEngine connectedSilence;
    connectedSilence.prepare(kSampleRate);
    auto loudMain = loud;
    float* loudMainChannels[] { loudMain.data() };
    connectedSilence.process(settings, {}, loudMainChannels, 1, length, silentSide, 1);
    REQUIRE_THAT(connectedSilence.currentCutoffs()[0], WithinAbs(1000.0f, 0.5f));

    settings.bands[0].envListen = EnvelopeListen::Main;
    FilterEngine mainOnly;
    mainOnly.prepare(kSampleRate);
    auto quiet = silent;
    float* quietChannels[] { quiet.data() };
    mainOnly.process(settings, {}, quietChannels, 1, length, loudSide, 1);
    REQUIRE_THAT(mainOnly.currentCutoffs()[0], WithinAbs(1000.0f, 0.5f));
}

TEST_CASE("Engine true-peak guard keeps inter-sample peaks under the ceiling after decimation", "[engine][limiter][dsp]") {
    auto settings = transparentSettings();
    settings.limiterEnabled = true;
    settings.ceiling = 0.5f;
    settings.outputGain = 4.0f;
    const auto output = runEngine(settings, sine(7000.0, kNumSamples, 0.9f));
    REQUIRE(peak(output) <= 0.5f);
    // Cubic midpoint estimate between adjacent samples approximates the inter-sample true peak.
    float maxMidpoint = 0.0f;
    std::ranges::for_each(std::views::iota(size_t{ 2400 }, output.size() - 2), [&](size_t i) {
        const float mid = std::abs(0.5625f * (output[i] + output[i + 1]) - 0.0625f * (output[i - 1] + output[i + 2]));
        maxMidpoint = std::max(maxMidpoint, mid);
    });
    REQUIRE(maxMidpoint <= 0.52f);
}

TEST_CASE("Sub preserve LR4 crossover has no phase-cancellation notch in the crossover band", "[engine][sub]") {
    auto settings = transparentSettings();
    settings.bands[0].shape = FilterShape{ true, FilterType::LowPass, FilterSlope::Slope24dB, 0.0f, 0.5f };
    settings.bands[0].cutoffHz = 900.0f;
    settings.subPreserveHz = 120.0f;
    const auto gainAtHz = [&](double hz) {
        const auto input = sine(hz, kNumSamples);
        return settledRms(runEngine(settings, input)) / settledRms(input);
    };
    REQUIRE(gainAtHz(60.0) > 0.85f);
    REQUIRE(gainAtHz(120.0) > 0.8f);
    REQUIRE(gainAtHz(180.0) > 0.7f);
}

TEST_CASE("Sidechain envelope attack and release match the main envelope timing", "[engine][sidechain]") {
    const auto cutoffAfter = [](bool sidechainFeed) {
        auto settings = transparentSettings();
        settings.bands[0].cutoffHz = 1000.0f;
        settings.bands[0].envDepth = 0.2f;
        settings.bands[0].envAttackMs = 5.0f;
        settings.bands[0].envReleaseMs = 150.0f;
        settings.bands[0].envListen = sidechainFeed ? EnvelopeListen::Sidechain : EnvelopeListen::Main;
        FilterEngine engine;
        engine.prepare(kSampleRate);
        auto loud = sine(1000.0, 480);
        auto silent = std::vector<float>(480, 0.0f);
        auto mainBuf = sidechainFeed ? silent : loud;
        float* mainCh[] { mainBuf.data() };
        float* sideCh[] { loud.data() };
        engine.process(settings, {}, mainCh, 1, 480, sidechainFeed ? sideCh : nullptr, sidechainFeed ? 1 : 0);
        auto decayMain = std::vector<float>(2400, 0.0f);
        auto decaySide = sine(1000.0, 2400, 0.0f);
        float* decayMainCh[] { decayMain.data() };
        float* decaySideCh[] { decaySide.data() };
        engine.process(settings, {}, decayMainCh, 1, 2400, sidechainFeed ? decaySideCh : nullptr, sidechainFeed ? 1 : 0);
        return engine.currentCutoffs()[0];
    };
    const float mainCutoff = cutoffAfter(false);
    const float sideCutoff = cutoffAfter(true);
    REQUIRE_THAT(sideCutoff, WithinRel(mainCutoff, 0.05f));
}

TEST_CASE("Serial to parallel routing morph has no comb hollow at the crossover point", "[engine][routing]") {
    auto settings = transparentSettings();
    settings.bands[0].shape = FilterShape{ true, FilterType::LowPass, FilterSlope::Slope24dB, 0.2f, 0.5f };
    settings.bands[0].cutoffHz = 1000.0f;
    settings.bands[1].shape = FilterShape{ true, FilterType::HighPass, FilterSlope::Slope24dB, 0.2f, 0.5f };
    settings.bands[1].cutoffHz = 1000.0f;
    settings.parallelBlend = 0.5f;
    auto samples = sine(1000.0, 48000, 0.4f);
    FilterEngine engine;
    engine.prepare(kSampleRate);
    float* first[] { samples.data() };
    engine.process(settings, {}, first, 1, 24000);
    settings.parallel = true;
    float* second[] { samples.data() + 24000 };
    engine.process(settings, {}, second, 1, 24000);
    // Minimum 480-sample window RMS during the morph, relative to the settled parallel RMS.
    const auto windowRms = [&](size_t start) {
        float sum = 0.0f;
        std::ranges::for_each(std::views::iota(start, start + size_t{ 480 }), [&](size_t i) { sum += samples[i] * samples[i]; });
        return std::sqrt(sum / 480.0f);
    };
    float minWindow = 1.0f;
    std::ranges::for_each(std::views::iota(size_t{ 24000 }, size_t{ 28800 }), [&](size_t start) {
        if (start % 240 == 0) minWindow = std::min(minWindow, windowRms(start));
    });
    const float settled = windowRms(47000);
    REQUIRE(settled > 0.05f);
    REQUIRE(minWindow > settled * 0.55f);
}

TEST_CASE("Topology change crossfade is equal-power and state-warmed with no loudness dip or thump", "[engine][fade]") {
    auto settings = transparentSettings();
    settings.bands[0].shape = FilterShape{ true, FilterType::LowPass, FilterSlope::Slope12dB, 0.0f, 0.5f };
    settings.bands[0].cutoffHz = 200.0f;
    auto samples = sine(100.0, 48000, 0.4f);
    FilterEngine engine;
    engine.prepare(kSampleRate);
    float* first[] { samples.data() };
    engine.process(settings, {}, first, 1, 24000);
    settings.bands[0].shape.slope = FilterSlope::Slope96dB;
    float* second[] { samples.data() + 24000 };
    engine.process(settings, {}, second, 1, 24000);
    const auto windowRms = [&](size_t start) {
        float sum = 0.0f;
        std::ranges::for_each(std::views::iota(start, start + size_t{ 480 }), [&](size_t i) { sum += samples[i] * samples[i]; });
        return std::sqrt(sum / 480.0f);
    };
    const float steady = windowRms(20000);
    float minWindow = steady;
    std::ranges::for_each(std::views::iota(size_t{ 24000 }, size_t{ 26400 }), [&](size_t start) {
        if (start % 120 == 0) minWindow = std::min(minWindow, windowRms(start));
    });
    REQUIRE(minWindow > steady * 0.7f);
    float largestStep = 0.0f;
    std::ranges::for_each(std::views::iota(size_t{ 24000 }, size_t{ 28800 }), [&](size_t i) {
        largestStep = std::max(largestStep, std::abs(samples[i + 1] - samples[i]));
    });
    REQUIRE(largestStep < 0.01f);
}
