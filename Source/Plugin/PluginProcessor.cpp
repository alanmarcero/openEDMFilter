#include "PluginProcessor.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>

namespace core = openedm::core;
namespace params = openedm::params;

// processBlock stores the time signature for the editor, so the store must not lock.
static_assert(std::atomic<juce::AudioPlayHead::TimeSignature>::is_always_lock_free);

OpenEdmFilterProcessor::OpenEdmFilterProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "openEDMFilter", params::createParameterLayout()),
      editHistory_(*this),
      values_([this] {
          const auto raw = [this](const juce::String& id) { return parameters.getRawParameterValue(id); };
          const auto bandValues = [&](int band) {
              const auto bandRaw = [&](const char* suffix) { return raw(params::bandId(band, suffix)); };
              return BandParameters{
                  bandRaw(params::kBypass), bandRaw(params::kType), bandRaw(params::kExtendedType), bandRaw(params::kSlope), bandRaw(params::kCutoff),
                  bandRaw(params::kResonance), bandRaw(params::kWidth), bandRaw(params::kLfoShape),
                  bandRaw(params::kLfoRate), bandRaw(params::kLfoDepth), bandRaw(params::kEnvSource),
                  bandRaw(params::kEnvAttack), bandRaw(params::kEnvRelease), bandRaw(params::kEnvDepth),
                  bandRaw(params::kEnvInput)
              };
          };
          const auto sweepValues = [&](int slot) {
              const auto sweepRaw = [&](const char* key) { return raw(params::sweepId(slot, key)); };
              SweepParameters sweep{
                  sweepRaw(params::kSweepTarget), sweepRaw(params::kSweepBars), sweepRaw(params::kSweepMode),
                  sweepRaw(params::kSweepCutoffStart), sweepRaw(params::kSweepCutoffEnd), sweepRaw(params::kSweepCutoffSlope),
                  sweepRaw(params::kSweepResStart), sweepRaw(params::kSweepResEnd), sweepRaw(params::kSweepResSlope), {}
              };
              for (size_t i = 0; i < sweep.effects.size(); ++i) sweep.effects[i] = sweepRaw(params::kSweepEffectIds[i]);
              return sweep;
          };
          return ParameterValues{
              { bandValues(0), bandValues(1) },
              raw(params::kFmRate), raw(params::kFmDepth), raw(params::kDrive), raw(params::kDrivePosition),
              raw(params::kCrushRate), raw(params::kCrushBits), raw(params::kCrushMix), raw(params::kCrushPosition),
              raw(params::kCrushLink), raw(params::kRingFrequency), raw(params::kRingMix), raw(params::kMix),
              raw(params::kOutput), raw(params::kLimiter), raw(params::kCeiling), raw(params::kInertia),
              raw(params::kMasterBypass), raw(params::kClockSync), raw(params::kTempoBpm), raw(params::kTimeSignature),
              raw(params::kSweepEnabled), raw(params::kSweepBypass), raw(params::kSweepSlot),
              { sweepValues(0), sweepValues(1), sweepValues(2), sweepValues(3) }, raw(params::kInputGain),
              raw(params::kSubPreserve), raw(params::kAnalogDrift), raw(params::kStereoSpread)
          };
      }()) {
    routing_ = parameters.getRawParameterValue("routing");
    parallelBlend_ = parameters.getRawParameterValue("parallel_blend");
    fmShape_ = parameters.getRawParameterValue("fm_shape");
    fmTarget_ = parameters.getRawParameterValue("fm_target");
    stereoMode_ = parameters.getRawParameterValue("stereo_mode");
    std::ranges::for_each(std::views::iota(0, 2), [&](int band) {
        const auto raw = [&](const char* suffix) { return parameters.getRawParameterValue(params::bandId(band, suffix)); };
        auto& mod = modulation_[static_cast<size_t>(band)];
        mod = { raw("midi_source"), raw("midi_cc"), raw("midi_depth"), raw("step_mode"), raw("step_length"),
            raw("step_pulses"), raw("step_rotation"), raw("step_beats"), raw("step_depth"), raw("lane2_depth"),
            raw("lane2_phase"), raw("lane2_offset"), {} };
        std::ranges::for_each(std::views::iota(0, 16), [&](int step) {
            mod.steps[static_cast<size_t>(step)] = parameters.getRawParameterValue(params::bandId(band, "step_") + juce::String(step + 1));
        });
    });
    setLatencySamples(core::FilterEngine::kLatencySamples);
    parameters.state.setProperty("barsLayout", params::kSweepBarsLayout, nullptr);
    presetName_ = openedm::presets::factoryPresets().front().name;
    editHistory_.setHooks(
        [this](juce::ValueTree& snapshot) {
            const juce::ScopedLock lock(presetLock_);
            snapshot.setProperty("program", currentProgram_.load(), nullptr);
            snapshot.setProperty("presetName", presetName_, nullptr);
            snapshot.setProperty("presetEdited", presetEdited_, nullptr);
        },
        [this](const juce::ValueTree& snapshot) {
            if (snapshot.hasProperty("program"))
                currentProgram_ = static_cast<int>(snapshot.getProperty("program"));
            if (snapshot.hasProperty("presetName"))
                setPreset(snapshot.getProperty("presetName").toString(), static_cast<bool>(snapshot.getProperty("presetEdited")));
        },
        [this] { setPreset(presetName(), true); });
    ab_.reset(editHistory_.captureParameters());
}

