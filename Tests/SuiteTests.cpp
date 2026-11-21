#include "ABCompare.h"
#include "ParameterUndo.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PresetBrowser.h"
#include "PresetStore.h"
#include "Presets.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

namespace {

float cutoff(const OpenEdmFilterProcessor& processor) {
    return processor.parameters.getRawParameterValue("a_cutoff")->load();
}

void setCutoff(OpenEdmFilterProcessor& processor, float hz) {
    auto* parameter = processor.parameters.getParameter("a_cutoff");
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(hz));
    parameter->endChangeGesture();
}

struct TempPresets {
    juce::File directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("oef-presets-" + juce::Uuid().toString());
    TempPresets() { directory.createDirectory(); }
    ~TempPresets() { directory.deleteRecursively(); }
};

juce::Component* findId(juce::Component& root, const juce::String& id) {
    if (root.getComponentID() == id)
        return &root;
    for (auto* child : root.getChildren())
        if (auto* found = findId(*child, id))
            return found;
    return nullptr;
}

void auditControls(juce::Component& component) {
    for (auto* child : component.getChildren()) {
        if (!child->isVisible())
            continue;
        if (auto* dial = dynamic_cast<juce::Slider*>(child); dial != nullptr && dial->isRotary()) {
            INFO(component.getComponentID().toStdString() << " " << dial->getBounds().toString().toStdString());
            CHECK(dial->getHeight() - dial->getTextBoxHeight() >= 32);
            CHECK(dial->getWidth() >= 32);
        }
        if (dynamic_cast<juce::TextButton*>(child) != nullptr || dynamic_cast<juce::ComboBox*>(child) != nullptr) {
            INFO(child->getName().toStdString() << " " << child->getBounds().toString().toStdString()
                 << " parent " << component.getLocalBounds().toString().toStdString());
            CHECK(child->getHeight() >= 36);
            CHECK(child->getWidth() >= 44);
            CHECK(component.getLocalBounds().contains(child->getBounds()));
        } else {
            auditControls(*child);
        }
    }
}

} // namespace

TEST_CASE("Gesture edits undo and redo as one step, and automation does not", "[suite]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    auto* parameter = processor.parameters.getParameter("a_cutoff");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(500.0f));
    processor.flushEdits();
    CHECK_FALSE(processor.canUndoEdit());
    REQUIRE_THAT(cutoff(processor), WithinAbs(500.0f, 0.1f));

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(800.0f));
    parameter->setValueNotifyingHost(parameter->convertTo0to1(900.0f));
    parameter->endChangeGesture();
    processor.flushEdits();
    REQUIRE(processor.canUndoEdit());
    processor.undoEdit();
    REQUIRE_THAT(cutoff(processor), WithinAbs(500.0f, 0.1f));
    processor.redoEdit();
    REQUIRE_THAT(cutoff(processor), WithinAbs(900.0f, 0.1f));

    setCutoff(processor, 1200.0f);
    setCutoff(processor, 1600.0f);
    processor.flushEdits();
    processor.undoEdit();
    REQUIRE_THAT(cutoff(processor), WithinAbs(900.0f, 0.1f));
    REQUIRE(processor.canRedoEdit());
}

TEST_CASE("Loading a factory preset is one undo step", "[suite]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    setCutoff(processor, 432.0f);
    processor.flushEdits();
    const int program = processor.getCurrentProgram();
    processor.setCurrentProgram(4);
    REQUIRE(processor.getCurrentProgram() == 4);
    REQUIRE(processor.canUndoEdit());
    processor.undoEdit();
    REQUIRE(processor.getCurrentProgram() == program);
    REQUIRE_THAT(cutoff(processor), WithinAbs(432.0f, 0.1f));
}

