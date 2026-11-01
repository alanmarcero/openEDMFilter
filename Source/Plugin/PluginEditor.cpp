#include "PluginEditor.h"
#include "Parameters.h"
#include "Presets.h"
#include "Theme.h"
#include "BinaryData.h"

namespace {

namespace theme = openedm::theme;

constexpr int kEditorWidth = 980;
constexpr int kEditorHeight = 840;
constexpr int kMargin = 12;
constexpr int kHeaderHeight = 44;
constexpr int kResponseHeight = 164;
constexpr int kSweepHeight = 166;
constexpr int kBandHeight = 256;

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
      effects_(processorToEdit.parameters) {
    setLookAndFeel(&lookAndFeel_);
    faceplate_ = juce::ImageCache::getFromMemory(BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize);

    const auto& presets = openedm::presets::factoryPresets();
    std::ranges::for_each(presets, [&, id = 1](const auto& preset) mutable { presetBox_.addItem(preset.name, id++); });
    presetBox_.setColour(juce::ComboBox::outlineColourId, theme::kText);
    presetBox_.setSelectedId(processor_.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox_.onChange = [this] { processor_.setCurrentProgram(presetBox_.getSelectedId() - 1); };

    addAndMakeVisible(presetBox_);
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
    g.setOpacity(0.75f);
    g.drawImage(faceplate_, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    g.setOpacity(1.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4), 5, 1);

    const auto header = getLocalBounds().reduced(kMargin, 0).removeFromTop(kHeaderHeight + kMargin).withTrimmedTop(kMargin).withHeight(28);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.setColour(theme::kText);
    g.drawText("open", header, juce::Justification::centredLeft);
    const int openWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "open");
    g.setColour(theme::kAccentA);
    g.drawText("EDM", header.withTrimmedLeft(openWidth), juce::Justification::centredLeft);
    const int edmWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "EDM");
    g.setColour(theme::kAccentB);
    g.drawText("Filter", header.withTrimmedLeft(openWidth + edmWidth), juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(11.0f));
    g.setColour(theme::kTextDim);
    g.drawText("Dual multimode filter  /  12-96 dB  /  4-64 Bar Auto-Sweep  /  Tempo & Time Sig Sync",
               header.translated(2, 28).withHeight(14).withWidth(620),
               juce::Justification::centredLeft);
}

void OpenEdmFilterEditor::resized() {
    auto bounds = getLocalBounds().reduced(kMargin);
    auto header = bounds.removeFromTop(kHeaderHeight);
    presetBox_.setBounds(header.removeFromRight(240).withSizeKeepingCentre(240, 28));
    header.removeFromRight(8);
    inertia_.setBounds(header.removeFromRight(84));
    header.removeFromRight(8);
    masterBypass_.setBounds(header.removeFromRight(76));

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
}