void OpenEdmFilterProcessor::prepareToPlay(double sampleRate, int) {
    processingSampleRate_ = sampleRate;
    engine_.prepare(sampleRate);
    spectrumFft_ = std::make_unique<juce::dsp::FFT>(kSpectrumFftOrder);
    spectrumWindowSum_ = 0.0f;
    for (int sample = 0; sample < kSpectrumFftSize; ++sample) {
        const float window = 0.5f - 0.5f * std::cos(static_cast<float>(2.0 * std::numbers::pi * sample / (kSpectrumFftSize - 1)));
        spectrumWindow_[static_cast<size_t>(sample)] = window;
        spectrumWindowSum_ += window;
    }
    spectrumFifo_ = {};
    spectrumFftData_ = {};
    spectrumWriteIndex_ = 0;
    displayInputPeak.store(0.0f);
    displayOutputPeak.store(0.0f);
    std::ranges::for_each(displaySpectrum, [](auto& value) { value.store(0.0f); });
    heldVelocities_ = {};
    notePressure_ = {};
    channelPressure_ = {};
    controllers_ = {};
    controllerOrder_ = 0;
}

bool OpenEdmFilterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn = layouts.getMainInputChannelSet();
    const bool mainOk = (mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo())
        && mainIn == mainOut;
    if (!mainOk)
        return false;
    if (layouts.inputBuses.size() < 2)
        return true;
    const auto sidechain = layouts.getChannelSet(true, 1);
    return sidechain.isDisabled()
        || sidechain == juce::AudioChannelSet::mono()
        || sidechain == juce::AudioChannelSet::stereo();
}

bool OpenEdmFilterProcessor::isSidechainConnected() const noexcept {
    if (getBusCount(true) < 2)
        return false;
    const auto* bus = getBus(true, 1);
    return bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0;
}

void OpenEdmFilterProcessor::setEditorSize(int width, int height) noexcept {
    editorWidth_ = juce::jlimit(kMinEditorWidth, kMaxEditorWidth, width);
    editorHeight_ = juce::jlimit(kMinEditorHeight, kMaxEditorHeight, height);
}

// A switch or a copy is not an undo step. Each slot keeps its own history. A copy clears the target's history.
void OpenEdmFilterProcessor::toggleAB() {
    editHistory_.flush();
    ab_.remember(editHistory_.captureParameters(), ab_.active());
    ab_.setActive(1 - ab_.active());
    editHistory_.selectHistory(ab_.active());
    editHistory_.restore(ab_.slot(ab_.active()));
}

