#include "PluginEditor.h"
#include "Parameters.h"
#include "Presets.h"
#include "Theme.h"
#include "BinaryData.h"

namespace {

namespace theme = openedm::theme;

constexpr int kEditorWidth = 1200;
constexpr int kEditorHeight = 1000;
constexpr int kMargin = 12;
constexpr int kHeaderHeight = 40;
constexpr int kResponseHeight = 140;
constexpr int kSweepHeight = 320;
constexpr int kBandHeight = 220;

} // namespace

OpenEdmFilterEditor::OpenEdmFilterEditor(OpenEdmFilterProcessor& processorToEdit)
    : AudioProcessorEditor(processorToEdit),
      processor_(processorToEdit),
      masterBypass_(processorToEdit.parameters, openedm::params::kMasterBypass, "BYPASS", theme::kAccentOut),
      inertia_(processorToEdit.parameters, openedm::params::kInertia, "INERTIA", theme::kText),
      response_(processorToEdit),
      sweep_(processorToEdit),
      bandA_(processorToEdit.parameters, 0),
      bandB_(processorToEdit.parameters, 1),
      effects_(processorToEdit) {
    setLookAndFeel(&lookAndFeel_);
    faceplate_ = juce::ImageCache::getFromMemory(BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize);

    const auto& presets = openedm::presets::factoryPresets();
    std::ranges::for_each(presets, [&, id = 1](const auto& preset) mutable { presetBox_.addItem(preset.name, id++); });
    presetBox_.setColour(juce::ComboBox::outlineColourId, theme::kText);
    presetBox_.setSelectedId(processor_.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox_.onChange = [this] { processor_.setCurrentProgram(presetBox_.getSelectedId() - 1); };

    prevPresetBtn_.setComponentID("prevPreset");
    prevPresetBtn_.setTooltip("Previous preset");
    prevPresetBtn_.setColour(juce::TextButton::buttonOnColourId, theme::kAccentA);
    prevPresetBtn_.onClick = [this] {
        const int num = processor_.getNumPrograms();
        if (num <= 0) return;
        const int cur = processor_.getCurrentProgram();
        const int nextProg = (cur <= 0 ? num : cur) - 1;
        processor_.setCurrentProgram(nextProg);
        presetBox_.setSelectedId(nextProg + 1, juce::dontSendNotification);
    };

    nextPresetBtn_.setComponentID("nextPreset");
    nextPresetBtn_.setTooltip("Next preset");
    nextPresetBtn_.setColour(juce::TextButton::buttonOnColourId, theme::kAccentA);
    nextPresetBtn_.onClick = [this] {
        const int num = processor_.getNumPrograms();
        if (num <= 0) return;
        const int cur = processor_.getCurrentProgram();
        const int nextProg = (cur + 1) % num;
        processor_.setCurrentProgram(nextProg);
        presetBox_.setSelectedId(nextProg + 1, juce::dontSendNotification);
    };

    advancedButton_.setComponentID("advancedModulation");
    advancedButton_.setTooltip("Routing, FM, MIDI, step patterns and independent R/S modulation");
    advancedButton_.onClick = [this] {
        if (!advancedControls_) {
            advancedControls_ = std::make_unique<juce::GenericAudioProcessorEditor>(processor_);
            addChildComponent(*advancedControls_);
        }
        const bool show = !advancedControls_->isVisible();
        advancedControls_->setVisible(show);
        if (show) advancedControls_->toFront(false);
        advancedButton_.setButtonText(show ? "CLOSE CONTROLS" : "MODULATION / ROUTING");
        resized();
    };
    spectrumButton_.setClickingTogglesState(true);
    spectrumButton_.setToggleState(processor_.spectrumVisible.load(), juce::dontSendNotification);
    spectrumButton_.setTooltip("Show or hide the live input spectrum in the response display");
    spectrumButton_.setColour(juce::TextButton::buttonOnColourId, theme::kAccentSpectrum);
    spectrumButton_.onClick = [this] { processor_.spectrumVisible.store(spectrumButton_.getToggleState()); };
    freezeSpectrumButton_.setClickingTogglesState(true);
    freezeSpectrumButton_.setToggleState(processor_.spectrumFrozen.load(), juce::dontSendNotification);
    freezeSpectrumButton_.setTooltip("Hold the current spectrum trace");
    freezeSpectrumButton_.setColour(juce::TextButton::buttonOnColourId, theme::kAccentB);
    freezeSpectrumButton_.onClick = [this] { processor_.spectrumFrozen.store(freezeSpectrumButton_.getToggleState()); };
    addAndMakeVisible(advancedButton_);
    addAndMakeVisible(spectrumButton_);
    addAndMakeVisible(freezeSpectrumButton_);
    addAndMakeVisible(presetBox_);
    addAndMakeVisible(prevPresetBtn_);
    addAndMakeVisible(nextPresetBtn_);
    addAndMakeVisible(masterBypass_);
    addAndMakeVisible(inertia_);
    addAndMakeVisible(response_);
    addAndMakeVisible(sweep_);
    addAndMakeVisible(bandA_);
    addAndMakeVisible(bandB_);
    addAndMakeVisible(effects_);
    setSize(kEditorWidth, kEditorHeight);
}

OpenEdmFilterEditor::~OpenEdmFilterEditor() {
    setLookAndFeel(nullptr);
}

void OpenEdmFilterEditor::paint(juce::Graphics& g) {
    g.fillAll(theme::kBackground);
    g.setOpacity(0.12f);
    g.drawImage(faceplate_, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    g.setOpacity(1.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4), 5, 1);

    const auto header = getLocalBounds().reduced(kMargin, 0).removeFromTop(kHeaderHeight + kMargin).withTrimmedTop(kMargin);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.setColour(theme::kText);
    g.drawText("open", header, juce::Justification::centredLeft);
    const int openWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "open");
    g.setColour(theme::kAccentA);
    g.drawText("EDM", header.withTrimmedLeft(openWidth), juce::Justification::centredLeft);
    const int edmWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "EDM");
    g.setColour(theme::kAccentB);
    g.drawText("Filter", header.withTrimmedLeft(openWidth + edmWidth), juce::Justification::centredLeft);
}

