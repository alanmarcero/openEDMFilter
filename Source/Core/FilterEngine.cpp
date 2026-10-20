#include "FilterEngine.h"
#include <algorithm>
#include <cmath>
#include <limits>
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
constexpr double kTopologyFadeSeconds = 0.01;

bool topologyDiffers(const FilterShape& a, const FilterShape& b) noexcept {
    return a.type != b.type || a.slope != b.slope;
}

float onePoleCoefficient(double seconds, double sampleRate) noexcept {
    return static_cast<float>(std::exp(-1.0 / (seconds * sampleRate)));
}

} // namespace

void FilterEngine::prepare(double sampleRate) noexcept {
    sampleRate_ = sampleRate * Oversampler::kFactor;
    parameterSmoothing_ = onePoleCoefficient(kParameterSmoothingSeconds, sampleRate);
    fadeSamples_ = std::max(1, static_cast<int>(kTopologyFadeSeconds * sampleRate_));
    limiter_.prepare(sampleRate);
    reset();
}

void FilterEngine::reset() noexcept {
    std::ranges::for_each(filters_, [](auto& slots) { slots = {}; });
    fadeRemaining_ = {};
    std::ranges::for_each(envelopes_, [](EnvelopeFollower& envelope) { envelope.reset(); });
    crusher_.reset();
    oversampler_.reset();
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
        std::ranges::for_each(bandIndices, [&](int band) { slotShapes_[band][activeSlot_[band]] = settings.bands[band].shape; });
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
        std::array<Oversampler::Frame, MultiModeFilter::kMaxChannels> upsampled{};
        std::ranges::for_each(channelIndices, [&](int channel) {
            upsampled[channel] = oversampler_.upsample(channel, channels[channel][sample]);
        });

        std::ranges::for_each(std::views::iota(0, Oversampler::kFactor), [&](int phase) {
            const float monoInput = std::accumulate(channelIndices.begin(), channelIndices.end(), 0.0f,
                [&](float sum, int channel) { return sum + upsampled[channel][phase]; }) / static_cast<float>(channelCount);

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
                const float octaves = kModulationOctaves * (bandSettings.lfoDepth * lfoValue + bandSettings.envDepth * envelopeValue);
                const float targetLogCutoff = std::log2(bandSettings.cutoffHz) + octaves;
                logCutoffs_[band] = targetLogCutoff + cutoffSmoothing[band] * (logCutoffs_[band] - targetLogCutoff);
                auto& slots = filters_[band];
                auto& shapes = slotShapes_[band];
                const FilterShape& live = bandSettings.shape;
                if (fadeRemaining_[band] == 0 && topologyDiffers(shapes[activeSlot_[band]], live)) {
                    activeSlot_[band] = 1 - activeSlot_[band];
                    slots[activeSlot_[band]] = MultiModeFilter{};
                    shapes[activeSlot_[band]] = live;
                    fadeRemaining_[band] = fadeSamples_;
                }
                // A topology change during a fade waits for the fade to end. The other settings follow at once.
                const int active = activeSlot_[band];
                shapes[active] = { live.enabled, shapes[active].type, shapes[active].slope, live.resonance, live.width };

                // FM skips the smoother, which would otherwise filter out audio-rate FM.
                const double cutoffHz = std::exp2(logCutoffs_[band] + fmOctaves);
                slots[active].update(shapes[active], cutoffHz, sampleRate_);
                if (fadeRemaining_[band] > 0) {
                    slots[1 - active].update(shapes[1 - active], cutoffHz, sampleRate_);
                }
                return octaves + fmOctaves;
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

                const float dry = upsampled[channel][phase];
                const float preColoured = colour(dry, Placement::PreFilter);
                const auto filterBand = [&](int band, float input) {
                    auto& slots = filters_[band];
                    const int active = activeSlot_[band];
                    const float incoming = slots[active].process(channel, input);
                    if (fadeRemaining_[band] == 0) {
                        return incoming;
                    }
                    const float outgoing = slots[1 - active].process(channel, input);
                    const float progress = 1.0f - static_cast<float>(fadeRemaining_[band]) / static_cast<float>(fadeSamples_);
                    return outgoing + progress * (incoming - outgoing);
                };
                const float filtered = filterBand(1, filterBand(0, preColoured));
                const float postColoured = colour(filtered, Placement::PostFilter);
                const float ringModulated = postColoured + settings.ringMix * (postColoured * ringCarrier - postColoured);
                upsampled[channel][phase] = (dry + mix_ * (ringModulated - dry)) * outputGain_;
            });
            std::ranges::for_each(fadeRemaining_, [](int& remaining) { remaining = std::max(0, remaining - 1); });
        });

        std::ranges::for_each(channelIndices, [&](int channel) {
            channels[channel][sample] = oversampler_.downsample(channel, upsampled[channel]);
        });
    });

    // With the limiter off, it still runs with no ceiling, so the latency does not change.
    const float ceiling = settings.limiterEnabled ? settings.ceiling : std::numeric_limits<float>::infinity();
    limiter_.process(channels, channelCount, numSamples, ceiling);
}

} // namespace openedm::core
