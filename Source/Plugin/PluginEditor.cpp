#include "PluginEditor.h"
#include "Parameters.h"
#include "Presets.h"
#include "Theme.h"
#include "BinaryData.h"
#include <vector>

namespace {

namespace theme = openedm::theme;

suite::Colours suiteColours() {
    return { theme::kBackground, theme::kPanel, theme::kPanelEdge, theme::kText, theme::kTextDim, theme::kAccentA };
}

} // namespace

OpenEdmFilterEditor::OpenEdmFilterEditor(OpenEdmFilterProcessor& processorToEdit)
    : AudioProcessorEditor(processorToEdit),
      processor_(processorToEdit),
      presetStore_(suite::PresetStore::defaultDirectory("openEDMFilter")),
      masterBypass_(processorToEdit.parameters, openedm::params::kMasterBypass, "BYPASS", theme::kAccentOut),
      inertia_(processorToEdit.parameters, openedm::params::kInertia, "INERTIA", theme::kAccentA),
      response_(processorToEdit),
      sweep_(processorToEdit),
      bandA_(processorToEdit.parameters, 0),
      bandB_(processorToEdit.parameters, 1),
      effects_(processorToEdit) {
    setLookAndFeel(&lookAndFeel_);
    faceplate_ = juce::ImageCache::getFromMemory(BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize);
    pulse_ = juce::Drawable::createFromImageData(BinaryData::filterpulse_svg, BinaryData::filterpulse_svgSize);

    std::vector<juce::String> factoryNames;
    for (const auto& preset : openedm::presets::factoryPresets())
        factoryNames.emplace_back(preset.name);
    presetBrowser_ = std::make_unique<suite::PresetBrowser>(presetStore_, std::move(factoryNames), suiteColours());
    presetBrowser_->captureState = [this] { return processor_.parameterSnapshot(); };
    presetBrowser_->onFactoryPreset = [this](int index) { processor_.setCurrentProgram(index); };
    presetBrowser_->onUserPreset = [this](const juce::ValueTree& snapshot) { processor_.applyParameterSnapshot(snapshot, "User preset"); };
    addChildComponent(*presetBrowser_);

    suiteBar_.onUndo = [this] { processor_.undoEdit(); };
    suiteBar_.onRedo = [this] { processor_.redoEdit(); };
    suiteBar_.onShowA = [this] { if (processor_.abSlot() != 0) processor_.toggleAB(); };
    suiteBar_.onShowB = [this] { if (processor_.abSlot() != 1) processor_.toggleAB(); };
    suiteBar_.onCopyAToB = [this] { processor_.copyBetweenSlots(0, 1); };
    suiteBar_.onCopyBToA = [this] { processor_.copyBetweenSlots(1, 0); };
    suiteBar_.onPresets = [this] {
        presetBrowserVisible_ = !presetBrowserVisible_;
        resized();
    };
    suiteBar_.setActiveSlot(processor_.abSlot());

    openedm::presets::populateComboMenu(presetBox_, openedm::presets::factoryPresets());
    presetBox_.setComponentID("factoryPreset");
    presetBox_.setColour(juce::ComboBox::outlineColourId, theme::kPanelEdge);
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
            advancedControls_->setOpaque(true);
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

    addAndMakeVisible(suiteBar_);
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

    setWantsKeyboardFocus(true);
    addKeyListener(this);
    setResizeLimits(OpenEdmFilterProcessor::kMinEditorWidth, OpenEdmFilterProcessor::kMinEditorHeight,
                    OpenEdmFilterProcessor::kMaxEditorWidth, OpenEdmFilterProcessor::kMaxEditorHeight);
    setResizable(true, true);
    setSize(processor_.editorWidth(), processor_.editorHeight());
    startTimerHz(30);
}

OpenEdmFilterEditor::~OpenEdmFilterEditor() {
    stopTimer();
    removeKeyListener(this);
    setLookAndFeel(nullptr);
}

bool OpenEdmFilterEditor::keyPressed(const juce::KeyPress& key) {
    const auto code = key.getKeyCode();
    const bool isZ = code == 'z' || code == 'Z';
    if (!key.getModifiers().isCommandDown() || !isZ)
        return false;
    if (key.getModifiers().isShiftDown())
        processor_.redoEdit();
    else
        processor_.undoEdit();
    return true;
}

bool OpenEdmFilterEditor::keyPressed(const juce::KeyPress& key, juce::Component* originating) {
    if (dynamic_cast<juce::TextEditor*>(originating) != nullptr)
        return false;
    return keyPressed(key);
}

void OpenEdmFilterEditor::timerCallback() {
    const auto id = processor_.getCurrentProgram() + 1;
    if (presetBox_.getSelectedId() != id)
        presetBox_.setSelectedId(id, juce::dontSendNotification);
    suiteBar_.setHistoryEnabled(processor_.canUndoEdit(), processor_.canRedoEdit());
    suiteBar_.setActiveSlot(processor_.abSlot());
    if (!applyingStateSize_
        && (getWidth() != processor_.editorWidth() || getHeight() != processor_.editorHeight())) {
        applyingStateSize_ = true;
        setSize(processor_.editorWidth(), processor_.editorHeight());
        applyingStateSize_ = false;
    }
    repaint(pulseArea_);
}