void OpenEdmFilterEditor::resized() {
    auto bounds = getLocalBounds().reduced(kMargin);
    auto header = bounds.removeFromTop(kHeaderHeight);
    nextPresetBtn_.setBounds(header.removeFromRight(58).withSizeKeepingCentre(58, 36));
    header.removeFromRight(4);
    presetBox_.setBounds(header.removeFromRight(180).withSizeKeepingCentre(180, 36));
    header.removeFromRight(4);
    prevPresetBtn_.setBounds(header.removeFromRight(58).withSizeKeepingCentre(58, 36));
    header.removeFromRight(4);
    freezeSpectrumButton_.setBounds(header.removeFromRight(78).withSizeKeepingCentre(78, 36));
    header.removeFromRight(4);
    spectrumButton_.setBounds(header.removeFromRight(88).withSizeKeepingCentre(88, 36));
    header.removeFromRight(8);
    inertia_.setBounds(header.removeFromRight(100));
    header.removeFromRight(8);
    masterBypass_.setBounds(header.removeFromRight(86));
    header.removeFromRight(8);
    advancedButton_.setBounds(header.removeFromRight(192).withSizeKeepingCentre(192, 36));

    bounds.removeFromTop(4);
    response_.setBounds(bounds.removeFromTop(kResponseHeight));
    bounds.removeFromTop(10);
    sweep_.setBounds(bounds.removeFromTop(kSweepHeight));
    bounds.removeFromTop(10);
    auto bands = bounds.removeFromTop(kBandHeight);
    bandA_.setBounds(bands.removeFromLeft((bands.getWidth() - kMargin) / 2));
    bands.removeFromLeft(kMargin);
    bandB_.setBounds(bands);
    bounds.removeFromTop(10);
    effects_.setBounds(bounds);
    if (advancedControls_)
        advancedControls_->setBounds(getLocalBounds().reduced(kMargin).withTrimmedTop(kHeaderHeight + 4));
}
