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
