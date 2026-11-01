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
          return ParameterValues{
              { bandValues(0), bandValues(1) },
              raw(params::kFmRate), raw(params::kFmDepth), raw(params::kDrive), raw(params::kDrivePosition),
              raw(params::kCrushRate), raw(params::kCrushBits), raw(params::kCrushMix), raw(params::kCrushPosition),
              raw(params::kCrushLink), raw(params::kRingFrequency), raw(params::kRingMix), raw(params::kMix),
              raw(params::kOutput), raw(params::kLimiter), raw(params::kCeiling), raw(params::kInertia),
              raw(params::kClockSync), raw(params::kTempoBpm), raw(params::kTimeSignature),
              raw(params::kSweepEnabled), raw(params::kSweepTarget), raw(params::kSweepBars), raw(params::kSweepMode),
              raw(params::kSweepCutoffStart), raw(params::kSweepCutoffEnd), raw(params::kSweepCutoffSlope),
              raw(params::kSweepResStart), raw(params::kSweepResEnd), raw(params::kSweepResSlope)
          };
      }()) {
    setLatencySamples(core::FilterEngine::kLatencySamples);
}

void OpenEdmFilterProcessor::prepareToPlay(double sampleRate, int) {
    engine_.prepare(sampleRate);
}

bool OpenEdmFilterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto output = layouts.getMainOutputChannelSet();
    const bool isMonoOrStereo = output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
    return isMonoOrStereo && layouts.getMainInputChannelSet() == output;
}

core::AutoSweepSettings OpenEdmFilterProcessor::currentSweepSettings() const noexcept {
    const auto isOn = [](const std::atomic<float>* value) { return value->load() > 0.5f; };
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };
    const auto barIndex = static_cast<size_t>(std::clamp(choice(values_.sweepBars), 0, static_cast<int>(params::kSweepBarCounts.size()) - 1));
    return core::AutoSweepSettings{
        isOn(values_.sweepEnabled),
        static_cast<core::SweepTarget>(choice(values_.sweepTarget)),
        params::kSweepBarCounts[barIndex],
        static_cast<core::SweepMode>(choice(values_.sweepMode)),
        values_.sweepCutoffStart->load(),
        values_.sweepCutoffEnd->load(),
        values_.sweepCutoffSlope->load(),
        values_.sweepResStart->load(),
        values_.sweepResEnd->load(),
        values_.sweepResSlope->load()
    };
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
        currentSweepSettings()
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
        if (!position->getIsPlaying() || !position->getPpqPosition().hasValue()) {
            return core::Transport{ bpm, std::nullopt, beatsPerBar };
        }
        return core::Transport{ bpm, *position->getPpqPosition(), beatsPerBar };
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
    const auto xml = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void OpenEdmFilterProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) {
        return;
    }
    const auto state = juce::ValueTree::fromXml(*xml);
    currentProgram_ = state.getProperty("program", 0);
    parameters.replaceState(state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OpenEdmFilterProcessor();
}
