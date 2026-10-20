#include "Oversampler.h"
#include "TestSignals.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace openedm::core;
using namespace openedm::test;
using Catch::Matchers::WithinAbs;

namespace {

std::vector<float> roundTrip(const std::vector<float>& input) {
    Oversampler oversampler;
    std::vector<float> output(input.size());
    std::ranges::transform(input, output.begin(),
        [&](float sample) { return oversampler.downsample(0, oversampler.upsample(0, sample)); });
    return output;
}

} // namespace

TEST_CASE("Oversampler round trip is a pure delay of kLatency samples", "[oversampler]") {
    const auto input = sine(1000.0, 4800);
    const auto output = roundTrip(input);
    for (size_t i = Oversampler::kLatency; i + Oversampler::kLatency < input.size(); ++i) {
        REQUIRE_THAT(output[i + Oversampler::kLatency], WithinAbs(input[i], 1e-4));
    }
}

TEST_CASE("Oversampler passband is flat to 19 kHz at 48 kHz", "[oversampler]") {
    const auto input = sine(19000.0, 9600);
    REQUIRE_THAT(settledRms(roundTrip(input)) / settledRms(input), WithinAbs(1.0, 0.01));
}

TEST_CASE("Oversampler upsampling rejects the image above the host Nyquist", "[oversampler]") {
    Oversampler oversampler;
    const auto input = sine(5000.0, 9600);
    std::vector<float> upsampled;
    std::ranges::for_each(input, [&](float sample) {
        const auto frame = oversampler.upsample(0, sample);
        upsampled.insert(upsampled.end(), frame.begin(), frame.end());
    });
    // At 96 kHz the image of 5 kHz is at 43 kHz. toneLevel assumes 48 kHz, so read both at half frequency.
    const double tone = toneLevel(upsampled, 2500.0);
    const double image = toneLevel(upsampled, 21500.0);
    REQUIRE(image < tone * 1e-3);
}
