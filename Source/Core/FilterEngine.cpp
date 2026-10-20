#include "FilterEngine.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <ranges>

namespace openedm::core {

namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr double kInertiaSeconds = 0.03;
constexpr double kFastSmoothingSeconds = 0.002;
constexpr double kParameterSmoothingSeconds = 0.01;
constexpr float kMinCrushRateHz = 50.0f;

float onePoleCoefficient(double seconds, double sampleRate) noexcept {
    return static_cast<float>(std::exp(-1.0 / (seconds * sampleRate)));
}

} // namespace

void FilterEngine::prepare(double sampleRate) noexcept {
    sampleRate_ = sampleRate;
    parameterSmoothing_ = onePoleCoefficient(kParameterSmoothingSeconds, sampleRate);
    limiter_.prepare(sampleRate);
    reset();
}

void FilterEngine::reset() noexcept {
    std::ranges::for_each(filters_, [](MultiModeFilter& filter) { filter.reset(); });
    std::ranges::for_each(envelopes_, [](EnvelopeFollower& envelope) { envelope.reset(); });
    crusher_.reset();
    limiter_.reset();
    needsSnap_ = true;
}

std::array<float, FilterEngine::kNumBands> FilterEngine::currentCutoffs() const noexcept {
    return { std::exp2(logCutoffs_[0]), std::exp2(logCutoffs_[1]) };
}

void FilterEngine::process(const EngineSettings& settings, const Transport& transport,
                           float* const* channels, int numChannels, int numSamples) noexcept {
    const int channelCount = std::min(numChannels, MultiModeFilter::kMaxChannels);
    if (channelCount <= 0) {
        return;
    }
    const auto bandIndices = std::views::iota(0, kNumBands);
    const auto channelIndices = std::views::iota(0, channelCount);

    if (needsSnap_) {
        std::ranges::transform(settings.bands, logCutoffs_.begin(),
            [](const BandSettings& band) { return std::log2(band.cutoffHz); });
        mix_ = settings.mix;
        outputGain_ = settings.outputGain;
        needsSnap_ = false;
    }

    std::array<float, kNumBands> cutoffSmoothing{};
    std::array<double, kNumBands> lfoIncrements{};
    std::ranges::for_each(bandIndices, [&](int band) {
        const BandSettings& bandSettings = settings.bands[band];
        const bool isSteep = bandSettings.shape.slope >= FilterSlope::Slope48dB;
        const double smoothingSeconds = settings.inertia && isSteep ? kInertiaSeconds : kFastSmoothingSeconds;
        cutoffSmoothing[band] = onePoleCoefficient(smoothingSeconds, sampleRate_);
        lfoIncrements[band] = transport.bpm / 60.0 / sampleRate_ / bandSettings.lfoBeats;
        envelopes_[band].configure(bandSettings.envSourceHz, bandSettings.envAttackMs, bandSettings.envReleaseMs, sampleRate_);
        if (transport.ppqPosition) {
            lfos_[band].syncToTransport(*transport.ppqPosition, bandSettings.lfoBeats);
        }
    });

    std::ranges::for_each(std::views::iota(0, numSamples), [&](int sample) {
        const float monoInput = std::accumulate(channelIndices.begin(), channelIndices.end(), 0.0f,
            [&](float sum, int channel) { return sum + channels[channel][sample]; }) / static_cast<float>(channelCount);

        const auto fmOctaves = static_cast<float>(settings.fmDepth * kFmOctaves * std::sin(kTwoPi * fmPhase_));
        const auto ringCarrier = static_cast<float>(std::sin(kTwoPi * ringPhase_));
        const double nextFmPhase = fmPhase_ + settings.fmRateHz / sampleRate_;
        const double nextRingPhase = ringPhase_ + settings.ringFrequencyHz / sampleRate_;
        fmPhase_ = nextFmPhase - std::floor(nextFmPhase);
        ringPhase_ = nextRingPhase - std::floor(nextRingPhase);

        std::array<float, kNumBands> modulationOctaves{};
        std::ranges::transform(bandIndices, modulationOctaves.begin(), [&](int band) {
            const BandSettings& bandSettings = settings.bands[band];
            const float lfoValue = lfos_[band].next(bandSettings.lfoShape, lfoIncrements[band]);
            const float envelopeValue = envelopes_[band].process(monoInput);
            const float octaves = kModulationOctaves * (bandSettings.lfoDepth * lfoValue + bandSettings.envDepth * envelopeValue) + fmOctaves;
            const float targetLogCutoff = std::log2(bandSettings.cutoffHz) + octaves;
            logCutoffs_[band] = targetLogCutoff + cutoffSmoothing[band] * (logCutoffs_[band] - targetLogCutoff);
            filters_[band].update(bandSettings.shape, std::exp2(logCutoffs_[band]), sampleRate_);
            return octaves;
        });

        const bool followsBandA = settings.crushLinked && settings.bands[0].shape.enabled;
        const float crushOctaves = followsBandA ? modulationOctaves[0] : 0.0f;
        const float crushRateHz = std::clamp(settings.crushRateHz * std::exp2(crushOctaves),
                                             kMinCrushRateHz, static_cast<float>(sampleRate_));
        mix_ = settings.mix + parameterSmoothing_ * (mix_ - settings.mix);
        outputGain_ = settings.outputGain + parameterSmoothing_ * (outputGain_ - settings.outputGain);

        std::ranges::for_each(channelIndices, [&](int channel) {
            const auto colour = [&](float signal, Placement placement) {
                const float drive = settings.drivePlacement == placement ? settings.drive : 0.0f;
                const float driven = saturate(signal, drive);
                if (settings.crushPlacement != placement || settings.crushMix <= 0.0f) {
                    return driven;
                }
                const float crushed = crusher_.process(channel, driven, crushRateHz, settings.crushBits, sampleRate_);
                return driven + settings.crushMix * (crushed - driven);
            };

            const float dry = channels[channel][sample];
            const float preColoured = colour(dry, Placement::PreFilter);
            const float filtered = filters_[1].process(channel, filters_[0].process(channel, preColoured));
            const float postColoured = colour(filtered, Placement::PostFilter);
            const float ringModulated = postColoured + settings.ringMix * (postColoured * ringCarrier - postColoured);
            channels[channel][sample] = (dry + mix_ * (ringModulated - dry)) * outputGain_;
        });
    });

    if (settings.limiterEnabled) {
        limiter_.process(channels, channelCount, numSamples, settings.ceiling);
    }
}

} // namespace openedm::core