TEST_CASE("A and B keep independent settings and survive a session", "[suite]") {
    OpenEdmFilterProcessor processor;
    setCutoff(processor, 300.0f);
    processor.flushEdits();
    processor.copyBetweenSlots(0, 1);
    setCutoff(processor, 1800.0f);
    processor.flushEdits();
    REQUIRE(processor.abSlot() == 0);
    processor.toggleAB();
    REQUIRE(processor.abSlot() == 1);
    REQUIRE_THAT(cutoff(processor), WithinAbs(300.0f, 0.1f));
    processor.toggleAB();
    REQUIRE_THAT(cutoff(processor), WithinAbs(1800.0f, 0.1f));
    processor.undoEdit();
    REQUIRE(processor.abSlot() == 0);
    REQUIRE_THAT(cutoff(processor), WithinAbs(300.0f, 0.1f));
    processor.redoEdit();
    REQUIRE_THAT(cutoff(processor), WithinAbs(1800.0f, 0.1f));

    processor.toggleAB();
    setCutoff(processor, 700.0f);
    processor.flushEdits();
    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmFilterProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(restored.abSlot() == 1);
    REQUIRE_THAT(cutoff(restored), WithinAbs(700.0f, 0.1f));
    restored.toggleAB();
    REQUIRE_THAT(cutoff(restored), WithinAbs(1800.0f, 0.1f));
    restored.toggleAB();
    REQUIRE_THAT(cutoff(restored), WithinAbs(700.0f, 0.1f));
}

TEST_CASE("Legacy sessions keep their parameters and gain an editor size", "[suite]") {
    OpenEdmFilterProcessor processor;
    processor.setEditorSize(1400, 900);
    juce::XmlElement legacy("openEDMFilter");
    auto* cutoffXml = legacy.createNewChildElement("PARAM");
    cutoffXml->setAttribute("id", "a_cutoff");
    cutoffXml->setAttribute("value", 640.0);
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary(legacy, saved);
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE_THAT(cutoff(processor), WithinAbs(640.0f, 0.1f));
    REQUIRE(processor.parameters.getRawParameterValue("a_env_input")->load() == 0.0f);
    REQUIRE(processor.editorWidth() == 1400);
    REQUIRE(processor.editorHeight() == 900);
    OpenEdmFilterProcessor fresh;
    fresh.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(fresh.editorWidth() == OpenEdmFilterProcessor::kDefaultEditorWidth);
    REQUIRE(fresh.editorHeight() == OpenEdmFilterProcessor::kDefaultEditorHeight);
    REQUIRE(processor.abSlot() == 0);
    processor.toggleAB();
    REQUIRE_THAT(cutoff(processor), WithinAbs(640.0f, 0.1f));
}

TEST_CASE("Envelope input parameter keeps the old id and defaults to the main input", "[suite]") {
    OpenEdmFilterProcessor processor;
    auto* input = processor.parameters.getParameter("a_env_input");
    auto* source = processor.parameters.getParameter("a_env_source");
    REQUIRE(input != nullptr);
    REQUIRE(source != nullptr);
    REQUIRE(input->getVersionHint() == 7);
    REQUIRE(dynamic_cast<juce::AudioParameterChoice*>(input)->choices[0] == "Main");
    REQUIRE(input->getValue() == input->getDefaultValue());
    REQUIRE(processor.parameters.getParameter("b_env_input") != nullptr);
}

