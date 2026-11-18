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
    parameterSmoothing_ = onePoleCoefficient(kParameterSmoothingSeconds, sampleRate_);
    fadeSamples_ = std::max(1, static_cast<int>(kTopologyFadeSeconds * sampleRate_));
    limiter_.prepare(sampleRate);
    stutter_.prepare(sampleRate_);
    reset();
}

void FilterEngine::reset() noexcept {
    std::ranges::for_each(filters_, [](auto& slots) { slots = {}; });
    fadeRemaining_ = {};
    sweep_.reset();
    envelopes_ = {};
    parallelB_ = {};
    lfos_ = {};
    stepPhases_ = {};
    fmPhase_ = 0.0;
    ringPhase_ = 0.0;
    clearMidi();
    crusher_.reset();
    stutter_.reset();
    glitchBeatPhase_ = 0.0;
    crushMix_ = 0.0f;
    glitchMix_ = 0.0f;
    inputGain_ = 1.0f;
    driftPhase_ = 0.0;
    subPreserveLp_ = {};
    oversampler_.reset();
    limiter_.reset();
    needsSnap_ = true;
}

void FilterEngine::clearMidi() noexcept {
    midiValues_ = {};
    midiCCs_ = {};
}

void FilterEngine::setMidiValue(MidiSource source, float value, int controller) noexcept {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    if (source == MidiSource::CC) {
        midiCCs_[static_cast<size_t>(std::clamp(controller, 0, 127))] = clamped;
        return;
    }
    midiValues_[static_cast<size_t>(source)] = clamped;
}

std::array<float, FilterEngine::kNumBands> FilterEngine::currentCutoffs() const noexcept {
    return currentLaneCutoffs(0);
}

std::array<float, FilterEngine::kNumBands> FilterEngine::currentLaneCutoffs(int lane) const noexcept {
    return displayedCutoffs_[lane == 1 ? 1 : 0];
}

