#pragma once

#include "FilterEngine.h"
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

class OpenEdmFilterProcessor : public juce::AudioProcessor {
public:
    OpenEdmFilterProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram_; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;

    // Modulated cutoffs for the response display, written by the audio thread.
    std::array<std::atomic<float>, openedm::core::FilterEngine::kNumBands> displayCutoffs{};

private:
    struct BandParameters {
        std::atomic<float>* bypass;
        std::atomic<float>* type;
        std::atomic<float>* slope;
        std::atomic<float>* cutoff;
        std::atomic<float>* resonance;
        std::atomic<float>* width;
        std::atomic<float>* lfoShape;
        std::atomic<float>* lfoRate;
        std::atomic<float>* lfoDepth;
        std::atomic<float>* envSource;
        std::atomic<float>* envAttack;
        std::atomic<float>* envRelease;
        std::atomic<float>* envDepth;
    };

    // Raw parameter values, looked up once so the audio thread never builds ID strings.
    struct ParameterValues {
        std::array<BandParameters, openedm::core::FilterEngine::kNumBands> bands;
        std::atomic<float>* fmRate;
        std::atomic<float>* fmDepth;
        std::atomic<float>* drive;
        std::atomic<float>* drivePosition;
        std::atomic<float>* crushRate;
        std::atomic<float>* crushBits;
        std::atomic<float>* crushMix;
        std::atomic<float>* crushPosition;
        std::atomic<float>* crushLink;
        std::atomic<float>* ringFrequency;
        std::atomic<float>* ringMix;
        std::atomic<float>* mix;
        std::atomic<float>* output;
        std::atomic<float>* limiter;
        std::atomic<float>* ceiling;
        std::atomic<float>* inertia;
    };

    ParameterValues values_;
    openedm::core::FilterEngine engine_;
    int currentProgram_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenEdmFilterProcessor)
};