void OpenEdmFilterProcessor::copyBetweenSlots(int from, int to) {
    from = juce::jlimit(0, 1, from);
    to = juce::jlimit(0, 1, to);
    editHistory_.flush();
    ab_.remember(editHistory_.captureParameters(), ab_.active());
    const auto source = ab_.slot(from);
    ab_.remember(source, to);
    editHistory_.clearHistory(to);
    if (ab_.active() == to)
        editHistory_.restore(source);
}

void OpenEdmFilterProcessor::editParameters(const std::function<void()>& edit, const juce::String& name) {
    editHistory_.performEdit([&] {
        edit();
        setPreset(presetName(), true);
    }, name);
}

void OpenEdmFilterProcessor::loadUserPreset(const juce::String& name, const juce::ValueTree& snapshot) {
    editHistory_.performEdit([&] {
        editHistory_.restore(snapshot);
        setPreset(name, false);
    }, "User preset");
}

void OpenEdmFilterProcessor::setPreset(const juce::String& name, bool isEdited) {
    const juce::ScopedLock lock(presetLock_);
    presetName_ = name;
    presetEdited_ = isEdited;
}

void OpenEdmFilterProcessor::renamePreset(const juce::String& from, const juce::String& to) {
    const juce::ScopedLock lock(presetLock_);
    if (presetName_ == from)
        presetName_ = to;
}

juce::String OpenEdmFilterProcessor::presetName() const {
    const juce::ScopedLock lock(presetLock_);
    return presetName_;
}

juce::String OpenEdmFilterProcessor::presetDisplayName() const {
    const juce::ScopedLock lock(presetLock_);
    return presetEdited_ ? presetName_ + " *" : presetName_;
}

int OpenEdmFilterProcessor::currentSweepSlot() const noexcept {
    return std::clamp(static_cast<int>(values_.sweepSlot->load()), 0, 3);
}

core::AutoSweepSettings OpenEdmFilterProcessor::currentSweepSettings() const noexcept {
    const auto& sweep = values_.sweeps[static_cast<size_t>(currentSweepSlot())];
    const auto isOn = [](const std::atomic<float>* value) { return value->load() > 0.5f; };
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };
    const auto barIndex = static_cast<size_t>(std::clamp(choice(sweep.sweepBars), 0, static_cast<int>(params::kSweepBarCounts.size()) - 1));
    return core::AutoSweepSettings{
        isOn(values_.sweepEnabled) && !isOn(values_.sweepBypass),
        static_cast<core::SweepTarget>(choice(sweep.sweepTarget)),
        params::kSweepBarCounts[barIndex],
        static_cast<core::SweepMode>(choice(sweep.sweepMode)),
        sweep.sweepCutoffStart->load(),
        sweep.sweepCutoffEnd->load(),
        sweep.sweepCutoffSlope->load(),
        sweep.sweepResStart->load(),
        sweep.sweepResEnd->load(),
        sweep.sweepResSlope->load(),
        { sweep.effects[0]->load(), sweep.effects[1]->load(), sweep.effects[2]->load(), sweep.effects[3]->load() },
        { sweep.effects[4]->load(), sweep.effects[5]->load(), sweep.effects[6]->load(), sweep.effects[7]->load() }
    };
}

juce::AudioProcessorParameter* OpenEdmFilterProcessor::getBypassParameter() const {
    return parameters.getParameter(params::kMasterBypass);
}

void OpenEdmFilterProcessor::updatePeakMeter(std::atomic<float>& meter, float peak) noexcept {
    // A failed exchange reloads the current value, so a reset by the editor is never lost.
    float held = meter.load(std::memory_order_relaxed);
    while (peak > held && !meter.compare_exchange_weak(held, peak, std::memory_order_relaxed)) {}
}

