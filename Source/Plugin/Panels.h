#pragma once

#include "Controls.h"
#include "PluginProcessor.h"
#include <array>

namespace openedm::ui {

// Live response curve of both filters, including LFO, envelope and FM movement.
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

    OpenEdmFilterProcessor& processor_;
    juce::RangedAudioParameter* dragCutoff_ = nullptr;
    juce::RangedAudioParameter* dragResonance_ = nullptr;
    float resonanceAtDragStart_ = 0.0f;
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
