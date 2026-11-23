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
      masterBypass_(processorToEdit.parameters, openedm::params::kMasterBypass, "BYPASS ALL", theme::kBypassEngaged),
      inertia_(processorToEdit.parameters, openedm::params::kInertia, "INERTIA", theme::kAccentA),
      response_(processorToEdit),
      sweep_(processorToEdit),
      bandA_(processorToEdit.parameters, 0),
      bandB_(processorToEdit.parameters, 1),
      effects_(processorToEdit),
      modulationPage_(processorToEdit.parameters) {
    setLookAndFeel(&lookAndFeel_);
    inertia_.setTooltip("Slows cutoff moves on the 48 and 96 dB slopes to stop clicks. Turn off for fast gates.");
    masterBypass_.setTooltip("Bypasses the whole plugin. The host bypass uses this switch.");
    faceplate_ = juce::ImageCache::getFromMemory(BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize);
    pulse_ = juce::Drawable::createFromImageData(BinaryData::filterpulse_svg, BinaryData::filterpulse_svgSize);

    std::vector<juce::String> factoryNames;
    for (const auto& preset : openedm::presets::factoryPresets())
        factoryNames.emplace_back(preset.name);
    presetBrowser_ = std::make_unique<suite::PresetBrowser>(presetStore_, std::move(factoryNames), suiteColours());
    presetBrowser_->captureState = [this] { return processor_.parameterSnapshot(); };
    presetBrowser_->onFactoryPreset = [this](int index) { processor_.setCurrentProgram(index); };
    presetBrowser_->onUserPreset = [this](const juce::String& name, const juce::ValueTree& snapshot) { processor_.loadUserPreset(name, snapshot); };
    presetBrowser_->onSaved = [this](const juce::String& name) { processor_.markPresetSaved(name); };
    presetBrowser_->onRenamed = [this](const juce::String& from, const juce::String& to) { processor_.renamePreset(from, to); };
    addChildComponent(*presetBrowser_);

    suiteBar_.onUndo = [this] { presetBrowser_->disarm(); processor_.undoEdit(); };
    suiteBar_.onRedo = [this] { presetBrowser_->disarm(); processor_.redoEdit(); };
    suiteBar_.onShowA = [this] { presetBrowser_->disarm(); if (processor_.abSlot() != 0) processor_.toggleAB(); };
    suiteBar_.onShowB = [this] { presetBrowser_->disarm(); if (processor_.abSlot() != 1) processor_.toggleAB(); };
    suiteBar_.onCopyAToB = [this] { presetBrowser_->disarm(); processor_.copyBetweenSlots(0, 1); };
    suiteBar_.onCopyBToA = [this] { presetBrowser_->disarm(); processor_.copyBetweenSlots(1, 0); };
    suiteBar_.onPresets = [this] {
        presetBrowser_->disarm();
        presetBrowserVisible_ = !presetBrowserVisible_;
        if (presetBrowserVisible_)
            presetBrowser_->selectPreset(processor_.presetName());
        resized();
    };
    suiteBar_.setActiveSlot(processor_.abSlot());
    suiteBar_.setHistoryEnabled(processor_.canUndoEdit(), processor_.canRedoEdit());

    openedm::presets::populateComboMenu(presetBox_, openedm::presets::factoryPresets());
    presetBox_.setComponentID("factoryPreset");
    presetBox_.setTitle("Preset");
    presetBox_.setColour(juce::ComboBox::outlineColourId, theme::kPanelEdge);
    presetBox_.setText(processor_.presetDisplayName(), juce::dontSendNotification);
    presetBox_.onChange = [this] {
        presetBrowser_->disarm();
        processor_.setCurrentProgram(presetBox_.getSelectedId() - 1);
    };

    prevPresetBtn_.setComponentID("prevPreset");
    prevPresetBtn_.setTitle("Previous preset");
    prevPresetBtn_.setTooltip("Previous preset");
    prevPresetBtn_.setColour(juce::TextButton::buttonOnColourId, theme::kAccentA);
    prevPresetBtn_.onClick = [this] {
        presetBrowser_->disarm();
        presetBrowser_->loadNeighbour(processor_.presetName(), -1);
    };

    nextPresetBtn_.setComponentID("nextPreset");
    nextPresetBtn_.setTitle("Next preset");
    nextPresetBtn_.setTooltip("Next preset");
    nextPresetBtn_.setColour(juce::TextButton::buttonOnColourId, theme::kAccentA);
    nextPresetBtn_.onClick = [this] {
        presetBrowser_->disarm();
        presetBrowser_->loadNeighbour(processor_.presetName(), 1);
    };

    advancedButton_.setComponentID("advancedModulation");
    advancedButton_.setTitle("Modulation page");
    advancedButton_.setColour(juce::TextButton::buttonOnColourId, theme::kAccentFx);
    advancedButton_.setTooltip("Shows routing, FM shape, channels, steps and MIDI. Click again to show the sweep and filters.");
    advancedButton_.onClick = [this] {
        processor_.modulationPageVisible = !processor_.modulationPageVisible.load();
        resized();
    };
    spectrumButton_.setTitle("Spectrum");
    freezeSpectrumButton_.setTitle("Freeze spectrum");
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
    addChildComponent(modulationPage_);

    setWantsKeyboardFocus(true);
    addKeyListener(this);
    // Set the size first. The limits clamp the empty start bounds. That resize replaces the saved size.
    setSize(processor_.editorWidth(), processor_.editorHeight());
    setResizeLimits(OpenEdmFilterProcessor::kMinEditorWidth, OpenEdmFilterProcessor::kMinEditorHeight,
                    OpenEdmFilterProcessor::kMaxEditorWidth, OpenEdmFilterProcessor::kMaxEditorHeight);
    setResizable(true, true);
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
    presetBrowser_->disarm();
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
    if (const auto shown = processor_.presetDisplayName(); presetBox_.getText() != shown) {
        presetBox_.setText(shown, juce::dontSendNotification);
        presetBrowser_->selectPreset(processor_.presetName());
    }
    suiteBar_.setHistoryEnabled(processor_.canUndoEdit(), processor_.canRedoEdit());
    suiteBar_.setActiveSlot(processor_.abSlot());
    const auto sweep = processor_.currentSweepSettings();
    bandA_.refresh(openedm::core::AutoSweep::appliesToBand(sweep, 0));
    bandB_.refresh(openedm::core::AutoSweep::appliesToBand(sweep, 1));
    sweep_.refresh();
    modulationPage_.refresh();
    openedm::ui::setInactive(freezeSpectrumButton_, !spectrumButton_.getToggleState(), "Turn on SPECTRUM to freeze the trace.");
    if (modulationPage_.isVisible() != processor_.modulationPageVisible.load())
        resized();

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

    const int margin = getWidth() <= 1100 ? 8 : 12;
    auto bounds = getLocalBounds().reduced(margin);
    headerHeight_ = 40;
    auto header = bounds.removeFromTop(headerHeight_);

    const int titleText = juce::GlyphArrangement::getStringWidthInt(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)), "openEDMFilter");
    titleArea_ = header.removeFromLeft(juce::jlimit(160, 210, titleText + 4));
    pulseArea_ = header.removeFromLeft(56);
    header.removeFromLeft(4);

    const auto placeRight = [&](juce::Component& control, int width) {
        control.setBounds(header.removeFromRight(width).withSizeKeepingCentre(width, 36));
        header.removeFromRight(4);
    };
    placeRight(suiteBar_, juce::jmin(suiteBar_.preferredWidth(), header.getWidth()));
    placeRight(masterBypass_, 104);
    nextPresetBtn_.setBounds(header.removeFromRight(44));
    header.removeFromRight(4);
    prevPresetBtn_.setBounds(header.removeFromLeft(44));
    header.removeFromLeft(4);
    presetBox_.setBounds(header.withSizeKeepingCentre(header.getWidth(), 36));

    bounds.removeFromTop(4);
    const int gap = 4;
    const int available = bounds.getHeight() - gap * 3;
    // The minimums keep every dial at least 32 px. Of the spare height, the response view takes the first 52 px
    // and the effects strip the next 8 px, so the default size keeps a 96 px effects strip.
    const int responseMin = 56;
    const int sweepMin = 218;
    const int bandMin = 198;
    const int effectsMin = 88;
    const int extra = juce::jmax(0, available - (responseMin + sweepMin + bandMin + effectsMin));
    const int responseFirst = juce::jmin(extra, 52);
    const int effectsFirst = juce::jmin(extra - responseFirst, 8);
    const int rest = extra - responseFirst - effectsFirst;
    const int responseH = responseMin + responseFirst + rest * 40 / 100;
    const int sweepH = sweepMin + rest * 20 / 100;
    const int bandH = bandMin + rest * 25 / 100;

    // The display buttons sit beside the response view, in two rows when they fit.
    auto responseRow = bounds.removeFromTop(responseH);
    const bool hasTwoRows = responseRow.getHeight() >= 76;
    auto cluster = responseRow.removeFromRight(hasTwoRows ? 192 : 356);
    responseRow.removeFromRight(gap);
    response_.setBounds(responseRow);
    auto topRow = cluster.removeFromTop(36);
    auto bottomRow = cluster.withTrimmedTop(gap).withHeight(36);
    auto& displayRow = hasTwoRows ? bottomRow : topRow;
    const auto placeLeft = [](juce::Rectangle<int>& row, juce::Component& control, int width) {
        control.setBounds(row.removeFromLeft(width));
        row.removeFromLeft(4);
    };
    placeLeft(topRow, advancedButton_, 112);
    placeLeft(topRow, inertia_, 76);
    placeLeft(displayRow, spectrumButton_, hasTwoRows ? 100 : 84);
    placeLeft(displayRow, freezeSpectrumButton_, hasTwoRows ? 88 : 72);
    bounds.removeFromTop(gap);
    sweep_.setBounds(bounds.removeFromTop(sweepH));
    bounds.removeFromTop(gap);
    auto bands = bounds.removeFromTop(bandH);
    bandA_.setBounds(bands.removeFromLeft((bands.getWidth() - gap) / 2));
    bands.removeFromLeft(gap);
    bandB_.setBounds(bands);
    bounds.removeFromTop(gap);
    effects_.setBounds(bounds);

    // The modulation page takes the place of the sweep and filter panels. The response view stays visible.
    const bool showsModulation = processor_.modulationPageVisible.load();
    modulationPage_.setBounds(sweep_.getBounds().getUnion(bandB_.getBounds()));
    modulationPage_.setVisible(showsModulation);
    std::ranges::for_each(std::initializer_list<juce::Component*>{ &sweep_, &bandA_, &bandB_ },
                          [&](juce::Component* panel) { panel->setVisible(!showsModulation); });
    advancedButton_.setToggleState(showsModulation, juce::dontSendNotification);

    if (presetBrowser_ != nullptr) {
        const int width = juce::jmin(380, getWidth() - margin * 2);
        const int height = juce::jlimit(280, 480, getHeight() - headerHeight_ - margin * 2);
        presetBrowser_->setBounds(getWidth() - width - margin, headerHeight_ + margin, width, height);
        presetBrowser_->setVisible(presetBrowserVisible_);
        if (presetBrowserVisible_)
            presetBrowser_->toFront(false);
    }
}