void OpenEdmFilterProcessor::updateInputAnalysis(const juce::AudioBuffer<float>& buffer, float inputGain) noexcept {
    const int channels = std::min(2, buffer.getNumChannels());
    if (channels == 0) {
        spectrumWriteIndex_ = 0;
        return;
    }
    float peak = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        for (int channel = 0; channel < channels; ++channel)
            peak = std::max(peak, std::abs(buffer.getSample(channel, sample) * inputGain));
    }
    updatePeakMeter(displayInputPeak, peak);

    if (spectrumFft_ == nullptr || !spectrumVisible.load(std::memory_order_relaxed)
        || spectrumFrozen.load(std::memory_order_relaxed)) {
        spectrumWriteIndex_ = 0;
        return;
    }

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        const size_t write = spectrumWriteIndex_++;
        for (int channel = 0; channel < 2; ++channel) {
            const int sourceChannel = std::min(channel, channels - 1);
            spectrumFifo_[static_cast<size_t>(channel)][write] = buffer.getSample(sourceChannel, sample) * inputGain;
        }
        if (spectrumWriteIndex_ == static_cast<size_t>(kSpectrumFftSize)) {
            spectrumWriteIndex_ = 0;
            updateSpectrumFrame();
        }
    }
}

void OpenEdmFilterProcessor::updateOutputMeter(const juce::AudioBuffer<float>& buffer) noexcept {
    const int channels = std::min(2, buffer.getNumChannels());
    float peak = 0.0f;
    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            peak = std::max(peak, std::abs(buffer.getSample(channel, sample)));
    updatePeakMeter(displayOutputPeak, peak);
}

void OpenEdmFilterProcessor::updateSpectrumFrame() noexcept {
    if (spectrumFrozen.load(std::memory_order_relaxed) || !spectrumVisible.load(std::memory_order_relaxed))
        return;

    std::array<float, kSpectrumDisplayBins> peaks{};
    const double sampleRate = std::max(1.0, processingSampleRate_);
    const double nyquist = sampleRate * 0.5;
    const double displayBinCount = static_cast<double>(kSpectrumDisplayBins);
    const auto spectrumForChannel = [&](size_t channel, std::array<float, kSpectrumDisplayBins>& destination) {
        for (int sample = 0; sample < kSpectrumFftSize; ++sample) {
            const auto index = static_cast<size_t>(sample);
            spectrumFftData_[index] = spectrumFifo_[channel][index] * spectrumWindow_[index];
        }
        std::fill(spectrumFftData_.begin() + kSpectrumFftSize, spectrumFftData_.end(), 0.0f);
        spectrumFft_->performFrequencyOnlyForwardTransform(spectrumFftData_.data(), true);

        for (size_t displayBin = 0; displayBin < destination.size(); ++displayBin) {
            const double lowHz = 20.0 * std::pow(1000.0, static_cast<double>(displayBin) / displayBinCount);
            if (lowHz >= nyquist) continue;
            const double highHz = std::min(
                20.0 * std::pow(1000.0, static_cast<double>(displayBin + 1) / displayBinCount), nyquist);
            const int firstFftBin = std::clamp(static_cast<int>(std::floor(lowHz * kSpectrumFftSize / sampleRate)),
                                               1, kSpectrumFftSize / 2);
            const int lastFftBin = std::clamp(static_cast<int>(std::ceil(highHz * kSpectrumFftSize / sampleRate)),
                                              firstFftBin, kSpectrumFftSize / 2);
            for (int fftBin = firstFftBin; fftBin <= lastFftBin; ++fftBin) {
                const float magnitude = spectrumFftData_[static_cast<size_t>(fftBin)] * 2.0f / spectrumWindowSum_;
                destination[displayBin] = std::max(destination[displayBin], magnitude);
            }
        }
    };

    std::array<float, kSpectrumDisplayBins> left{};
    std::array<float, kSpectrumDisplayBins> right{};
    spectrumForChannel(0, left);
    spectrumForChannel(1, right);
    for (size_t bin = 0; bin < peaks.size(); ++bin)
        peaks[bin] = std::max(left[bin], right[bin]);
    for (size_t bin = 0; bin < peaks.size(); ++bin)
        displaySpectrum[bin].store(peaks[bin], std::memory_order_relaxed);
}

