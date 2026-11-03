#include "Effects.h"
#include "TestSignals.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace openedm::core;
using namespace openedm::test;
using Catch::Matchers::WithinAbs;

TEST_CASE("Saturation with zero drive returns the input unchanged", "[saturate]") {
    REQUIRE(saturate(0.37f, 0.0f) == 0.37f);
}

TEST_CASE("Beat repeat uses the host tempo even outside the manual tempo range", "[stutter]") {
    for (const auto [bpm, subdivisions] : { std::pair{ 15.0, 8 }, { 30.0, 4 }, { 120.0, 4 }, { 480.0, 4 } }) {
        BeatStutter stutter;
        stutter.prepare(8000.0);
        const int sliceSamples = static_cast<int>(8000.0 * 60.0 / bpm / subdivisions);
        for (int i = 0; i < sliceSamples * 4; ++i) {
            const double phase = static_cast<double>(i) * bpm / 60.0 / 8000.0;
            const float input = static_cast<float>(i) / 10000.0f;
            const float left = stutter.process(0, input, phase, bpm, 8000.0);
            const float right = stutter.process(1, -input, phase, bpm, 8000.0);
            REQUIRE_THAT(right, WithinAbs(-left, 0.00001f));
            if (i % sliceSamples == sliceSamples / 2) {
                INFO(bpm);
                REQUIRE_THAT(left, WithinAbs(static_cast<float>(sliceSamples / 2) / 10000.0f, 0.0001f));
            }
        }
        stutter.reset();
        REQUIRE(stutter.process(0, 0.25f, 0.8, bpm, 8000.0) == 0.25f);
    }
}

TEST_CASE("Saturation at full drive keeps a hot signal bounded", "[saturate]") {
    REQUIRE(std::abs(saturate(10.0f, 1.0f)) < 0.3f);
}

TEST_CASE("Bit crusher quantizes to the bit depth", "[crusher]") {
    BitCrusher crusher;
    REQUIRE(crusher.process(0, 0.3f, static_cast<float>(kSampleRate), 2, kSampleRate) == 0.5f);
}

TEST_CASE("Bit crusher holds the sample between rate ticks", "[crusher]") {
    BitCrusher crusher;
    const float rateHz = static_cast<float>(kSampleRate / 4.0);
    const float first = crusher.process(0, 0.1f, rateHz, 16, kSampleRate);
    const float second = crusher.process(0, 0.2f, rateHz, 16, kSampleRate);
    const float third = crusher.process(0, 0.3f, rateHz, 16, kSampleRate);
    const float fourth = crusher.process(0, 0.4f, rateHz, 16, kSampleRate);
    REQUIRE(first == 0.0f);
    REQUIRE(second == 0.0f);
    REQUIRE(third == 0.0f);
    REQUIRE_THAT(fourth, WithinAbs(0.4, 1e-4));
}

TEST_CASE("Limiter keeps peaks at or below the ceiling", "[limiter]") {
    PeakLimiter limiter;
    limiter.prepare(kSampleRate);
    auto left = sine(200.0, 4800, 2.0f);
    auto right = sine(300.0, 4800, 1.5f);
    float* channels[] = { left.data(), right.data() };
    limiter.process(channels, 2, 4800, 0.5f);
    REQUIRE(peak(left) <= 0.5f);
    REQUIRE(peak(right) <= 0.5f);
}

TEST_CASE("Limiter delays a signal under the ceiling without changing it", "[limiter]") {
    PeakLimiter limiter;
    limiter.prepare(kSampleRate);
    const auto input = sine(200.0, 4800, 0.1f);
    auto output = input;
    float* channels[] = { output.data() };
    limiter.process(channels, 1, 4800, 1.0f);
    REQUIRE(std::equal(input.begin(), input.end() - PeakLimiter::kLatency, output.begin() + PeakLimiter::kLatency));
}

TEST_CASE("Limiter ramps the gain down before a peak arrives", "[limiter]") {
    constexpr int kJump = 1000;
    PeakLimiter limiter;
    limiter.prepare(kSampleRate);
    std::vector<float> samples(2000, 0.1f);
    std::fill(samples.begin() + kJump, samples.end(), 1.0f);
    float* channels[] = { samples.data() };
    limiter.process(channels, 1, 2000, 0.5f);

    const int jumpOut = kJump + PeakLimiter::kLatency;
    REQUIRE(samples[jumpOut - PeakLimiter::kLookahead / 2] < 0.1f);
    REQUIRE(samples[jumpOut - PeakLimiter::kLookahead / 2] > 0.05f);
    REQUIRE(peak(samples) <= 0.5f);
    REQUIRE_THAT(samples[jumpOut], WithinAbs(0.5, 1e-5));
}