TEST_CASE("Sidechain bus is optional and drives the selected envelope", "[suite]") {
    OpenEdmFilterProcessor processor;
    auto layout = processor.getBusesLayout();
    REQUIRE(processor.isBusesLayoutSupported(layout));
    layout.inputBuses.clear();
    layout.outputBuses.clear();
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.inputBuses.add(juce::AudioChannelSet::disabled());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(processor.isBusesLayoutSupported(layout));
    layout.inputBuses.getReference(1) = juce::AudioChannelSet::stereo();
    REQUIRE(processor.isBusesLayoutSupported(layout));
    layout.inputBuses.getReference(1) = juce::AudioChannelSet::create5point1();
    REQUIRE_FALSE(processor.isBusesLayoutSupported(layout));
    layout.inputBuses.getReference(0) = juce::AudioChannelSet::mono();
    layout.inputBuses.getReference(1) = juce::AudioChannelSet::mono();
    layout.outputBuses.getReference(0) = juce::AudioChannelSet::mono();
    REQUIRE(processor.isBusesLayoutSupported(layout));

    juce::AudioProcessor::BusesLayout sidechain;
    sidechain.inputBuses.add(juce::AudioChannelSet::stereo());
    sidechain.inputBuses.add(juce::AudioChannelSet::stereo());
    sidechain.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(processor.setBusesLayout(sidechain));
    processor.prepareToPlay(48000.0, 512);
    auto* env = processor.parameters.getParameter("a_env_input");
    env->setValueNotifyingHost(env->convertTo0to1(1.0f));
    auto* depth = processor.parameters.getParameter("a_env_depth");
    depth->setValueNotifyingHost(depth->convertTo0to1(0.2f));
    auto* frequency = processor.parameters.getParameter("a_cutoff");
    frequency->setValueNotifyingHost(frequency->convertTo0to1(1000.0f));
    processor.parameters.getParameter("b_bypass")->setValueNotifyingHost(1.0f);
    processor.parameters.getParameter("limiter")->setValueNotifyingHost(0.0f);

    constexpr int length = 12000;
    juce::AudioBuffer<float> buffer(4, length);
    buffer.clear();
    for (int sample = 0; sample < length; ++sample) {
        const float value = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * sample / 48000.0f);
        buffer.setSample(2, sample, value);
        buffer.setSample(3, sample, value);
    }
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
    REQUIRE(processor.isSidechainConnected());
    REQUIRE(processor.displayCutoffs[0].load() > 1100.0f);

    juce::AudioProcessor::BusesLayout mainOnly;
    mainOnly.inputBuses.add(juce::AudioChannelSet::stereo());
    mainOnly.inputBuses.add(juce::AudioChannelSet::disabled());
    mainOnly.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(processor.setBusesLayout(mainOnly));
    processor.prepareToPlay(48000.0, 512);
    REQUIRE_FALSE(processor.isSidechainConnected());
    juce::AudioBuffer<float> fallback(2, length);
    for (int sample = 0; sample < length; ++sample) {
        const float value = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * sample / 48000.0f);
        fallback.setSample(0, sample, value);
        fallback.setSample(1, sample, value);
    }
    processor.processBlock(fallback, midi);
    REQUIRE(processor.displayCutoffs[0].load() > 1100.0f);
}

TEST_CASE("User presets save, rename, favorite, and delete without touching factory names", "[suite]") {
    TempPresets temp;
    suite::PresetStore store(temp.directory);
    const auto home = suite::PresetStore::defaultDirectory("openEDMFilter");
    CHECK(home.getFileName() == "Presets");
    CHECK(home.getParentDirectory().getFileName() == "openEDMFilter");
    CHECK(home.getParentDirectory().getParentDirectory().getFileName() == "Alan Marcero");

    juce::ValueTree snapshot("Snapshot");
    juce::ValueTree param("PARAM");
    param.setProperty("id", "a_cutoff", nullptr);
    param.setProperty("value", 2222.0, nullptr);
    snapshot.appendChild(param, nullptr);
    REQUIRE(store.save("Night Drive", snapshot));
    REQUIRE_FALSE(store.save("   ", snapshot));
    auto loaded = store.load("Night Drive");
    REQUIRE(loaded.has_value());
    REQUIRE_THAT(static_cast<float>(static_cast<double>(loaded->getChild(0).getProperty("value"))), WithinAbs(2222.0f, 0.01f));
    REQUIRE(store.setFavorite(false, "Night Drive", true));
    REQUIRE(store.isFavorite(false, "Night Drive"));
    REQUIRE(store.setFavorite(true, "Uplifting / Factory", true));
    REQUIRE(store.isFavorite(true, "Uplifting / Factory"));
    REQUIRE(store.rename("Night Drive", "Late Drive"));
    REQUIRE_FALSE(store.load("Night Drive").has_value());
    REQUIRE(store.isFavorite(false, "Late Drive"));
    REQUIRE(store.remove("Late Drive"));
    REQUIRE(store.entries().empty());
    REQUIRE(store.isFavorite(true, "Uplifting / Factory"));
    REQUIRE_FALSE(store.remove("Uplifting / Factory"));
}

