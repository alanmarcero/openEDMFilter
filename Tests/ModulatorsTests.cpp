#include "Modulators.h"
#include "TestSignals.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace openedm::core;
using namespace openedm::test;
using Catch::Matchers::WithinAbs;

TEST_CASE("LFO shapes at a quarter cycle", "[lfo]") {
    REQUIRE_THAT(Lfo::valueAt(LfoShape::Sine, 0.25), WithinAbs(1.0, 1e-6));
    REQUIRE_THAT(Lfo::valueAt(LfoShape::Triangle, 0.25), WithinAbs(0.0, 1e-6));
    REQUIRE_THAT(Lfo::valueAt(LfoShape::SawUp, 0.25), WithinAbs(-0.5, 1e-6));
    REQUIRE_THAT(Lfo::valueAt(LfoShape::SawDown, 0.25), WithinAbs(0.5, 1e-6));
    REQUIRE(Lfo::valueAt(LfoShape::Square, 0.25) == 1.0f);
}

TEST_CASE("Square LFO is low in the second half of the cycle", "[lfo]") {
    REQUIRE(Lfo::valueAt(LfoShape::Square, 0.75) == -1.0f);
}

TEST_CASE("LFO synced to the transport starts from the bar position", "[lfo]") {
    Lfo lfo;
    lfo.syncToTransport(5.0, 4.0);
    REQUIRE_THAT(lfo.next(LfoShape::Sine, 0.0), WithinAbs(1.0, 1e-6));
}

TEST_CASE("LFO advances by the phase increment after each value", "[lfo]") {
    Lfo lfo;
    const float first = lfo.next(LfoShape::SawUp, 0.25);
    const float second = lfo.next(LfoShape::SawUp, 0.25);
    REQUIRE_THAT(first, WithinAbs(-1.0, 1e-6));
    REQUIRE_THAT(second, WithinAbs(-0.5, 1e-6));
}

TEST_CASE("Envelope follower rises on a loud signal at its source frequency", "[envelope]") {
    EnvelopeFollower envelope;
    envelope.configure(500.0f, 1.0f, 100.0f, kSampleRate);
    const auto input = sine(500.0, 4800, 0.8f);
    const float level = std::accumulate(input.begin(), input.end(), 0.0f,
        [&](float, float sample) { return envelope.process(sample); });
    REQUIRE(level > 0.5f);
}

TEST_CASE("Envelope follower falls back to zero after the signal stops", "[envelope]") {
    EnvelopeFollower envelope;
    envelope.configure(500.0f, 1.0f, 50.0f, kSampleRate);
    const auto loud = sine(500.0, 4800, 0.8f);
    const std::vector<float> silence(48000, 0.0f);
    std::ranges::for_each(loud, [&](float sample) { (void)envelope.process(sample); });
    const float level = std::accumulate(silence.begin(), silence.end(), 0.0f,
        [&](float, float sample) { return envelope.process(sample); });
    REQUIRE(level < 0.001f);
}

TEST_CASE("Envelope follower ignores content far from its source frequency", "[envelope]") {
    EnvelopeFollower envelope;
    envelope.configure(8000.0f, 1.0f, 100.0f, kSampleRate);
    const auto input = sine(60.0, 9600, 0.8f);
    const float level = std::accumulate(input.begin(), input.end(), 0.0f,
        [&](float, float sample) { return envelope.process(sample); });
    REQUIRE(level < 0.1f);
}
