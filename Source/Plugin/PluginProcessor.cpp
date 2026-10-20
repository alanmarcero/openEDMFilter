#include "PluginProcessor.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "Presets.h"

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
              raw(params::kOutput), raw(params::kLimiter), raw(params::kCeiling), raw(params::kInertia)
          };
      }()) {
}

void OpenEdmFilterProcessor::prepareToPlay(double sampleRate, int) {
    engine_.prepare(sampleRate);
}

bool OpenEdmFilterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto output = layouts.getMainOutputChannelSet();
    const bool isMonoOrStereo = output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
    return isMonoOrStereo && layouts.getMainInputChannelSet() == output;
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
        isOn(values_.inertia)
    };

    const core::Transport transport = [this] {
        auto* playHead = getPlayHead();
        if (playHead == nullptr) {
            return core::Transport{};
        }
        const auto position = playHead->getPosition();
        if (!position.hasValue()) {
            return core::Transport{};
        }
        const double bpm = position->getBpm().orFallback(120.0);
        if (!position->getIsPlaying() || !position->getPpqPosition().hasValue()) {
            return core::Transport{ bpm, std::nullopt };
        }
        return core::Transport{ bpm, *position->getPpqPosition() };
    }();

    engine_.process(settings, transport, buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    const auto cutoffs = engine_.currentCutoffs();
    displayCutoffs[0].store(cutoffs[0]);
    displayCutoffs[1].store(cutoffs[1]);
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
