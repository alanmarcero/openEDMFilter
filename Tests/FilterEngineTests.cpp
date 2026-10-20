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
