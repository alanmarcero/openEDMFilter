#pragma once

#include "EdmLookAndFeel.h"
#include "Panels.h"
#include "PluginProcessor.h"

class OpenEdmFilterEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit OpenEdmFilterEditor(OpenEdmFilterProcessor& processor);
    ~OpenEdmFilterEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    OpenEdmFilterProcessor& processor_;
    openedm::ui::EdmLookAndFeel lookAndFeel_;
    juce::Image faceplate_;
    std::unique_ptr<juce::Drawable> pulse_;
    juce::ComboBox presetBox_;
    juce::TextButton advancedButton_{"MODULATION / ROUTING"};
    juce::TextButton spectrumButton_{"SPECTRUM"};
    juce::TextButton freezeSpectrumButton_{"FREEZE"};
    std::unique_ptr<juce::GenericAudioProcessorEditor> advancedControls_;
    juce::TextButton prevPresetBtn_{"PREV"};
    juce::TextButton nextPresetBtn_{"NEXT"};
    openedm::ui::Toggle masterBypass_;
    openedm::ui::Toggle inertia_;
    openedm::ui::ResponseView response_;
    openedm::ui::SweepPanel sweep_;
    openedm::ui::BandPanel bandA_;
    openedm::ui::BandPanel bandB_;
    openedm::ui::EffectsPanel effects_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenEdmFilterEditor)
};