TEST_CASE("Preset browser searches and keeps factory rows read-only", "[suite]") {
    juce::ScopedJuceInitialiser_GUI gui;
    TempPresets temp;
    suite::PresetStore store(temp.directory);
    juce::ValueTree snapshot("Snapshot");
    REQUIRE(store.save("My Sweep", snapshot));
    suite::PresetBrowser browser(store, { "Uplifting / Wide", "Rolling / Tight" }, {});
    browser.setBounds(0, 0, 380, 420);
    CHECK(browser.listedNames().size() == 3);
    browser.setSearchText("tight");
    CHECK(browser.listedNames().size() == 1);
    CHECK(browser.listedNames()[0] == "Rolling / Tight");
    auto* list = dynamic_cast<juce::ListBox*>(browser.findChildWithID("presetList"));
    auto* remove = dynamic_cast<juce::TextButton*>(browser.findChildWithID("presetDelete"));
    auto* rename = dynamic_cast<juce::TextButton*>(browser.findChildWithID("presetRename"));
    REQUIRE(list != nullptr);
    REQUIRE(remove != nullptr);
    list->selectRow(0);
    CHECK_FALSE(remove->isEnabled());
    CHECK_FALSE(rename->isEnabled());
    browser.setSearchText("My");
    list->selectRow(0);
    CHECK(remove->isEnabled());
    CHECK(rename->isEnabled());
}

TEST_CASE("Editor scales down, remembers its size, and undo keys work", "[suite][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr)
        SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    REQUIRE(editor->isResizable());
    REQUIRE(editor->getConstrainer()->getFixedAspectRatio() == 0.0);
    CHECK(editor->getWidth() == OpenEdmFilterProcessor::kDefaultEditorWidth);
    CHECK(editor->getHeight() == OpenEdmFilterProcessor::kDefaultEditorHeight);
    CHECK_FALSE(findId(*editor, "undoButton")->isEnabled());
    CHECK(editor->getHeight() < 800);
    CHECK(OpenEdmFilterProcessor::kMinEditorHeight < 800);
    editor->setSize(OpenEdmFilterProcessor::kMinEditorWidth, OpenEdmFilterProcessor::kMinEditorHeight);
    CHECK(editor->getHeight() == OpenEdmFilterProcessor::kMinEditorHeight);
    CHECK(processor.editorHeight() == OpenEdmFilterProcessor::kMinEditorHeight);
    auditControls(*editor);

    setCutoff(processor, 555.0f);
    processor.flushEdits();
    dynamic_cast<juce::Timer&>(*editor).timerCallback();
    CHECK(dynamic_cast<juce::ComboBox*>(findId(*editor, "factoryPreset"))->getText() == "Uplifting / Init *");
    REQUIRE(editor->keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)));
    REQUIRE_THAT(cutoff(processor), WithinAbs(processor.parameters.getParameter("a_cutoff")->convertFrom0to1(
        processor.parameters.getParameter("a_cutoff")->getDefaultValue()), 0.1f));
    REQUIRE(editor->keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE_THAT(cutoff(processor), WithinAbs(555.0f, 0.1f));

    auto* presets = findId(*editor, "presetBrowserButton");
    REQUIRE(presets != nullptr);
    auto* browserButton = dynamic_cast<juce::Button*>(presets);
    REQUIRE(browserButton != nullptr);
    browserButton->triggerClick();
    auto* search = findId(*editor, "presetSearch");
    REQUIRE(search != nullptr);
    REQUIRE(search->isVisible());
    auditControls(*editor);

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmFilterProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    CHECK(restored.editorWidth() == OpenEdmFilterProcessor::kMinEditorWidth);
    CHECK(restored.editorHeight() == OpenEdmFilterProcessor::kMinEditorHeight);
    std::unique_ptr<juce::AudioProcessorEditor> reopened(restored.createEditor());
    CHECK(reopened->getWidth() == OpenEdmFilterProcessor::kMinEditorWidth);
    CHECK(reopened->getHeight() == OpenEdmFilterProcessor::kMinEditorHeight);
}

