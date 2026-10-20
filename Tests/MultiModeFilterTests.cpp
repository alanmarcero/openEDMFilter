#include "MultiModeFilter.h"
#include "TestSignals.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace openedm::core;
using namespace openedm::test;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr int kNumSamples = 48000;

std::vector<float> runFilter(const FilterShape& shape, double cutoffHz, std::vector<float> samples) {
    MultiModeFilter filter;
    std::ranges::for_each(samples, [&](float& sample) {
        filter.update(shape, cutoffHz, kSampleRate);
        sample = filter.process(0, sample);
    });
    return samples;
}

float gainAt(const FilterShape& shape, double cutoffHz, double frequencyHz) {
    const auto input = sine(frequencyHz, kNumSamples);
    return settledRms(runFilter(shape, cutoffHz, input)) / settledRms(input);
}

} // namespace

TEST_CASE("Low-pass passes content well below the cutoff", "[filter]") {
    const FilterShape shape{ true, FilterType::LowPass, FilterSlope::Slope24dB, 0.0f, 0.5f };
    REQUIRE_THAT(gainAt(shape, 2000.0, 100.0), WithinAbs(1.0, 0.02));
}

TEST_CASE("24 dB low-pass attenuates two octaves above the cutoff by more than 40 dB", "[filter]") {
    const FilterShape shape{ true, FilterType::LowPass, FilterSlope::Slope24dB, 0.0f, 0.5f };
    REQUIRE(gainAt(shape, 1000.0, 4000.0) < 0.01f);
}

TEST_CASE("96 dB slope attenuates one octave above the cutoff far more than 12 dB", "[filter]") {
    const FilterShape gentle{ true, FilterType::LowPass, FilterSlope::Slope12dB, 0.0f, 0.5f };
    const FilterShape steep{ true, FilterType::LowPass, FilterSlope::Slope96dB, 0.0f, 0.5f };
    REQUIRE(gainAt(gentle, 1000.0, 2000.0) > 0.2f);
    REQUIRE(gainAt(steep, 1000.0, 2000.0) < 0.005f);
}

TEST_CASE("High-pass removes content below the cutoff", "[filter]") {
    const FilterShape shape{ true, FilterType::HighPass, FilterSlope::Slope48dB, 0.0f, 0.5f };
    REQUIRE(gainAt(shape, 1000.0, 250.0) < 0.001f);
    REQUIRE_THAT(gainAt(shape, 1000.0, 8000.0), WithinAbs(1.0, 0.02));
}

TEST_CASE("Band-pass keeps the cutoff frequency at unity gain", "[filter]") {
    const FilterShape shape{ true, FilterType::BandPass, FilterSlope::Slope12dB, 0.0f, 0.5f };
    REQUIRE_THAT(gainAt(shape, 1000.0, 1000.0), WithinAbs(1.0, 0.03));
}

TEST_CASE("Notch removes the cutoff frequency", "[filter]") {
    const FilterShape shape{ true, FilterType::Notch, FilterSlope::Slope24dB, 0.0f, 0.5f };
    REQUIRE(gainAt(shape, 1000.0, 1000.0) < 0.01f);
}

TEST_CASE("Full resonance boosts the cutoff frequency", "[filter]") {
    const FilterShape shape{ true, FilterType::LowPass, FilterSlope::Slope12dB, 1.0f, 0.0f };
    REQUIRE(gainAt(shape, 1000.0, 1000.0) > 3.0f);
}

TEST_CASE("Bypassed filter returns the input unchanged", "[filter]") {
    const FilterShape shape{ false, FilterType::LowPass, FilterSlope::Slope96dB, 1.0f, 0.0f };
    const auto input = sine(5000.0, 256);
    REQUIRE(runFilter(shape, 100.0, input) == input);
}

TEST_CASE("Bypassing a filter crossfades to the dry input instead of jumping", "[filter]") {
    const FilterShape active{ true, FilterType::LowPass, FilterSlope::Slope24dB, 0.0f, 0.5f };
    const FilterShape bypassed{ false, FilterType::LowPass, FilterSlope::Slope24dB, 0.0f, 0.5f };
    const auto input = sine(5000.0, 2400);
    MultiModeFilter filter;
    std::ranges::for_each(input, [&](float sample) {
        filter.update(active, 200.0, kSampleRate);
        (void)filter.process(0, sample);
    });

    filter.update(bypassed, 200.0, kSampleRate);
    const float firstBypassedSample = filter.process(0, input[100]);
    std::ranges::for_each(input.begin(), input.begin() + 480, [&](float sample) {
        filter.update(bypassed, 200.0, kSampleRate);
        (void)filter.process(0, sample);
    });
    filter.update(bypassed, 200.0, kSampleRate);
    const float settledSample = filter.process(0, input[100]);

    REQUIRE(std::abs(firstBypassedSample - input[100]) > 0.1f);
    REQUIRE(settledSample == input[100]);
}

TEST_CASE("Response of a 12 dB low-pass is -3 dB at the cutoff", "[filter]") {
    const FilterShape shape{ true, FilterType::LowPass, FilterSlope::Slope12dB, 0.0f, 0.5f };
    REQUIRE_THAT(MultiModeFilter::magnitudeAt(shape, 1000.0, 1000.0, kSampleRate), WithinRel(1.0 / std::sqrt(2.0), 1e-6));
}

TEST_CASE("Response of a bypassed filter is flat", "[filter]") {
    const FilterShape shape{ false, FilterType::HighPass, FilterSlope::Slope96dB, 0.0f, 0.5f };
    REQUIRE(MultiModeFilter::magnitudeAt(shape, 1000.0, 50.0, kSampleRate) == 1.0);
}

TEST_CASE("Drawn response matches the measured response near Nyquist", "[filter]") {
    const FilterShape lowPass{ true, FilterType::LowPass, FilterSlope::Slope24dB, 0.6f, 0.4f };
    REQUIRE_THAT(gainAt(lowPass, 10000.0, 16000.0), WithinRel(MultiModeFilter::magnitudeAt(lowPass, 10000.0, 16000.0, kSampleRate), 0.02));
    REQUIRE_THAT(gainAt(lowPass, 10000.0, 10000.0), WithinRel(MultiModeFilter::magnitudeAt(lowPass, 10000.0, 10000.0, kSampleRate), 0.02));

    const FilterShape bandPass{ true, FilterType::BandPass, FilterSlope::Slope48dB, 0.0f, 0.8f };
    REQUIRE_THAT(gainAt(bandPass, 6000.0, 15000.0), WithinRel(MultiModeFilter::magnitudeAt(bandPass, 6000.0, 15000.0, kSampleRate), 0.02));
}
