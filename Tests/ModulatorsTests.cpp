#include "Modulators.h"
#include "TestSignals.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace openedm::core;
using namespace openedm::test;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

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

TEST_CASE("AutoSweep locks phase to 1, 2, 4, 8, 16, 32 and 64 bar windows across time signatures", "[autosweep]") {
    for (int bars : { 1, 2, 4, 8, 16, 32, 64 }) {
        for (double beatsPerBar : { 4.0, 3.0, 2.0, 5.0, 3.5, 6.0 }) {
            AutoSweep sweep;
            const double quarterWayPpq = 0.25 * beatsPerBar * static_cast<double>(bars);
            sweep.syncToTransport(quarterWayPpq, beatsPerBar, bars);
            REQUIRE_THAT(sweep.phase(), WithinAbs(0.25, 1e-6));

            // Full window plus half wraps back to 0.5
            sweep.syncToTransport(1.5 * beatsPerBar * static_cast<double>(bars), beatsPerBar, bars);
            REQUIRE_THAT(sweep.phase(), WithinAbs(0.5, 1e-6));
        }
    }
}

TEST_CASE("AutoSweep EveryBar mode repeats the slope on every bar in the window", "[autosweep]") {
    REQUIRE_THAT(AutoSweep::progressAt(SweepMode::EveryBar, 8, 0.25 / 8.0), WithinAbs(0.25, 1e-6));
    REQUIRE_THAT(AutoSweep::progressAt(SweepMode::EveryBar, 8, 3.25 / 8.0), WithinAbs(0.25, 1e-6));
    REQUIRE_THAT(AutoSweep::progressAt(SweepMode::EveryBar, 16, 7.75 / 16.0), WithinAbs(0.75, 1e-6));
}

TEST_CASE("AutoSweep 1, 2 and 4 bar frames keep each playback mode on the bar grid", "[autosweep]") {
    for (int bars : { 1, 2, 4 }) {
        const double window = static_cast<double>(bars);
        REQUIRE_THAT(AutoSweep::progressAt(SweepMode::BarWindow, bars, 0.0), WithinAbs(0.0, 1e-6));
        REQUIRE_THAT(AutoSweep::progressAt(SweepMode::BarWindow, bars, 0.5), WithinAbs(0.5, 1e-6));
        if (bars > 1) {
            REQUIRE_THAT(AutoSweep::progressAt(SweepMode::BarWindow, bars, 1.0 / window), WithinAbs(1.0f / static_cast<float>(bars), 1e-5));
        }

        // Halfway through bar 1, and the same point in the last bar.
        REQUIRE_THAT(AutoSweep::progressAt(SweepMode::EveryBar, bars, 0.5 / window), WithinAbs(0.5, 1e-6));
        REQUIRE_THAT(AutoSweep::progressAt(SweepMode::EveryBar, bars, (window - 0.5) / window), WithinAbs(0.5, 1e-6));

        // Up & Down reaches the end at the midpoint of the whole window, including a 1-bar loop.
        REQUIRE_THAT(AutoSweep::progressAt(SweepMode::UpDown, bars, 0.25), WithinAbs(0.5, 1e-6));
        REQUIRE_THAT(AutoSweep::progressAt(SweepMode::UpDown, bars, 0.5), WithinAbs(1.0, 1e-6));
        REQUIRE_THAT(AutoSweep::progressAt(SweepMode::UpDown, bars, 0.75), WithinAbs(0.5, 1e-6));

        const double halfFirstBar = 0.5 / window;
        const float macro = 0.5f * (0.5f + static_cast<float>(halfFirstBar));
        REQUIRE_THAT(AutoSweep::progressAt(SweepMode::BarMacro, bars, halfFirstBar), WithinAbs(macro, 1e-5));
    }

    AutoSweep sweep;
    sweep.syncToTransport(16.0, 4.0, 1);
    REQUIRE_THAT(sweep.phase(), WithinAbs(0.0, 1e-6));
    sweep.syncToTransport(15.0, 4.0, 1);
    REQUIRE_THAT(sweep.phase(), WithinAbs(0.75, 1e-6));
    sweep.syncToTransport(-1.0, 4.0, 1);
    REQUIRE_THAT(sweep.phase(), WithinAbs(0.75, 1e-6));
    sweep.syncToTransport(-0.5, 4.0, 2);
    REQUIRE_THAT(sweep.phase(), WithinAbs(0.9375, 1e-6));
}

TEST_CASE("AutoSweep slope curvature round-trips midpoint fractions", "[autosweep]") {
    for (float fraction : { 0.1f, 0.25f, 0.5f, 0.75f, 0.9f }) {
        const float slope = AutoSweep::slopeFromMidpoint(fraction);
        REQUIRE_THAT(AutoSweep::applySlope(0.5f, slope), WithinAbs(fraction, 1e-4));
    }
}

TEST_CASE("AutoSweep interpolates cutoff in octaves and resonance linearly with Mirror support", "[autosweep]") {
    AutoSweepSettings settings;
    settings.enabled = true;
    settings.target = SweepTarget::Mirror;
    settings.bars = 8;
    settings.mode = SweepMode::BarWindow;
    settings.cutoffStartHz = 100.0f;
    settings.cutoffEndHz = 1600.0f;
    settings.cutoffSlope = 0.0f;
    settings.resonanceStart = 0.2f;
    settings.resonanceEnd = 0.8f;
    settings.resonanceSlope = 0.0f;

    const SweepValue midA = AutoSweep::valueAt(settings, 0, 0.5);
    REQUIRE_THAT(midA.cutoffHz, WithinRel(400.0f, 1e-4f));
    REQUIRE_THAT(midA.resonance, WithinAbs(0.5f, 1e-5f));

    const SweepValue quarterA = AutoSweep::valueAt(settings, 0, 0.25);
    const SweepValue quarterB = AutoSweep::valueAt(settings, 1, 0.25);
    REQUIRE_THAT(quarterA.cutoffHz, WithinRel(200.0f, 1e-4f));
    REQUIRE_THAT(quarterB.cutoffHz, WithinRel(800.0f, 1e-4f));
    REQUIRE_THAT(quarterA.resonance, WithinAbs(0.35f, 1e-5f));
    REQUIRE_THAT(quarterB.resonance, WithinAbs(0.65f, 1e-5f));
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
