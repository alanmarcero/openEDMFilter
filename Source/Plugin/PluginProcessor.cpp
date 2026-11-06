#include "PluginProcessor.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <algorithm>

namespace core = openedm::core;
namespace params = openedm::params;

OpenEdmFilterProcessor::OpenEdmFilterProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "openEDMFilter", params::createParameterLayout()),
      values_([this] {
          const auto raw = [this](const juce::String& id) { return parameters.getRawParameterValue(id); };
          const auto bandValues = [&](int band) {
              const auto bandRaw = [&](const char* suffix) { return raw(params::bandId(band, suffix)); };
              return BandParameters{
                  bandRaw(params::kBypass), bandRaw(params::kType), bandRaw(params::kSlope), bandRaw(params::kCutoff),
                  bandRaw(params::kResonance), bandRaw(params::kWidth), bandRaw(params::kLfoShape),
                  bandRaw(params::kLfoRate), bandRaw(params::kLfoDepth), bandRaw(params::kEnvSource),
                  bandRaw(params::kEnvAttack), bandRaw(params::kEnvRelease), bandRaw(params::kEnvDepth)
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
              { sweepValues(0), sweepValues(1), sweepValues(2), sweepValues(3) }
          };
      }()) {
    setLatencySamples(core::FilterEngine::kLatencySamples);
    parameters.state.setProperty("barsLayout", params::kSweepBarsLayout, nullptr);
}

void OpenEdmFilterProcessor::prepareToPlay(double sampleRate, int) {
    engine_.prepare(sampleRate);
}

bool OpenEdmFilterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto output = layouts.getMainOutputChannelSet();
    const bool isMonoOrStereo = output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
    return isMonoOrStereo && layouts.getMainInputChannelSet() == output;
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

void OpenEdmFilterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const auto isOn = [](const std::atomic<float>* value) { return value->load() > 0.5f; };
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };

    const auto bandSettings = [&](const BandParameters& band) {
        return core::BandSettings{
            core::FilterShape{ !isOn(band.bypass), static_cast<core::FilterType>(choice(band.type)),
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
    };

    const core::EngineSettings settings{
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

    const bool syncToHost = isOn(values_.clockSync);
    const double manualBpm = static_cast<double>(values_.tempoBpm->load());
    const int sigChoice = choice(values_.timeSignature);
    const double manualBeatsPerBar = params::beatsPerBarForSignature(sigChoice);

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
        const double bpm = position->getBpm().orFallback(manualBpm);
        double beatsPerBar = manualBeatsPerBar;
        if (sigChoice == 0 && position->getTimeSignature().hasValue()) {
            const auto sig = *position->getTimeSignature();
            if (sig.numerator > 0 && sig.denominator > 0) {
                beatsPerBar = static_cast<double>(sig.numerator) * (4.0 / static_cast<double>(sig.denominator));
            }
        }
        if (!position->getPpqPosition().hasValue()) {
            return core::Transport{ bpm, std::nullopt, beatsPerBar, true };
        }
        // Keep the playhead while stopped so the sweep holds instead of free-running.
        return core::Transport{ bpm, *position->getPpqPosition(), beatsPerBar, position->getIsPlaying() };
    }();

    engine_.process(settings, transport, buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    const auto cutoffs = engine_.currentCutoffs();
    const auto resonances = engine_.currentResonances();
    displayCutoffs[0].store(cutoffs[0]);
    displayCutoffs[1].store(cutoffs[1]);
    displayResonances[0].store(resonances[0]);
    displayResonances[1].store(resonances[1]);
    displaySweepPhase.store(engine_.currentSweepPhase());
    displayBpm.store(static_cast<float>(transport.bpm));
    displayBeatsPerBar.store(static_cast<float>(transport.beatsPerBar));
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
    currentProgram_ = index;
    openedm::presets::apply(presets[static_cast<size_t>(index)], parameters);
}

const juce::String OpenEdmFilterProcessor::getProgramName(int index) {
    const auto& presets = openedm::presets::factoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) {
        return {};
    }
    return presets[static_cast<size_t>(index)].name;
}

void OpenEdmFilterProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    state.setProperty("program", currentProgram_, nullptr);
    state.setProperty("barsLayout", params::kSweepBarsLayout, nullptr);
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
    currentProgram_ = state.getProperty("program", 0);
    parameters.replaceState(state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OpenEdmFilterProcessor();
}
