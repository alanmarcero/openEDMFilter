#pragma once

#include "EdmLookAndFeel.h"
#include "Panels.h"
#include "PluginProcessor.h"

class OpenEdmFilterEditor : public juce::AudioProcessorEditor {
public:
    explicit OpenEdmFilterEditor(OpenEdmFilterProcessor& processor);
    ~OpenEdmFilterEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    OpenEdmFilterProcessor& processor_;
    openedm::ui::EdmLookAndFeel lookAndFeel_;
    juce::Image faceplate_;
    juce::ComboBox presetBox_;
    juce::TextButton prevPresetBtn_{"<"};
    juce::TextButton nextPresetBtn_{">"};
    openedm::ui::Toggle masterBypass_;
    openedm::ui::Toggle inertia_;
    openedm::ui::ResponseView response_;
    openedm::ui::SweepPanel sweep_;
    openedm::ui::BandPanel bandA_;
    openedm::ui::BandPanel bandB_;
    openedm::ui::EffectsPanel effects_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenEdmFilterEditor)
};