TEST_CASE("The preset name follows loads, edits, saves, A/B slots and sessions", "[suite]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    const juce::String factoryFour = openedm::presets::factoryPresets()[4].name;
    juce::ValueTree snapshot("Snapshot");
    juce::ValueTree param("PARAM");
    param.setProperty("id", "a_cutoff", nullptr);
    param.setProperty("value", 640.0, nullptr);
    snapshot.appendChild(param, nullptr);

    CHECK(processor.presetDisplayName() == "Uplifting / Init");
    processor.loadUserPreset("Night Drive", snapshot);
    CHECK(processor.presetDisplayName() == "Night Drive");
    setCutoff(processor, 900.0f);
    processor.flushEdits();
    CHECK(processor.presetDisplayName() == "Night Drive *");
    processor.undoEdit();
    CHECK(processor.presetDisplayName() == "Night Drive");
    processor.redoEdit();
    CHECK(processor.presetDisplayName() == "Night Drive *");
    processor.markPresetSaved("Late Drive");
    CHECK(processor.presetDisplayName() == "Late Drive");

    setCutoff(processor, 1000.0f);
    processor.flushEdits();
    processor.copyBetweenSlots(0, 1);
    processor.toggleAB();
    processor.setCurrentProgram(4);
    CHECK(processor.presetDisplayName() == factoryFour);
    processor.toggleAB();
    CHECK(processor.presetDisplayName() == "Late Drive *");

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmFilterProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    CHECK(restored.presetDisplayName() == "Late Drive *");
    restored.toggleAB();
    CHECK(restored.presetDisplayName() == factoryFour);

    juce::XmlElement legacy("openEDMFilter");
    legacy.setAttribute("program", 4);
    legacy.setAttribute("preset_catalog_version", 2);
    juce::MemoryBlock legacyState;
    juce::AudioProcessor::copyXmlToBinary(legacy, legacyState);
    restored.setStateInformation(legacyState.getData(), static_cast<int>(legacyState.getSize()));
    CHECK(restored.presetDisplayName() == factoryFour + " *");

    restored.markPresetSaved("Night Run");
    restored.renamePreset("Other", "Ignored");
    restored.renamePreset("Night Run", "Day Run");
    CHECK(restored.presetDisplayName() == "Day Run");
}

TEST_CASE("Previous and next step through the preset list and wrap at the ends", "[suite]") {
    juce::ScopedJuceInitialiser_GUI gui;
    TempPresets temp;
    suite::PresetStore store(temp.directory);
    REQUIRE(store.save("My Sweep", juce::ValueTree("Snapshot")));
    suite::PresetBrowser browser(store, { "Uplifting / Wide", "Rolling / Tight" }, {});
    std::vector<juce::String> loaded;
    browser.onFactoryPreset = [&](int program) { loaded.push_back("factory " + juce::String(program)); };
    browser.onUserPreset = [&](const juce::String& name, const juce::ValueTree&) { loaded.push_back(name); };

    browser.loadNeighbour("Uplifting / Wide", 1);
    browser.loadNeighbour("Rolling / Tight", 1);
    browser.loadNeighbour("My Sweep", 1);
    browser.loadNeighbour("Uplifting / Wide", -1);
    browser.loadNeighbour("Not Listed", 1);

    CHECK(loaded == std::vector<juce::String>{ "factory 1", "My Sweep", "factory 0", "My Sweep", "factory 0" });
}

