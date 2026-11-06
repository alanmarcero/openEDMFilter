#pragma once

#include "Controls.h"
#include "PluginProcessor.h"
#include <array>

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
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    OpenEdmFilterProcessor& processor_;
    Toggle bypass_;
    Toggle enabled_;
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
    juce::Rectangle<int> slotArea_;
    void selectSlot(int slot);
    juce::Rectangle<int> clockArea_;
    juce::Rectangle<int> editorArea_;
    juce::Rectangle<int> cutoffArea_;
    juce::Rectangle<int> resArea_;
    juce::ParameterAttachment bypassWatcher_; // dims the sweep controls while it is bypassed
    juce::ParameterAttachment slotWatcher_;
};

class BandPanel : public juce::Component {
public:
    BandPanel(Apvts& state, int band);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::String title_;
    juce::Colour accent_;
    Toggle bypass_;
    Choice type_;
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
    juce::Rectangle<int> filterArea_;
    juce::Rectangle<int> lfoArea_;
    juce::Rectangle<int> envelopeArea_;
    juce::ParameterAttachment bypassWatcher_; // dims the band's controls while it is bypassed
};

class EffectsPanel : public juce::Component {
public:
    explicit EffectsPanel(Apvts& state);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct Section {
        juce::String title;
        juce::Colour accent;
        int units;
        juce::Rectangle<int> area;
    };

    Knob drive_;
    Choice drivePosition_;
    Knob crushRate_;
    Knob crushBits_;
    Knob crushMix_;
    Choice crushPosition_;
    Toggle crushLink_;
    Knob ringFrequency_;
    Knob ringMix_;
    Knob fmRate_;
    Knob fmDepth_;
    Knob mix_;
    Knob output_;
    Knob ceiling_;
    Toggle limiter_;
    std::array<Section, 5> sections_;
};

} // namespace openedm::ui
