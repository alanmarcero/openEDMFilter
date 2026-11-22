#pragma once

#include "Controls.h"
#include "PluginProcessor.h"
#include <array>
#include <vector>

namespace openedm::ui {

// Live response curve of both filters, including LFO, envelope, FM and multi-bar auto-sweep movement.
// Dragging a band's marker sets its cutoff (horizontal) and resonance (vertical).
class ResponseView : public juce::Component, private juce::Timer {
public:
    explicit ResponseView(OpenEdmFilterProcessor& processor);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    // The dB grid values whose labels fit the plot without touching each other or a readout, 0 dB first.
    [[nodiscard]] static std::vector<float> labelledDecibels(juce::Rectangle<float> plot,
                                                             const std::array<juce::Rectangle<float>, 2>& readouts);
    // A band readout beside its marker, inside the plot and clear of the other readout.
    [[nodiscard]] static juce::Rectangle<float> readoutBounds(juce::Point<float> marker, float width,
                                                             juce::Rectangle<float> plot, juce::Rectangle<float> other);

private:
    void timerCallback() override { repaint(); }
    [[nodiscard]] juce::Rectangle<float> plotArea() const;
    [[nodiscard]] float xForHz(double hz) const;
    [[nodiscard]] double displayCutoff(int band) const;
    [[nodiscard]] float displayResonance(int band) const;

    OpenEdmFilterProcessor& processor_;
    juce::RangedAudioParameter* dragCutoff_ = nullptr;
    juce::RangedAudioParameter* dragResonance_ = nullptr;
    float resonanceAtDragStart_ = 0.0f;
};

// Interactive multi-bar slope editor for cutoff and resonance trajectories.
// Dragging the start, midpoint or end handles edits the cutoff and resonance slopes in real time.
class SweepSlopeEditor : public juce::Component, private juce::Timer {
public:
    explicit SweepSlopeEditor(OpenEdmFilterProcessor& processor);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void endDrag();

private:
    enum class Handle {
        None,
        CutoffStart,
        CutoffSlope,
        CutoffEnd,
        ResStart,
        ResSlope,
        ResEnd
    };

    void timerCallback() override { repaint(); }
    [[nodiscard]] static const char* parameterIdForHandle(Handle handle) noexcept;
    [[nodiscard]] juce::Rectangle<float> plotArea() const;
    [[nodiscard]] float yForHz(float hz) const;
    [[nodiscard]] float hzForY(float y) const;
    [[nodiscard]] float yForNormalized(float value) const;
    [[nodiscard]] float normalizedForY(float y) const;
    [[nodiscard]] std::pair<float, float> handlePhases(openedm::core::SweepMode mode, int bars) const;
    [[nodiscard]] juce::Point<float> handlePosition(Handle handle, const openedm::core::AutoSweepSettings& settings) const;

