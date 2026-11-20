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
    const float fifth = crusher.process(0, 0.9f, rateHz, 16, kSampleRate);
    const float sixth = crusher.process(0, -0.5f, rateHz, 16, kSampleRate);
    REQUIRE(first == 0.0f);
    REQUIRE(second == 0.0f);
    REQUIRE(third == 0.0f);
    // The tracking anti-alias pre-filter is still settling on the 4th sample, so the held value lags the step.
    REQUIRE(fourth > 0.03f);
    REQUIRE(fifth == fourth);
    REQUIRE(sixth == fourth);
    float settled = fourth;
    std::array<int, 64> settleSteps{};
    std::ranges::for_each(settleSteps, [&](int) { settled = crusher.process(0, 0.4f, rateHz, 16, kSampleRate); });
    REQUIRE_THAT(settled, WithinAbs(0.4, 0.02));
}

TEST_CASE("ADAA saturator suppresses folded upper harmonics compared to naive tanh", "[saturate][adaa]") {
    AdaaSaturator adaa;
    REQUIRE(adaa.process(0, 0.37f, 0.0f) == 0.37f);
    REQUIRE(std::abs(adaa.process(0, 10.0f, 1.0f)) < 0.3f);
    adaa.reset();

    // 6700 Hz at 48 kHz: 7th harmonic (46900 Hz) folds to 1100 Hz, 9th (60300 Hz) folds to 12300 Hz.
    const auto input = sine(6700.0, 9600, 0.6f);
    std::vector<float> naiveOut(input.size());
    std::vector<float> adaaOut(input.size());
    std::ranges::transform(input, naiveOut.begin(), [](float s) { return saturate(s, 1.0f); });
    std::ranges::transform(input, adaaOut.begin(), [&](float s) { return adaa.process(0, s, 1.0f); });

    const double naiveAlias = toneLevel(naiveOut, 1100.0) + toneLevel(naiveOut, 12300.0);
    const double adaaAlias = toneLevel(adaaOut, 1100.0) + toneLevel(adaaOut, 12300.0);
    REQUIRE(adaaAlias < naiveAlias * 0.45);
}

TEST_CASE("Pre-filtered bit crusher suppresses above-Nyquist alias foldover", "[crusher][dsp]") {
    BitCrusher crusher;
    const auto input = sine(7100.0, 9600, 0.8f);
    std::vector<float> output(input.size());
    std::ranges::transform(input, output.begin(), [&](float s) {
        return crusher.process(0, s, 2400.0f, 12, kSampleRate);
    });
    // Without pre-filtering, |7100 - 3 * 2400| = 100 Hz folds to sub-bass at ~0.75 amplitude.
    REQUIRE(toneLevel(output, 100.0) < 0.03);
}

TEST_CASE("Hilbert ring modulator shifts single sideband and blocks ultrasonic foldover", "[ring][dsp]") {
    HilbertRingModulator ring;
    const auto input = sine(1000.0, 9600, 0.5f);
    std::vector<float> ssb(input.size());
    double phase = 0.0;
    std::ranges::transform(input, ssb.begin(), [&](float s) {
        const float out = ring.process(0, s, phase, 400.0f, kSampleRate, kSampleRate, RingMode::SingleSideband);
        phase += 400.0 / kSampleRate;
        phase -= std::floor(phase);
        return out;
    });
    REQUIRE(toneLevel(ssb, 1400.0) > 0.25);
    REQUIRE(toneLevel(ssb, 600.0) < toneLevel(ssb, 1400.0) * 0.05);

    // Ultrasonic distortion product at 27 kHz (at 96 kHz oversampled rate, 48 kHz host rate)
    // must not heterodyne down to 22 kHz when modulated by a 5 kHz carrier.
    ring.reset();
    std::vector<float> ultrasonic(9600);
    double carrierPhase = 0.0;
    std::ranges::generate(ultrasonic, [&, n = 0]() mutable {
        const float s = 0.6f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 27000.0 * n++ / 96000.0));
        const float out = ring.process(0, s, carrierPhase, 5000.0f, 96000.0, 48000.0, RingMode::BandLimited);
        carrierPhase += 5000.0 / 96000.0;
        carrierPhase -= std::floor(carrierPhase);
        return out;
    });
    // At 96 kHz, 22 kHz corresponds to 11 kHz in toneLevel's 48 kHz scale.
    REQUIRE(toneLevel(ultrasonic, 11000.0) < 0.015);
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
