#include "PluginEditor.h"
#include "Parameters.h"
#include "Presets.h"
#include "Theme.h"

namespace {

namespace theme = openedm::theme;

constexpr int kEditorWidth = 980;
constexpr int kEditorHeight = 684;
constexpr int kMargin = 12;
constexpr int kHeaderHeight = 44;
constexpr int kResponseHeight = 176;
constexpr int kBandHeight = 268;

} // namespace

OpenEdmFilterEditor::OpenEdmFilterEditor(OpenEdmFilterProcessor& processorToEdit)
    : AudioProcessorEditor(processorToEdit),
      processor_(processorToEdit),
      inertia_(processorToEdit.parameters, openedm::params::kInertia, "INERTIA", theme::kText),
      response_(processorToEdit),
      bandA_(processorToEdit.parameters, 0),
      bandB_(processorToEdit.parameters, 1),
      effects_(processorToEdit.parameters) {
    setLookAndFeel(&lookAndFeel_);

    const auto& presets = openedm::presets::factoryPresets();
    std::ranges::for_each(presets, [&, id = 1](const auto& preset) mutable { presetBox_.addItem(preset.name, id++); });
    presetBox_.setColour(juce::ComboBox::outlineColourId, theme::kText);
    presetBox_.setSelectedId(processor_.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox_.onChange = [this] { processor_.setCurrentProgram(presetBox_.getSelectedId() - 1); };

    addAndMakeVisible(presetBox_);
    addAndMakeVisible(inertia_);
    addAndMakeVisible(response_);
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

    g.setFont(juce::FontOptions(11.0f));
    g.setColour(theme::kTextDim);
    g.drawText("DUAL MULTIMODE FILTER  /  12-96 dB  /  LFO  /  ENVELOPE  /  FM", header.withTrimmedLeft(200),
               juce::Justification::centredLeft);
}

void OpenEdmFilterEditor::resized() {
    auto bounds = getLocalBounds().reduced(kMargin);
    auto header = bounds.removeFromTop(kHeaderHeight);
    presetBox_.setBounds(header.removeFromRight(220).withSizeKeepingCentre(220, 28));
    header.removeFromRight(8);
    inertia_.setBounds(header.removeFromRight(84));

    bounds.removeFromTop(4);
    response_.setBounds(bounds.removeFromTop(kResponseHeight));
    bounds.removeFromTop(kMargin);
    auto bands = bounds.removeFromTop(kBandHeight);
    bandA_.setBounds(bands.removeFromLeft((bands.getWidth() - kMargin) / 2));
    bands.removeFromLeft(kMargin);
    bandB_.setBounds(bands);
    bounds.removeFromTop(kMargin);
    effects_.setBounds(bounds);
}