void OpenEdmFilterProcessor::handleMidi(const juce::MidiMessage& message) noexcept {
    const auto channel = static_cast<size_t>(std::clamp(message.getChannel() - 1, 0, 15));
    if (message.isNoteOn())
        heldVelocities_[channel][static_cast<size_t>(message.getNoteNumber())] = message.getFloatVelocity();
    if (message.isNoteOff()) {
        heldVelocities_[channel][static_cast<size_t>(message.getNoteNumber())] = 0.0f;
        notePressure_[channel][static_cast<size_t>(message.getNoteNumber())] = 0.0f;
    }
    if (message.isChannelPressure()) channelPressure_[channel] = static_cast<float>(message.getChannelPressureValue()) / 127.0f;
    if (message.isAftertouch()) notePressure_[channel][static_cast<size_t>(message.getNoteNumber())] = static_cast<float>(message.getAfterTouchValue()) / 127.0f;
    if (message.isAllNotesOff() || message.isAllSoundOff()) {
        heldVelocities_[channel] = {};
        notePressure_[channel] = {};
        channelPressure_[channel] = 0.0f;
    }
    if (message.isController()) {
        const auto controller = static_cast<size_t>(message.getControllerNumber());
        if (controller == 121) {
            notePressure_[channel] = {};
            channelPressure_[channel] = 0.0f;
            controllers_[channel] = {};
            std::ranges::for_each(std::views::iota(size_t{0}, controllers_[channel].size()), [&](size_t number) {
                const auto latest = std::ranges::max_element(controllers_, {}, [number](const auto& values) {
                    return values[number].order;
                });
                engine_.setMidiValue(core::MidiSource::CC, (*latest)[number].value, static_cast<int>(number));
            });
        }
        if (controller != 121) {
            const float value = static_cast<float>(message.getControllerValue()) / 127.0f;
            controllers_[channel][controller] = { value, ++controllerOrder_ };
            engine_.setMidiValue(core::MidiSource::CC, value, static_cast<int>(controller));
            return;
        }
    }
    float velocity = 0.0f, pressure = 0.0f;
    std::ranges::for_each(std::views::iota(size_t{0}, heldVelocities_.size()), [&](size_t lane) {
        pressure = std::max(pressure, channelPressure_[lane]);
        std::ranges::for_each(std::views::iota(size_t{0}, heldVelocities_[lane].size()), [&](size_t note) {
            velocity = std::max(velocity, heldVelocities_[lane][note]);
            pressure = std::max(pressure, notePressure_[lane][note]);
        });
    });
    engine_.setMidiValue(core::MidiSource::Velocity, velocity);
    engine_.setMidiValue(core::MidiSource::Aftertouch, pressure);
}

void OpenEdmFilterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto isOn = [](const std::atomic<float>* value) { return value->load() > 0.5f; };
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };

    const auto bandSettings = [&](const BandParameters& band) {
        core::BandSettings settings{
            core::FilterShape{ !isOn(band.bypass), static_cast<core::FilterType>(params::effectiveType(choice(band.type), choice(band.extendedType))),
                               static_cast<core::FilterSlope>(choice(band.slope)), band.resonance->load(),
                               band.width->load() },
            band.cutoff->load(),
            static_cast<core::LfoShape>(choice(band.lfoShape)),
            params::kLfoBeats[static_cast<size_t>(choice(band.lfoRate))],
            band.lfoDepth->load(),
            band.envSource->load(),
            band.envAttack->load(),
            band.envRelease->load(),
            band.envDepth->load()
        };
        settings.envListen = static_cast<core::EnvelopeListen>(choice(band.envInput));
        return settings;
    };

    core::EngineSettings settings{
        { bandSettings(values_.bands[0]), bandSettings(values_.bands[1]) },
        values_.fmRate->load(),
        values_.fmDepth->load(),
        values_.drive->load(),
        static_cast<core::Placement>(choice(values_.drivePosition)),
        values_.crushRate->load(),
        choice(values_.crushBits),
        values_.crushMix->load(),
        static_cast<core::Placement>(choice(values_.crushPosition)),
        isOn(values_.crushLink),
        values_.ringFrequency->load(),
        values_.ringMix->load(),
        values_.mix->load(),
        juce::Decibels::decibelsToGain(values_.output->load()),
        isOn(values_.limiter),
        juce::Decibels::decibelsToGain(values_.ceiling->load()),
        isOn(values_.inertia),
        currentSweepSettings(),
        isOn(values_.masterBypass)
    };
    settings.inputGain = juce::Decibels::decibelsToGain(values_.inputGain->load());
    settings.subPreserveHz = values_.subPreserve->load();
    settings.analogDrift = values_.analogDrift->load();
    settings.stereoSpread = values_.stereoSpread->load();

    const int layoutChannels = getTotalNumInputChannels();
    const bool bufferMatchesLayout = layoutChannels > 0 && buffer.getNumChannels() >= layoutChannels;
    juce::AudioBuffer<float> mainBuffer;
    if (bufferMatchesLayout)
        mainBuffer = getBusBuffer(buffer, true, 0);
    else
        mainBuffer = juce::AudioBuffer<float>(buffer.getArrayOfWritePointers(),
                                              juce::jmin(2, buffer.getNumChannels()), buffer.getNumSamples());
    float* sidechainBase[2] {};
    int sidechainChannels = 0;
    if (bufferMatchesLayout && isSidechainConnected()) {
        auto sidechainBuffer = getBusBuffer(buffer, true, 1);
        sidechainChannels = juce::jmin(2, sidechainBuffer.getNumChannels());
        for (int channel = 0; channel < sidechainChannels; ++channel)
            sidechainBase[channel] = sidechainBuffer.getWritePointer(channel);
    }
    updateInputAnalysis(mainBuffer, settings.bypassed ? 1.0f : settings.inputGain);

    settings.parallel = isOn(routing_);
    settings.parallelBlend = parallelBlend_->load();
    settings.fmShape = static_cast<core::LfoShape>(choice(fmShape_));
    settings.fmTarget = static_cast<core::ModTarget>(choice(fmTarget_));
    settings.stereoMode = static_cast<core::StereoMode>(choice(stereoMode_));
    std::ranges::for_each(std::views::iota(size_t{0}, modulation_.size()), [&](size_t band) {
        const auto& mod = modulation_[band];
        auto& target = settings.bands[band];
        target.midiSource = static_cast<core::MidiSource>(choice(mod.midiSource));
        target.midiCC = choice(mod.midiCC);
        target.midiDepth = mod.midiDepth->load();
        target.step.mode = static_cast<core::StepMode>(choice(mod.stepMode));
        target.step.length = choice(mod.stepLength);
        target.step.pulses = choice(mod.stepPulses);
        target.step.rotation = choice(mod.stepRotation);
        target.step.beatsPerStep = mod.stepBeats->load();
        target.step.depth = mod.stepDepth->load();
        target.lane2Depth = mod.lane2Depth->load();
        target.lane2Phase = mod.lane2Phase->load();
        target.lane2Offset = mod.lane2Offset->load();
        std::ranges::transform(mod.steps, target.step.values.begin(), [](const auto* value) { return value->load(); });
    });

    const bool syncToHost = isOn(values_.clockSync);
    const double manualBpm = static_cast<double>(values_.tempoBpm->load());
    const int sigChoice = choice(values_.timeSignature);
    const double manualBeatsPerBar = params::beatsPerBarForSignature(sigChoice);
    const auto [menuNumerator, menuDenominator]
        = params::kTimeSignatures[static_cast<size_t>(std::clamp(sigChoice, 0, static_cast<int>(params::kTimeSignatures.size()) - 1))];
    juce::AudioPlayHead::TimeSignature signature{ menuNumerator, menuDenominator };
    bool hostTempo = false;

    bool hostStopped = false;
    const core::Transport transport = [&] {
        if (!syncToHost) {
            return core::Transport{ manualBpm, std::nullopt, manualBeatsPerBar };
        }
        auto* playHead = getPlayHead();
        if (playHead == nullptr) {
            return core::Transport{ manualBpm, std::nullopt, manualBeatsPerBar };
        }
        const auto position = playHead->getPosition();
        if (!position.hasValue()) {
            return core::Transport{ manualBpm, std::nullopt, manualBeatsPerBar };
        }
        hostStopped = wrapperType != wrapperType_Standalone && !position->getIsPlaying();
        hostTempo = position->getBpm().hasValue();
        const double bpm = position->getBpm().orFallback(manualBpm);
        double beatsPerBar = manualBeatsPerBar;
        if (sigChoice == 0 && position->getTimeSignature().hasValue()) {
            const auto sig = *position->getTimeSignature();
            if (sig.numerator > 0 && sig.denominator > 0) {
                beatsPerBar = static_cast<double>(sig.numerator) * (4.0 / static_cast<double>(sig.denominator));
                signature = sig;
            }
        }
        if (!position->getPpqPosition().hasValue()) {
            return core::Transport{ bpm, std::nullopt, beatsPerBar, true };
        }
        // Keep the playhead while stopped so the sweep holds instead of free-running.
        return core::Transport{ bpm, *position->getPpqPosition(), beatsPerBar, position->getIsPlaying() };
    }();

    int rendered = 0;
    const auto renderUntil = [&](int end) {
        if (end <= rendered) return;
        std::array<float*, 2> channels{};
        const int count = std::min(2, mainBuffer.getNumChannels());
        std::ranges::for_each(std::views::iota(0, count), [&](int channel) {
            channels[static_cast<size_t>(channel)] = mainBuffer.getWritePointer(channel) + rendered;
        });
        float* sidechainPtrs[2] {};
        for (int channel = 0; channel < sidechainChannels; ++channel)
            sidechainPtrs[channel] = sidechainBase[channel] + rendered;
        auto segmentTransport = transport;
        if (segmentTransport.ppqPosition.has_value() && transport.playing)
            *segmentTransport.ppqPosition += rendered / processingSampleRate_ * transport.bpm / 60.0;
        engine_.process(settings, segmentTransport, channels.data(), count, end - rendered,
                        sidechainChannels > 0 ? sidechainPtrs : nullptr, sidechainChannels);
        rendered = end;
    };
    std::ranges::for_each(midi, [&](const auto metadata) {
        renderUntil(std::clamp(metadata.samplePosition, 0, buffer.getNumSamples()));
        if (metadata.numBytes <= 3) handleMidi(metadata.getMessage());
    });
    renderUntil(buffer.getNumSamples());
    midi.clear();
    updateOutputMeter(mainBuffer);

    const auto cutoffs = engine_.currentCutoffs();
    const auto resonances = engine_.currentResonances();
    displayCutoffs[0].store(cutoffs[0]);
    displayCutoffs[1].store(cutoffs[1]);
    displayResonances[0].store(resonances[0]);
    displayResonances[1].store(resonances[1]);
    displaySweepPhase.store(engine_.currentSweepPhase());
    displayBeatPhase.store(hostStopped ? 1.0f : engine_.currentBeatPhase(), std::memory_order_relaxed);
    displayBpm.store(static_cast<float>(transport.bpm));
    displayHostTempo.store(hostTempo);
    displayTimeSignature.store(signature);
    displayBeatsPerBar.store(static_cast<float>(transport.beatsPerBar));
}