void OpenEdmFilterEditor::paint(juce::Graphics& g) {
    g.fillAll(theme::kBackground);
    g.setOpacity(0.12f);
    g.drawImage(faceplate_, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    g.setOpacity(1.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4), 5, 1);

    if (pulse_ && !pulseArea_.isEmpty() && g.clipRegionIntersects(pulseArea_)) {
        const float beat = processor_.animationBeatPhase(juce::Time::getMillisecondCounterHiRes() * 0.001);
        const float motion = std::exp(-5.0f * beat);
        auto frame = pulseArea_.toFloat().reduced(3, 2);
        frame = frame.withSizeKeepingCentre(frame.getWidth(), frame.getHeight() * (0.78f + 0.22f * motion));
        pulse_->drawWithin(g, frame, juce::RectanglePlacement::stretchToFit, 0.72f + 0.28f * motion);
    }
    auto title = titleArea_;
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.setColour(theme::kText);
    g.drawText("open", title, juce::Justification::centredLeft);
    const int openWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "open");
    g.setColour(theme::kAccentA);
    g.drawText("EDM", title.withTrimmedLeft(openWidth), juce::Justification::centredLeft);
    const int edmWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "EDM");
    g.setColour(theme::kAccentB);
    g.drawText("Filter", title.withTrimmedLeft(openWidth + edmWidth), juce::Justification::centredLeft);
}

void OpenEdmFilterEditor::resized() {
    if (!applyingStateSize_)
        processor_.setEditorSize(getWidth(), getHeight());

    const int margin = getWidth() < 1100 ? 8 : 12;
    auto bounds = getLocalBounds().reduced(margin);
    headerHeight_ = 76;
    auto header = bounds.removeFromTop(headerHeight_);
    auto row1 = header.removeFromTop(36);
    header.removeFromTop(4);
    auto row2 = header.removeFromTop(36);

    const int titleText = juce::GlyphArrangement::getStringWidthInt(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)), "openEDMFilter");
    titleArea_ = row1.removeFromLeft(juce::jlimit(160, 210, titleText + 4));
    pulseArea_ = row1.removeFromLeft(56);
    row1.removeFromLeft(4);

    suiteBar_.setBounds(row1.removeFromRight(juce::jmin(suiteBar_.preferredWidth(), row1.getWidth())));
    row1.removeFromRight(4);
    nextPresetBtn_.setBounds(row1.removeFromRight(52));
    row1.removeFromRight(4);
    prevPresetBtn_.setBounds(row1.removeFromLeft(76));
    row1.removeFromLeft(4);
    presetBox_.setBounds(row1);

    auto place = [](juce::Rectangle<int>& row, juce::Component& control, int width) {
        auto cell = row.removeFromRight(width);
        row.removeFromRight(4);
        control.setBounds(cell.withSizeKeepingCentre(cell.getWidth(), 36));
    };
    place(row2, freezeSpectrumButton_, 72);
    place(row2, spectrumButton_, 84);
    place(row2, inertia_, 76);
    place(row2, masterBypass_, 72);
    place(row2, advancedButton_, juce::jmin(168, juce::jmax(120, row2.getWidth())));

    bounds.removeFromTop(4);
    const int gap = 4;
    const int available = bounds.getHeight() - gap * 3;
    const int responseMin = 64;
    const int sweepMin = 190;
    const int bandMin = 180;
    const int effectsMin = 84;
    const int extra = juce::jmax(0, available - (responseMin + sweepMin + bandMin + effectsMin));
    const int responseH = responseMin + extra * 15 / 100;
    const int sweepH = sweepMin + extra * 40 / 100;
    const int bandH = bandMin + extra * 30 / 100;
    response_.setBounds(bounds.removeFromTop(responseH));
    bounds.removeFromTop(gap);
    sweep_.setBounds(bounds.removeFromTop(sweepH));
    bounds.removeFromTop(gap);
    auto bands = bounds.removeFromTop(bandH);
    bandA_.setBounds(bands.removeFromLeft((bands.getWidth() - gap) / 2));
    bands.removeFromLeft(gap);
    bandB_.setBounds(bands);
    bounds.removeFromTop(gap);
    effects_.setBounds(bounds);

    if (presetBrowser_ != nullptr) {
        const int width = juce::jmin(380, getWidth() - margin * 2);
        const int height = juce::jlimit(280, 480, getHeight() - headerHeight_ - margin * 2);
        presetBrowser_->setBounds(getWidth() - width - margin, headerHeight_ + margin, width, height);
        presetBrowser_->setVisible(presetBrowserVisible_);
        if (presetBrowserVisible_)
            presetBrowser_->toFront(false);
    }
    if (advancedControls_) {
        const auto overlay = getLocalBounds().reduced(margin).withTrimmedTop(headerHeight_ + 4);
        constexpr float scale = 1.8f;
        const int scaledW = juce::roundToInt(overlay.getWidth() / scale);
        const int scaledH = (juce::roundToInt(overlay.getHeight() / scale) / 40) * 40;
        advancedControls_->setBounds(0, 0, juce::jmax(40, scaledW), juce::jmax(40, scaledH));
        advancedControls_->setTransform(juce::AffineTransform::scale(scale).translated(float(overlay.getX()), float(overlay.getY())));
        if (advancedControls_->isVisible())
            advancedControls_->toFront(false);
    }
}