void FilterEngine::process(const EngineSettings& settings, const Transport& transport,
                           float* const* channels, int numChannels, int numSamples) noexcept {
    const int channelCount = std::min(numChannels, MultiModeFilter::kMaxChannels);
    if (channelCount <= 0) {
        return;
    }
    const auto bandIndices = std::views::iota(0, kNumBands);
    const auto channelIndices = std::views::iota(0, channelCount);

    const double beatsPerBar = std::max(0.5, transport.beatsPerBar);
    const double totalSweepBeats = std::max(0.25, beatsPerBar * static_cast<double>(std::max(1, settings.sweep.bars)));
    // A host playhead is the clock. While it is stopped the sweep holds on that position;
    // with no playhead the internal tempo keeps running.
    const bool hostTimeline = transport.ppqPosition.has_value();
    const bool advanceClock = !hostTimeline || transport.playing;
    const double sweepIncrement = advanceClock ? transport.bpm / 60.0 / sampleRate_ / totalSweepBeats : 0.0;
    const double glitchBeatIncrement = advanceClock ? transport.bpm / 60.0 / sampleRate_ : 0.0;
    if (hostTimeline) {
        sweep_.syncToTransport(*transport.ppqPosition, beatsPerBar, settings.sweep.bars);
        glitchBeatPhase_ = *transport.ppqPosition - std::floor(*transport.ppqPosition);
    }

    if (needsSnap_) {
        std::ranges::for_each(bandIndices, [&](int band) {
            const BandSettings& bandSettings = settings.bands[band];
            const bool swept = AutoSweep::appliesToBand(settings.sweep, band);
            const SweepValue initialSweep = swept
                ? AutoSweep::valueAt(settings.sweep, band, sweep_.phase())
                : SweepValue{ bandSettings.cutoffHz, bandSettings.shape.resonance };
            logCutoffs_[band] = std::log2(initialSweep.cutoffHz);
            lane2LogCutoffs_[band] = logCutoffs_[band];
            resonances_[band] = initialSweep.resonance;
            slotShapes_[band][activeSlot_[band]] = bandSettings.shape;
        });
        routingMix_ = settings.parallel ? 1.0f : 0.0f;
        parallelBlend_ = settings.parallelBlend;
        stereoAngle_ = settings.stereoMode == StereoMode::MidSide && channelCount == 2
            ? std::numbers::pi_v<float> * 0.25f : 0.0f;
        mix_ = settings.bypassed ? 0.0f : settings.mix;
        outputGain_ = settings.bypassed ? 1.0f : settings.outputGain;
        inputGain_ = settings.bypassed ? 1.0f : settings.inputGain;
        const float progress = AutoSweep::progressAt(settings.sweep.mode, settings.sweep.bars, sweep_.phase());
        const float autoCrush = settings.sweep.enabled ? AutoSweep::envelopeAt(settings.sweep.crush, progress) : 0.0f;
        crushMix_ = settings.crushMix + (1.0f - settings.crushMix) * autoCrush;
        glitchMix_ = settings.sweep.enabled ? AutoSweep::envelopeAt(settings.sweep.glitch, progress) : 0.0f;
        needsSnap_ = false;
    }

    std::array<float, kNumBands> cutoffSmoothing{};
    std::array<double, kNumBands> lfoIncrements{};
    std::ranges::for_each(bandIndices, [&](int band) {
        const BandSettings& bandSettings = settings.bands[band];
        const bool isSteep = bandSettings.shape.slope >= FilterSlope::Slope48dB;
        const double smoothingSeconds = settings.inertia && isSteep ? kInertiaSeconds : kFastSmoothingSeconds;
        cutoffSmoothing[band] = onePoleCoefficient(smoothingSeconds, sampleRate_);
        const double effectiveLfoBeats = bandSettings.lfoBeats >= 4.0
            ? bandSettings.lfoBeats * (beatsPerBar / 4.0)
            : bandSettings.lfoBeats;
        lfoIncrements[band] = advanceClock ? transport.bpm / 60.0 / sampleRate_ / effectiveLfoBeats : 0.0;
        std::ranges::for_each(envelopes_[band], [&](auto& envelope) {
            envelope.configure(bandSettings.envSourceHz, bandSettings.envAttackMs, bandSettings.envReleaseMs, sampleRate_);
        });
        if (hostTimeline) {
            lfos_[band].syncToTransport(*transport.ppqPosition, effectiveLfoBeats);
            const double cycles = *transport.ppqPosition / (std::max(0.03125, bandSettings.step.beatsPerStep)
                * std::clamp(bandSettings.step.length, 1, 16));
            stepPhases_[band] = cycles - std::floor(cycles);
        }
    });

    const float targetMix = settings.bypassed ? 0.0f : settings.mix;
    const float targetOutputGain = settings.bypassed ? 1.0f : settings.outputGain;
    const float targetInputGain = settings.bypassed ? 1.0f : settings.inputGain;
    const bool hasSubPreserve = settings.subPreserveHz > 20.5f;
    const float subAlpha = hasSubPreserve
        ? static_cast<float>(1.0 - std::exp(-kTwoPi * std::clamp(settings.subPreserveHz, 20.0f, 250.0f) / sampleRate_))
        : 0.0f;
    const bool hasRingMod = settings.ringMix > 1.0e-5f;
    const bool hasDrift = settings.analogDrift > 1.0e-5f;
    const double fmInc = settings.fmRateHz / sampleRate_;
    const double ringInc = settings.ringFrequencyHz / sampleRate_;
    const double driftInc = 0.37 / sampleRate_;

    std::ranges::for_each(std::views::iota(0, numSamples), [&](int sample) {
        inputGain_ = targetInputGain + parameterSmoothing_ * (inputGain_ - targetInputGain);
        std::array<Oversampler::Frame, MultiModeFilter::kMaxChannels> upsampled{};
        std::ranges::for_each(channelIndices, [&](int channel) {
            upsampled[channel] = oversampler_.upsample(channel, channels[channel][sample] * inputGain_);
        });

        std::ranges::for_each(std::views::iota(0, Oversampler::kFactor), [&](int phase) {
            const float targetAngle = settings.stereoMode == StereoMode::MidSide && channelCount == 2
                ? std::numbers::pi_v<float> * 0.25f : 0.0f;
            stereoAngle_ = targetAngle + parameterSmoothing_ * (stereoAngle_ - targetAngle);
            const bool activeStereo = stereoAngle_ > 1.0e-6f;
            const float cosine = activeStereo ? std::cos(stereoAngle_) : 1.0f;
            const float sine = activeStereo ? std::sin(stereoAngle_) : 0.0f;
            const float rightInput = channelCount == 2 ? upsampled[1][phase] : 0.0f;
            const std::array<float, 2> envelopeInputs{
                cosine * upsampled[0][phase] + sine * rightInput,
                -sine * upsampled[0][phase] + cosine * rightInput
            };
            const float monoInput = std::accumulate(channelIndices.begin(), channelIndices.end(), 0.0f,
                [&](float sum, int channel) { return sum + upsampled[channel][phase]; }) / static_cast<float>(channelCount);

            const float fmOctaves = settings.fmDepth > 1.0e-5f
                ? settings.fmDepth * kFmOctaves * Lfo::valueAt(settings.fmShape, fmPhase_)
                : 0.0f;
            const float ringCarrier = hasRingMod ? static_cast<float>(std::sin(kTwoPi * ringPhase_)) : 0.0f;
            const double nextFmPhase = fmPhase_ + fmInc;
            const double nextRingPhase = ringPhase_ + ringInc;
            const double nextDriftPhase = driftPhase_ + driftInc;
            fmPhase_ = nextFmPhase - std::floor(nextFmPhase);
            ringPhase_ = nextRingPhase - std::floor(nextRingPhase);
            driftPhase_ = nextDriftPhase - std::floor(nextDriftPhase);
            const float driftOct = hasDrift
                ? settings.analogDrift * 0.14f * static_cast<float>(std::sin(kTwoPi * driftPhase_) + 0.35 * std::cos(kTwoPi * driftPhase_ * 2.31))
                : 0.0f;
            const float spreadOct = settings.stereoSpread * 1.25f;
            const double sweepPhase = sweep_.nextPhase(sweepIncrement);
            const float progress = AutoSweep::progressAt(settings.sweep.mode, settings.sweep.bars, sweepPhase);
            const float autoCrush = settings.sweep.enabled ? AutoSweep::envelopeAt(settings.sweep.crush, progress) : 0.0f;
            const float targetCrush = settings.crushMix + (1.0f - settings.crushMix) * autoCrush;
            const float targetGlitch = settings.sweep.enabled ? AutoSweep::envelopeAt(settings.sweep.glitch, progress) : 0.0f;
            crushMix_ = targetCrush + parameterSmoothing_ * (crushMix_ - targetCrush);
            glitchMix_ = targetGlitch + parameterSmoothing_ * (glitchMix_ - targetGlitch);

            std::array<float, kNumBands> modulationOctaves{};
            std::ranges::transform(bandIndices, modulationOctaves.begin(), [&](int band) {
                const BandSettings& bandSettings = settings.bands[band];
                const bool swept = AutoSweep::appliesToBand(settings.sweep, band);
                const SweepValue sweepValue = swept
                    ? AutoSweep::valueAt(settings.sweep, band, sweepPhase)
                    : SweepValue{ bandSettings.cutoffHz, bandSettings.shape.resonance };
                resonances_[band] = sweepValue.resonance + parameterSmoothing_ * (resonances_[band] - sweepValue.resonance);

                const double lfoPhase = lfos_[band].phase();
                const float lfoValue = lfos_[band].next(bandSettings.lfoShape, lfoIncrements[band]);
                const bool hasEnv = std::abs(bandSettings.envDepth) > 1.0e-5f;
                const float envelopeValue = hasEnv
                    ? envelopes_[band][0].process(settings.stereoMode == StereoMode::Linked ? monoInput : envelopeInputs[0])
                    : 0.0f;
                const float lane2Envelope = hasEnv
                    ? envelopes_[band][1].process(settings.stereoMode == StereoMode::Linked ? monoInput : envelopeInputs[1])
                    : 0.0f;
                const float midiValue = bandSettings.midiSource == MidiSource::CC
                    ? midiCCs_[static_cast<size_t>(std::clamp(bandSettings.midiCC, 0, 127))]
                    : midiValues_[static_cast<size_t>(bandSettings.midiSource)];
                const float stepValue = StepModulator::valueAt(bandSettings.step, stepPhases_[band]);
                const float octaves = kModulationOctaves * (bandSettings.lfoDepth * lfoValue
                    + bandSettings.envDepth * envelopeValue + bandSettings.midiDepth * midiValue
                    + bandSettings.step.depth * stepValue);
                const float targetLogCutoff = std::log2(sweepValue.cutoffHz) + octaves;
                logCutoffs_[band] = targetLogCutoff + cutoffSmoothing[band] * (logCutoffs_[band] - targetLogCutoff);
                auto& slots = filters_[band];
                auto& shapes = slotShapes_[band];
                const FilterShape live{
                    bandSettings.shape.enabled, bandSettings.shape.type, bandSettings.shape.slope,
                    resonances_[band], bandSettings.shape.width
                };
                if (fadeRemaining_[band] == 0 && topologyDiffers(shapes[activeSlot_[band]], live)) {
                    activeSlot_[band] = 1 - activeSlot_[band];
                    slots[activeSlot_[band]] = {};
                    if (band == 1) parallelB_[activeSlot_[band]] = {};
                    shapes[activeSlot_[band]] = live;
                    fadeRemaining_[band] = fadeSamples_;
                }
                // A topology change during a fade waits for the fade to end. The other settings follow at once.
                const int active = activeSlot_[band];
                shapes[active] = { live.enabled, shapes[active].type, shapes[active].slope, live.resonance, live.width };

                const bool fmApplies = settings.fmTarget == ModTarget::Both
                    || (settings.fmTarget == ModTarget::BandA && band == 0)
                    || (settings.fmTarget == ModTarget::BandB && band == 1);
                const float bandFm = fmApplies ? fmOctaves : 0.0f;
                const float bandSign = band == 0 ? 1.0f : -1.0f;
                const float targetLog = logCutoffs_[band] + bandFm + driftOct * bandSign - spreadOct * 0.5f * bandSign;
                const float lane0Hz = std::exp2(targetLog);
                // FM skips smoothing to retain audio-rate modulation.
                slots[active][0].update(shapes[active], lane0Hz, sampleRate_);
                if (fadeRemaining_[band] > 0)
                    slots[1 - active][0].update(shapes[1 - active], lane0Hz, sampleRate_);

                const double shiftedLfo = lfoPhase + bandSettings.lane2Phase;
                const float lane2Lfo = Lfo::valueAt(bandSettings.lfoShape, shiftedLfo - std::floor(shiftedLfo));
                const float lane2Step = StepModulator::valueAt(bandSettings.step, stepPhases_[band] + bandSettings.lane2Phase);
                const float lane2Octaves = kModulationOctaves * bandSettings.lane2Depth
                    * (bandSettings.lfoDepth * lane2Lfo + bandSettings.envDepth * lane2Envelope
                        + bandSettings.midiDepth * midiValue + bandSettings.step.depth * lane2Step);
                const float lane2Target = settings.stereoMode == StereoMode::Linked
                    ? std::log2(sweepValue.cutoffHz) + octaves
                    : std::log2(sweepValue.cutoffHz) + lane2Octaves + bandSettings.lane2Offset;
                lane2LogCutoffs_[band] = lane2Target + cutoffSmoothing[band] * (lane2LogCutoffs_[band] - lane2Target);
                const float lane2Hz = std::exp2(lane2LogCutoffs_[band] + bandFm - driftOct * bandSign + spreadOct * 0.5f * bandSign);
                displayedCutoffs_[0][band] = lane0Hz;
                displayedCutoffs_[1][band] = lane2Hz;
                slots[active][1].update(shapes[active], lane2Hz, sampleRate_);
                if (fadeRemaining_[band] > 0)
                    slots[1 - active][1].update(shapes[1 - active], lane2Hz, sampleRate_);
                if (band == 1 && (settings.parallel || routingMix_ > 1.0e-5f)) {
                    parallelB_[active][0].update(shapes[active], lane0Hz, sampleRate_);
                    parallelB_[active][1].update(shapes[active], lane2Hz, sampleRate_);
                    if (fadeRemaining_[band] > 0) {
                        parallelB_[1 - active][0].update(shapes[1 - active], lane0Hz, sampleRate_);
                        parallelB_[1 - active][1].update(shapes[1 - active], lane2Hz, sampleRate_);
                    }
                }
                const double increment = advanceClock ? transport.bpm / 60.0 / sampleRate_
                    / (std::max(0.03125, bandSettings.step.beatsPerStep) * std::clamp(bandSettings.step.length, 1, 16)) : 0.0;
                stepPhases_[band] += increment;
                stepPhases_[band] -= std::floor(stepPhases_[band]);
                const float sweepOctaves = swept ? std::log2(sweepValue.cutoffHz / std::max(20.0f, bandSettings.cutoffHz)) : 0.0f;
                return octaves + sweepOctaves + bandFm;
            });

            const bool followsBandA = settings.crushLinked && settings.bands[0].shape.enabled;
            const float crushOctaves = followsBandA ? modulationOctaves[0] : 0.0f;
            const float crushRateHz = std::clamp(settings.crushRateHz * std::exp2(crushOctaves),
                                                 kMinCrushRateHz, static_cast<float>(sampleRate_));
            mix_ = targetMix + parameterSmoothing_ * (mix_ - targetMix);
            outputGain_ = targetOutputGain + parameterSmoothing_ * (outputGain_ - targetOutputGain);

            const float targetRouting = settings.parallel ? 1.0f : 0.0f;
            routingMix_ = targetRouting + parameterSmoothing_ * (routingMix_ - targetRouting);
            parallelBlend_ = settings.parallelBlend + parameterSmoothing_ * (parallelBlend_ - settings.parallelBlend);
            const auto colour = [&](int channel, float signal, Placement placement) {
                const float drive = settings.drivePlacement == placement ? settings.drive : 0.0f;
                const float driven = saturate(signal, drive);
                if (settings.crushPlacement != placement || crushMix_ <= 0.0f) return driven;
                const float crushed = crusher_.process(channel, driven, crushRateHz, settings.crushBits, sampleRate_);
                return driven + crushMix_ * (crushed - driven);
            };
            std::array<float, 2> pre{}, filtered{};
            std::ranges::for_each(channelIndices, [&](int channel) {
                pre[channel] = colour(channel, upsampled[channel][phase], Placement::PreFilter);
            });
            const std::array<float, 2> lanes{
                cosine * pre[0] + sine * pre[1], -sine * pre[0] + cosine * pre[1]
            };
            const bool needSerial = routingMix_ < 1.0f - 1.0e-5f || !settings.parallel;
            const bool needParallel = routingMix_ > 1.0e-5f || settings.parallel;
            std::ranges::for_each(channelIndices, [&](int channel) {
                const auto filterBand = [&](int band, float input, bool parallelB = false) {
                    auto& slots = parallelB ? parallelB_ : filters_[band];
                    const int active = activeSlot_[band];
                    const float incoming = slots[active][channel].process(0, input);
                    if (fadeRemaining_[band] == 0) return incoming;
                    const float outgoing = slots[1 - active][channel].process(0, input);
                    const float fade = 1.0f - static_cast<float>(fadeRemaining_[band]) / static_cast<float>(fadeSamples_);
                    return outgoing + fade * (incoming - outgoing);
                };
                const float a = filterBand(0, lanes[channel]);
                const float serial = needSerial ? filterBand(1, a) : a;
                const float parallel = needParallel ? (a + parallelBlend_ * (filterBand(1, lanes[channel], true) - a)) : serial;
                filtered[channel] = serial + routingMix_ * (parallel - serial);
            });
            const std::array<float, 2> decoded{
                cosine * filtered[0] - sine * filtered[1], sine * filtered[0] + cosine * filtered[1]
            };
            std::ranges::for_each(channelIndices, [&](int channel) {
                const float dry = upsampled[channel][phase];
                if (hasSubPreserve) {
                    subPreserveLp_[channel] += subAlpha * (dry - subPreserveLp_[channel]);
                }
                const float postColoured = colour(channel, decoded[channel], Placement::PostFilter);
                const float ringModulated = hasRingMod
                    ? postColoured + settings.ringMix * (postColoured * ringCarrier - postColoured)
                    : postColoured;
                float glitched = ringModulated;
                if (glitchMix_ > 1.0e-5f || settings.sweep.enabled) {
                    const float repeated = stutter_.process(channel, ringModulated, glitchBeatPhase_, transport.bpm, sampleRate_);
                    glitched = ringModulated + glitchMix_ * (repeated - ringModulated);
                }
                if (hasSubPreserve) {
                    glitched += subPreserveLp_[channel] * 0.75f;
                }
                upsampled[channel][phase] = (dry + mix_ * (glitched - dry)) * outputGain_;
            });
            glitchBeatPhase_ += glitchBeatIncrement;
            glitchBeatPhase_ -= std::floor(glitchBeatPhase_);
            std::ranges::for_each(fadeRemaining_, [](int& remaining) { remaining = std::max(0, remaining - 1); });
        });

        std::ranges::for_each(channelIndices, [&](int channel) {
            channels[channel][sample] = oversampler_.downsample(channel, upsampled[channel]);
        });
    });

    // With the limiter off or engine bypassed, it still runs with no ceiling, so the latency does not change.
    const float ceiling = (settings.limiterEnabled && !settings.bypassed)
        ? settings.ceiling
        : std::numeric_limits<float>::infinity();
    limiter_.process(channels, channelCount, numSamples, ceiling);
}

} // namespace openedm::core