float OpenEdmFilterProcessor::animationBeatPhase(double seconds) const noexcept {
    const float phase = displayBeatPhase.load(std::memory_order_relaxed);
    if (phase >= 0) return phase;
    const auto beats = seconds * values_.tempoBpm->load(std::memory_order_relaxed) / 60.0;
    return static_cast<float>(beats - std::floor(beats));
}

juce::AudioProcessorEditor* OpenEdmFilterProcessor::createEditor() {
    return new OpenEdmFilterEditor(*this);
}

int OpenEdmFilterProcessor::getNumPrograms() {
    return static_cast<int>(openedm::presets::factoryPresets().size());
}

void OpenEdmFilterProcessor::setCurrentProgram(int index) {
    const auto& presets = openedm::presets::factoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) {
        return;
    }
    editHistory_.performEdit([&] {
        currentProgram_ = index;
        setPreset(presets[static_cast<size_t>(index)].name, false);
        openedm::presets::apply(presets[static_cast<size_t>(index)], parameters);
    }, "Preset");
}

const juce::String OpenEdmFilterProcessor::getProgramName(int index) {
    const auto& presets = openedm::presets::factoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) {
        return {};
    }
    return presets[static_cast<size_t>(index)].name;
}

void OpenEdmFilterProcessor::getStateInformation(juce::MemoryBlock& destData) {
    ab_.remember(editHistory_.captureParameters(), ab_.active());
    auto state = parameters.copyState();
    state.setProperty("program", currentProgram_.load(), nullptr);
    {
        const juce::ScopedLock lock(presetLock_);
        state.setProperty("presetName", presetName_, nullptr);
        state.setProperty("presetEdited", presetEdited_, nullptr);
    }
    state.setProperty("preset_catalog_version", 2, nullptr);
    state.setProperty("barsLayout", params::kSweepBarsLayout, nullptr);
    state.setProperty("editorWidth", editorWidth_.load(), nullptr);
    state.setProperty("editorHeight", editorHeight_.load(), nullptr);
    state.setProperty("modulationPage", modulationPageVisible.load(), nullptr);
    if (auto existing = state.getChildWithName("AB"); existing.isValid())
        state.removeChild(existing, nullptr);
    state.appendChild(ab_.toValueTree(), nullptr);
    const auto xml = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void OpenEdmFilterProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) {
        return;
    }
    auto state = juce::ValueTree::fromXml(*xml);
    params::migrateSweepBarState(state);
    for (auto* parameter : getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged != nullptr && !state.getChildWithProperty("id", ranged->paramID).isValid()) {
            juce::ValueTree value("PARAM");
            value.setProperty("id", ranged->paramID, nullptr);
            value.setProperty("value", ranged->convertFrom0to1(ranged->getDefaultValue()), nullptr);
            state.appendChild(value, nullptr);
        }
    }
    const int catalogVersion = static_cast<int>(state.getProperty("preset_catalog_version", 1));
    const int savedProgram = static_cast<int>(state.getProperty("program", 0));
    currentProgram_ = catalogVersion < 2 ? 0 : juce::jlimit(0, std::max(0, getNumPrograms() - 1), savedProgram);
    // Older sessions saved only the program. Their settings can differ from it, so they show as edited.
    const juce::String programName = openedm::presets::factoryPresets()[static_cast<size_t>(currentProgram_.load())].name;
    setPreset(state.getProperty("presetName", programName).toString(), static_cast<bool>(state.getProperty("presetEdited", true)));
    parameters.replaceState(state);
    const int width = static_cast<int>(state.getProperty("editorWidth", 0));
    const int height = static_cast<int>(state.getProperty("editorHeight", 0));
    if (width >= kMinEditorWidth && width <= kMaxEditorWidth)
        editorWidth_ = width;
    if (height >= kMinEditorHeight && height <= kMaxEditorHeight)
        editorHeight_ = height;
    modulationPageVisible = static_cast<bool>(state.getProperty("modulationPage", false));
    const auto fallback = editHistory_.captureParameters();
    if (auto ab = state.getChildWithName("AB"); ab.isValid())
        ab_.fromValueTree(ab, fallback);
    else
        ab_.reset(fallback);
    editHistory_.clear();
    editHistory_.selectHistory(ab_.active());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OpenEdmFilterProcessor();
}