    OpenEdmFilterProcessor& processor_;
    Handle activeHandle_ = Handle::None;
    juce::RangedAudioParameter* activeParameter_ = nullptr;
    float slopeAtDragStart_ = 0.0f;
    int dragSlot_ = 0;
};

class SweepPanel : public juce::Component {
public:
    explicit SweepPanel(OpenEdmFilterProcessor& processor);
    ~SweepPanel() override;
    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void resized() override;
    void refresh();

private:
    OpenEdmFilterProcessor& processor_;
    juce::TextButton enabled_{ "AUTO SWEEP" };
    Toggle clockSync_;
    Choice timeSig_;
    Choice bars_;
    juce::Label barsLabel_;
    Choice mode_;
    Choice target_;
    juce::ComboBox slopePresetBox_;
    Knob tempo_;
    SweepSlopeEditor slopeEditor_;
    Knob cutoffStart_;
    Knob cutoffEnd_;
    Knob cutoffSlope_;
    Knob resStart_;
    Knob resEnd_;
    Knob resSlope_;
    std::array<std::unique_ptr<Knob>, 8> effectKnobs_;
    std::array<juce::TextButton, 4> slotButtons_;
    juce::Rectangle<int> crushArea_;
    juce::Rectangle<int> glitchArea_;
    void selectSlot(int slot);
    juce::Rectangle<int> clockArea_;
    juce::String clockCaption_{ "Clock" };
    juce::Rectangle<int> editorArea_;
    juce::Rectangle<int> cutoffArea_;
    juce::Rectangle<int> resArea_;
    juce::ParameterAttachment sweepOn_;
    juce::ParameterAttachment sweepBypass_;
    juce::ParameterAttachment slotWatcher_;
};

class BandPanel : public juce::Component {
public:
    BandPanel(Apvts& state, int band);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void refresh(bool isSweepDriven);

private:
    juce::String title_;
    juce::Colour accent_;
    Toggle bypass_;
    FilterTypeMenu type_;
    Choice slope_;
    Knob cutoff_;
    Knob resonance_;
    Knob width_;
    Choice lfoShape_;
    Choice lfoRate_;
    Knob lfoDepth_;
    Knob envSource_;
    Knob envAttack_;
    Knob envRelease_;
    Knob envDepth_;
    Choice envListen_;
    juce::Rectangle<int> filterArea_;
    juce::Rectangle<int> lfoArea_;
    juce::Rectangle<int> envelopeArea_;
    const std::atomic<float>& bypassValue_;
    const std::atomic<float>& masterBypassValue_;
};

// Sixteen bipolar step values for one filter. A drag across the bars draws them.
class StepGrid : public juce::Component, public juce::SettableTooltipClient {
public:
    StepGrid(Apvts& state, int band, juce::Colour accent);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

private:
    Apvts& state_;
    juce::Colour accent_;
    juce::RangedAudioParameter& length_;
    juce::ParameterAttachment lengthWatcher_;
    std::array<juce::RangedAudioParameter*, 16> values_{};
    std::vector<std::unique_ptr<juce::ParameterAttachment>> stepAttachments_;
    std::array<bool, 16> isDrawing_{};
    int lastStep_ = -1;
};

// Steps, MIDI and the right or side lane for one filter.
class BandModulation : public juce::Component {
public:
    BandModulation(Apvts& state, int band);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void refresh(bool isLinked, bool isAllBypassed);

private:
    juce::String title_;
    juce::Colour accent_;
    Choice stepMode_;
    Knob stepLength_;
    Knob stepBeats_;
    Knob stepPulses_;
    Knob stepRotation_;
    Knob stepDepth_;
    StepGrid steps_;
    Choice midiSource_;
    Knob midiCc_;
    Knob midiDepth_;
    Knob laneDepth_;
    Knob lanePhase_;
    Knob laneOffset_;
    const std::atomic<float>& stepModeValue_;
    const std::atomic<float>& midiSourceValue_;
    juce::Rectangle<int> stepsArea_;
    juce::Rectangle<int> gridArea_;
    juce::Rectangle<int> midiArea_;
    juce::Rectangle<int> laneArea_;
};

class ModulationPage : public juce::Component {
public:
    explicit ModulationPage(Apvts& state);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void refresh();

private:
    Choice routing_;
    Knob blend_;
    Choice channels_;
    Choice fmShape_;
    Choice fmTarget_;
    BandModulation bandA_;
    BandModulation bandB_;
    const std::atomic<float>& routingValue_;
    const std::atomic<float>& channelsValue_;
    const std::atomic<float>& masterBypassValue_;
    juce::Rectangle<int> globalArea_;
};

class EffectsPanel : public juce::Component, public juce::Timer {
public:
    explicit EffectsPanel(OpenEdmFilterProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    struct Section {
        juce::String title;
        juce::Colour accent;
        int units;
        juce::Rectangle<int> area;
    };

    OpenEdmFilterProcessor& processor_;
    Knob inputGain_;
    Knob subPreserve_;
    Knob drive_;
    Choice drivePosition_;
    Knob analogDrift_;
    Knob crushRate_;
    Knob crushBits_;
    Knob crushMix_;
    Choice crushPosition_;
    Toggle crushLink_;
    Knob ringFrequency_;
    Knob ringMix_;
    Knob fmRate_;
    Knob fmDepth_;
    Knob stereoSpread_;
    Knob mix_;
    Knob output_;
    Knob ceiling_;
    Toggle limiter_;
    std::array<Section, 6> sections_;
    LevelMeter inputMeter_;
    LevelMeter outputMeter_;
};

} // namespace openedm::ui
