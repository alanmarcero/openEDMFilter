#pragma once

#include "FilterEngine.h"
#include "ABCompare.h"
#include "ParameterUndo.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class OpenEdmFilterProcessor : public juce::AudioProcessor {
public:
    static constexpr size_t kSpectrumDisplayBins = 128;

    static constexpr int kDefaultEditorWidth = 1180;
    static constexpr int kDefaultEditorHeight = 700;
    static constexpr int kMinEditorWidth = 1100;
    static constexpr int kMinEditorHeight = 640;
    static constexpr int kMaxEditorWidth = 1800;
    static constexpr int kMaxEditorHeight = 1200;

    OpenEdmFilterProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram_; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    [[nodiscard]] openedm::core::AutoSweepSettings currentSweepSettings() const noexcept;
    [[nodiscard]] int currentSweepSlot() const noexcept;

    void undoEdit() { editHistory_.undo(); }
    void redoEdit() { editHistory_.redo(); }
    [[nodiscard]] bool canUndoEdit() const { return editHistory_.canUndo(); }
    [[nodiscard]] bool canRedoEdit() const { return editHistory_.canRedo(); }
    void flushEdits() { editHistory_.flush(); }
    void editParameters(const std::function<void()>& edit, const juce::String& name) { editHistory_.performEdit(edit, name); }
    void toggleAB();
    void copyBetweenSlots(int from, int to);
    [[nodiscard]] int abSlot() const noexcept { return ab_.active(); }
    void setEditorSize(int width, int height) noexcept;
    [[nodiscard]] int editorWidth() const noexcept { return editorWidth_.load(); }
    [[nodiscard]] int editorHeight() const noexcept { return editorHeight_.load(); }
    [[nodiscard]] bool isSidechainConnected() const noexcept;
    [[nodiscard]] juce::ValueTree parameterSnapshot() const { return editHistory_.captureParameters(); }
    void applyParameterSnapshot(const juce::ValueTree& snapshot, const juce::String& name);

    juce::AudioProcessorValueTreeState parameters;

    // Modulated cutoffs, resonances, sweep phase and active tempo for the UI, written by the audio thread.
    std::array<std::atomic<float>, openedm::core::FilterEngine::kNumBands> displayCutoffs{};
    std::array<std::atomic<float>, openedm::core::FilterEngine::kNumBands> displayResonances{ -1.0f, -1.0f };
    std::atomic<float> displaySweepPhase{ 0.0f };
    std::atomic<float> displayBeatPhase{ -1.0f };
    float animationBeatPhase(double seconds) const noexcept;
    std::atomic<float> displayBpm{ 128.0f };
    std::atomic<float> displayBeatsPerBar{ 4.0f };
    std::atomic<float> displayInputPeak{ 0.0f };
    std::atomic<float> displayOutputPeak{ 0.0f };
    std::array<std::atomic<float>, kSpectrumDisplayBins> displaySpectrum{};
    std::atomic<bool> spectrumVisible{ true };
    std::atomic<bool> spectrumFrozen{ false };

private:
    static constexpr int kSpectrumFftOrder = 11;
    static constexpr int kSpectrumFftSize = 1 << kSpectrumFftOrder;

    struct BandParameters {
        std::atomic<float>* bypass;
        std::atomic<float>* type;
        std::atomic<float>* extendedType;
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
        std::atomic<float>* envInput;
    };

    struct SweepParameters {
        std::atomic<float>* sweepTarget;
        std::atomic<float>* sweepBars;
        std::atomic<float>* sweepMode;
        std::atomic<float>* sweepCutoffStart;
        std::atomic<float>* sweepCutoffEnd;
        std::atomic<float>* sweepCutoffSlope;
        std::atomic<float>* sweepResStart;
        std::atomic<float>* sweepResEnd;
        std::atomic<float>* sweepResSlope;
        std::array<std::atomic<float>*, 8> effects;
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
        std::atomic<float>* masterBypass;
        std::atomic<float>* clockSync;
        std::atomic<float>* tempoBpm;
        std::atomic<float>* timeSignature;
        std::atomic<float>* sweepEnabled;
        std::atomic<float>* sweepBypass;
        std::atomic<float>* sweepSlot;
        std::array<SweepParameters, 4> sweeps;
        std::atomic<float>* inputGain;
        std::atomic<float>* subPreserve;
        std::atomic<float>* analogDrift;
        std::atomic<float>* stereoSpread;
    };

    struct ModulationParameters {
        std::atomic<float>* midiSource{};
        std::atomic<float>* midiCC{};
        std::atomic<float>* midiDepth{};
        std::atomic<float>* stepMode{};
        std::atomic<float>* stepLength{};
        std::atomic<float>* stepPulses{};
        std::atomic<float>* stepRotation{};
        std::atomic<float>* stepBeats{};
        std::atomic<float>* stepDepth{};
        std::atomic<float>* lane2Depth{};
        std::atomic<float>* lane2Phase{};
        std::atomic<float>* lane2Offset{};
        std::array<std::atomic<float>*, 16> steps{};
    };

    suite::ABCompare ab_;
    suite::ParameterUndo editHistory_;
    std::atomic<int> editorWidth_ { kDefaultEditorWidth };
    std::atomic<int> editorHeight_ { kDefaultEditorHeight };
    ParameterValues values_;
    std::array<ModulationParameters, 2> modulation_{};
    std::atomic<float>* routing_{};
    std::atomic<float>* parallelBlend_{};
    std::atomic<float>* fmShape_{};
    std::atomic<float>* fmTarget_{};
    std::atomic<float>* stereoMode_{};
    // Per-note pressure and held velocity make releases deterministic for overlapping notes.
    std::array<std::array<float, 128>, 16> heldVelocities_{};
    std::array<std::array<float, 128>, 16> notePressure_{};
    std::array<float, 16> channelPressure_{};
    struct ControllerValue {
        float value = 0.0f;
        std::uint64_t order = 0;
    };
    std::array<std::array<ControllerValue, 128>, 16> controllers_{};
    std::uint64_t controllerOrder_ = 0;
    void handleMidi(const juce::MidiMessage& message) noexcept;
    void updateInputAnalysis(const juce::AudioBuffer<float>& buffer, float inputGain) noexcept;
    void updateOutputMeter(const juce::AudioBuffer<float>& buffer) noexcept;
    void updateSpectrumFrame() noexcept;
    void updatePeakMeter(std::atomic<float>& meter, float peak, int samples) noexcept;
    openedm::core::FilterEngine engine_;
    std::unique_ptr<juce::dsp::FFT> spectrumFft_;
    std::array<std::array<float, kSpectrumFftSize>, 2> spectrumFifo_{};
    std::array<float, kSpectrumFftSize * 2> spectrumFftData_{};
    std::array<float, kSpectrumFftSize> spectrumWindow_{};
    size_t spectrumWriteIndex_ = 0;
    float spectrumWindowSum_ = 0.0f;
    std::atomic<int> currentProgram_{ 0 };
    double processingSampleRate_ = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenEdmFilterProcessor)
};
