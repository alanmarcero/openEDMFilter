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

} // namespace

TEST_CASE("Engine with every stage neutral returns the input unchanged", "[engine]") {
    const auto input = sine(440.0, kNumSamples);
    const auto output = runEngine(transparentSettings(), input);
    REQUIRE(output == input);
}

TEST_CASE("Engine at zero mix returns the dry signal", "[engine]") {
    EngineSettings settings = transparentSettings();
    settings.bands[0].shape = FilterShape{ true, FilterType::LowPass, FilterSlope::Slope96dB, 1.0f, 0.0f };
    settings.bands[0].cutoffHz = 100.0f;
    settings.drive = 1.0f;
    settings.mix = 0.0f;
    const auto input = sine(3000.0, kNumSamples);
    REQUIRE(runEngine(settings, input) == input);
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
