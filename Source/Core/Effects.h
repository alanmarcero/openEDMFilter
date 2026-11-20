#pragma once

#include "Svf.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>
#include <vector>

namespace openedm::core {

// tanh saturation blended in by drive (0..1). Drive 0 returns the input unchanged.
[[nodiscard]] float saturate(float input, float drive) noexcept;

// First-order anti-derivative anti-aliased (ADAA) asymmetric tanh saturator.
class AdaaSaturator {
public:
    static constexpr int kMaxChannels = 2;

    void reset() noexcept;
    [[nodiscard]] float process(int channel, float input, float drive) noexcept;

private:
    std::array<double, kMaxChannels> prevDriven_{};
    std::array<double, kMaxChannels> prevAntideriv_{};
    std::array<bool, kMaxChannels> primed_{};
};

// Sample-rate reducer and bit crusher with a 4-pole tracking SVF anti-aliasing pre-filter.
class BitCrusher {
public:
    static constexpr int kMaxChannels = 2;

    void reset() noexcept;
    [[nodiscard]] float process(int channel, float input, float rateHz, int bits, double sampleRate) noexcept;

private:
    std::array<float, kMaxChannels> phases_{};
    std::array<float, kMaxChannels> held_{};
    std::array<std::array<SvfState, 2>, kMaxChannels> preFilters_{};
};

enum class RingMode {
    BandLimited,
    SingleSideband
};

// Analytic 8-pole quadrature Hilbert frequency shifter / band-limited ring modulator with pre-ring Nyquist LPF.
class HilbertRingModulator {
public:
    static constexpr int kMaxChannels = 2;

    void reset() noexcept;
    [[nodiscard]] float process(int channel, float input, double carrierPhase, float ringFrequencyHz,
                                double oversampledRate, double hostRate, RingMode mode = RingMode::BandLimited) noexcept;

private:
    struct Allpass2 {
        float x1 = 0.0f;
        float x2 = 0.0f;
        float y1 = 0.0f;
        float y2 = 0.0f;
        [[nodiscard]] float process(float x, float coeff) noexcept;
    };

    std::array<std::array<Allpass2, 4>, kMaxChannels> branchReal_{};
    std::array<std::array<Allpass2, 4>, kMaxChannels> branchImag_{};
    std::array<float, kMaxChannels> imagDelay_{};
    std::array<std::array<SvfState, 4>, kMaxChannels> preNyquistLp_{};
    std::array<std::array<SvfState, 2>, kMaxChannels> usbLpReal_{};
    std::array<std::array<SvfState, 2>, kMaxChannels> usbLpImag_{};
};

class BeatStutter {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    [[nodiscard]] float process(int channel, float input, double beatPhase, double bpm, double sampleRate) noexcept;

private:
    std::array<std::vector<float>, 2> history_;
    std::array<int, 2> position_{};
    std::array<int, 2> available_{};
};

// Stereo-linked lookahead peak limiter with optional 8-tap Kaiser-sinc inter-sample true-peak detection.
template <int Lookahead>
class LookaheadLimiter {
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kLookahead = Lookahead;
    static constexpr int kLatency = kLookahead - 1;

    void prepare(double sampleRate) noexcept {
        constexpr double releaseMs = 80.0;
        releaseCoefficient_ = static_cast<float>(std::exp(-1.0 / (releaseMs * 0.001 * sampleRate)));
        reset();
    }

    void reset() noexcept {
        releasedGain_ = 1.0f;
        releasedSum_ = static_cast<float>(kLookahead);
        position_ = 0;
        requiredGains_.fill(1.0f);
        releasedGains_.fill(1.0f);
        delayLines_ = {};
        tpHistory_ = {};
    }

    void processFrame(std::array<float, kMaxChannels>& frame, int numChannels, float ceiling,
                      bool detectTruePeak = false) noexcept {
        const int count = std::min(numChannels, kMaxChannels);
        const auto channelIndices = std::views::iota(0, count);
        float detectedPeak = 0.0f;

        std::ranges::for_each(channelIndices, [&](int channel) {
            const float sample = frame[static_cast<size_t>(channel)];
            detectedPeak = std::max(detectedPeak, std::abs(sample));
            if (!detectTruePeak)
                return;
            auto& hist = tpHistory_[static_cast<size_t>(channel)];
            std::shift_left(hist.begin(), hist.end(), 1);
            hist[7] = sample;
            constexpr std::array<float, 8> kSincMid{
                -0.0188f, 0.0646f, -0.1790f, 0.6332f, 0.6332f, -0.1790f, 0.0646f, -0.0188f
            };
            constexpr std::array<float, 8> kSincQuarter{
                -0.0115f, 0.0398f, -0.1066f, 0.8995f, 0.2288f, -0.0660f, 0.0221f, -0.0061f
            };
            float mid = 0.0f;
            float q1 = 0.0f;
            float q3 = 0.0f;
            std::ranges::for_each(std::views::iota(size_t{ 0 }, size_t{ 8 }), [&](size_t tap) {
                mid += kSincMid[tap] * hist[tap];
                q1 += kSincQuarter[tap] * hist[tap];
                q3 += kSincQuarter[7 - tap] * hist[tap];
            });
            detectedPeak = std::max({ detectedPeak, std::abs(mid), std::abs(q1), std::abs(q3) });
        });

        requiredGains_[static_cast<size_t>(position_)] = detectedPeak > ceiling ? ceiling / detectedPeak : 1.0f;
        const float heldGain = *std::ranges::min_element(requiredGains_);
        releasedGain_ = std::min(heldGain, heldGain + releaseCoefficient_ * (releasedGain_ - heldGain));
        releasedSum_ += releasedGain_ - releasedGains_[static_cast<size_t>(position_)];
        releasedGains_[static_cast<size_t>(position_)] = releasedGain_;
        const float gain = releasedSum_ / static_cast<float>(kLookahead);

        const int oldest = (position_ + 1) % kLookahead;
        std::ranges::for_each(channelIndices, [&](int channel) {
            auto& delayLine = delayLines_[static_cast<size_t>(channel)];
            delayLine[static_cast<size_t>(position_)] = frame[static_cast<size_t>(channel)];
            frame[static_cast<size_t>(channel)] = std::clamp(
                delayLine[static_cast<size_t>(oldest)] * gain, -ceiling, ceiling);
        });
        position_ = oldest;
    }

    void process(float* const* channels, int numChannels, int numSamples, float ceiling,
                 bool detectTruePeak = false) noexcept {
        const int count = std::min(numChannels, kMaxChannels);
        const auto channelIndices = std::views::iota(0, count);
        std::ranges::for_each(std::views::iota(0, numSamples), [&](int sample) {
            std::array<float, kMaxChannels> frame{};
            std::ranges::for_each(channelIndices, [&](int channel) {
                frame[static_cast<size_t>(channel)] = channels[channel][sample];
            });
            processFrame(frame, count, ceiling, detectTruePeak);
            std::ranges::for_each(channelIndices, [&](int channel) {
                channels[channel][sample] = frame[static_cast<size_t>(channel)];
            });
        });
    }

private:
    float releaseCoefficient_ = 0.0f;
    float releasedGain_ = 1.0f;
    float releasedSum_ = static_cast<float>(kLookahead);
    int position_ = 0;
    std::array<float, kLookahead> requiredGains_{};
    std::array<float, kLookahead> releasedGains_{};
    std::array<std::array<float, kLookahead>, kMaxChannels> delayLines_{};
    std::array<std::array<float, 8>, kMaxChannels> tpHistory_{};
};

using PeakLimiter = LookaheadLimiter<64>;

} // namespace openedm::core
