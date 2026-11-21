#pragma once

#include "EdmLookAndFeel.h"
#include "Panels.h"
#include "PluginProcessor.h"
#include "PresetBrowser.h"
#include "PresetStore.h"
#include "SuiteBar.h"
#include <memory>

class OpenEdmFilterEditor : public juce::AudioProcessorEditor,
                            public juce::Timer,
                            private juce::KeyListener {
public:
    explicit OpenEdmFilterEditor(OpenEdmFilterProcessor& processor);
    ~OpenEdmFilterEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    bool keyPressed(const juce::KeyPress& key, juce::Component* originating) override;
    void timerCallback() override;

    OpenEdmFilterProcessor& processor_;
    openedm::ui::EdmLookAndFeel lookAndFeel_;
    juce::TooltipWindow tooltips_{ this, 500 };
    juce::Image faceplate_;
    std::unique_ptr<juce::Drawable> pulse_;
    suite::PresetStore presetStore_;
    suite::SuiteBar suiteBar_;
    std::unique_ptr<suite::PresetBrowser> presetBrowser_;
    bool presetBrowserVisible_ = false;
    bool applyingStateSize_ = false;
    juce::ComboBox presetBox_;
    juce::TextButton advancedButton_{"MODULATION"};
    juce::TextButton spectrumButton_{"SPECTRUM"};
    juce::TextButton freezeSpectrumButton_{"FREEZE"};
    juce::TextButton prevPresetBtn_{"<"};
    juce::TextButton nextPresetBtn_{">"};
    openedm::ui::Toggle masterBypass_;
    openedm::ui::Toggle inertia_;
    openedm::ui::ResponseView response_;
    openedm::ui::SweepPanel sweep_;
    openedm::ui::BandPanel bandA_;
    openedm::ui::BandPanel bandB_;
    openedm::ui::EffectsPanel effects_;
    openedm::ui::ModulationPage modulationPage_;
    juce::Rectangle<int> pulseArea_;
    juce::Rectangle<int> titleArea_;
    int headerHeight_ = 40;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenEdmFilterEditor)
};