TEST_CASE("Each A/B slot keeps its own undo history", "[suite]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    const float defaultCutoff = cutoff(processor);
    setCutoff(processor, 300.0f);
    processor.flushEdits();
    processor.toggleAB();
    CHECK_FALSE(processor.canUndoEdit());
    setCutoff(processor, 5000.0f);
    processor.flushEdits();
    processor.toggleAB();

    processor.undoEdit();
    CHECK(processor.abSlot() == 0);
    CHECK_THAT(cutoff(processor), WithinAbs(defaultCutoff, 0.1f));
    CHECK_FALSE(processor.canUndoEdit());
    processor.redoEdit();
    CHECK_THAT(cutoff(processor), WithinAbs(300.0f, 0.1f));

    processor.copyBetweenSlots(0, 1);
    processor.toggleAB();
    CHECK_THAT(cutoff(processor), WithinAbs(300.0f, 0.1f));
    CHECK_FALSE(processor.canUndoEdit());
}

TEST_CASE("Save asks before replacing and Delete asks before removing a user preset", "[suite]") {
    juce::ScopedJuceInitialiser_GUI gui;
    TempPresets temp;
    suite::PresetStore store(temp.directory);
    const auto snapshotWith = [](double hz) {
        juce::ValueTree snapshot("Snapshot");
        juce::ValueTree param("PARAM");
        param.setProperty("id", "a_cutoff", nullptr);
        param.setProperty("value", hz, nullptr);
        snapshot.appendChild(param, nullptr);
        return snapshot;
    };
    const auto storedHz = [&] { return static_cast<double>(store.load("My Sweep")->getChild(0).getProperty("value")); };
    REQUIRE(store.save("My Sweep", snapshotWith(100.0)));
    suite::PresetBrowser browser(store, {}, {});
    browser.setBounds(0, 0, 380, 420);
    browser.captureState = [&] { return snapshotWith(200.0); };
    auto* save = dynamic_cast<juce::TextButton*>(findId(browser, "presetSave"));
    auto* remove = dynamic_cast<juce::TextButton*>(findId(browser, "presetDelete"));
    auto* name = dynamic_cast<juce::TextEditor*>(findId(browser, "presetName"));
    auto* list = dynamic_cast<juce::ListBox*>(findId(browser, "presetList"));

    name->setText("My Sweep", false);
    save->onClick();
    CHECK(save->getButtonText() == "REPLACE?");
    CHECK_THAT(storedHz(), WithinAbs(100.0, 1e-9));
    save->onClick();
    CHECK(save->getButtonText() == "SAVE");
    CHECK_THAT(storedHz(), WithinAbs(200.0, 1e-9));

    list->selectRow(0);
    remove->onClick();
    CHECK(remove->getButtonText() == "DELETE?");
    CHECK(store.load("My Sweep").has_value());
    remove->onClick();
    CHECK_FALSE(store.load("My Sweep").has_value());
    CHECK(remove->getButtonText() == "DELETE");
}

TEST_CASE("Save and rename report the name the store lists", "[suite]") {
    juce::ScopedJuceInitialiser_GUI gui;
    TempPresets temp;
    suite::PresetStore store(temp.directory);
    suite::PresetBrowser browser(store, {}, {});
    browser.setBounds(0, 0, 380, 420);
    browser.captureState = [] { return juce::ValueTree("Snapshot"); };
    juce::String saved;
    juce::StringArray renamed;
    browser.onSaved = [&](const juce::String& name) { saved = name; };
    browser.onRenamed = [&](const juce::String& from, const juce::String& to) { renamed = { from, to }; };
    auto* name = dynamic_cast<juce::TextEditor*>(findId(browser, "presetName"));

    name->setText("My Sweep (16 Bar)", false);
    dynamic_cast<juce::TextButton*>(findId(browser, "presetSave"))->onClick();
    CHECK(saved == "My Sweep _16 Bar_");
    CHECK(browser.listedNames().contains(saved));
    name->setText("Late Sweep", false);
    dynamic_cast<juce::TextButton*>(findId(browser, "presetRename"))->onClick();
    CHECK(renamed == juce::StringArray{ "My Sweep _16 Bar_", "Late Sweep" });
}
