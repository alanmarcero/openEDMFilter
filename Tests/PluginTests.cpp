#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "Presets.h"
#include "Theme.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <numbers>
#include <cmath>
#include <map>
#include <ranges>
#include <string>

using Catch::Matchers::WithinAbs;

namespace {
struct GestureCounts : juce::AudioProcessorParameter::Listener {
    explicit GestureCounts(juce::AudioProcessorParameter& value) : parameter(value) { parameter.addListener(this); }
    ~GestureCounts() override { parameter.removeListener(this); }
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override { if (starting) ++begins; else ++ends; }
    juce::AudioProcessorParameter& parameter;
    int begins = 0;
    int ends = 0;
};

void set(OpenEdmFilterProcessor& processor, const juce::String& id, float value) {
    auto* parameter = processor.parameters.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
}

TEST_CASE("Host automation selects independent sweep slots and recalls all four", "[plugin]") {
    OpenEdmFilterProcessor processor;
    auto* selector = processor.parameters.getParameter("sweep_slot");
    REQUIRE(selector != nullptr);
    REQUIRE(selector->isAutomatable());
    REQUIRE(selector->getNumSteps() == 4);
    set(processor, "sweep_on", 1.0f);
    const std::array<juce::String, 4> prefixes{ "sweep_", "sweep_b_", "sweep_c_", "sweep_d_" };
    for (int slot = 0; slot < 4; ++slot) {
        set(processor, prefixes[static_cast<size_t>(slot)] + "cut_start", 200.0f * static_cast<float>(slot + 1));
        set(processor, prefixes[static_cast<size_t>(slot)] + "cut_slope", 0.2f * static_cast<float>(slot));
        set(processor, prefixes[static_cast<size_t>(slot)] + "res_slope", -0.2f * static_cast<float>(slot));
    }
    for (int slot : { 3, 0, 2, 1 }) {
        set(processor, "sweep_slot", static_cast<float>(slot));
        const auto sweep = processor.currentSweepSettings();
        REQUIRE(sweep.enabled);
        REQUIRE_THAT(sweep.cutoffStartHz, WithinAbs(200.0f * static_cast<float>(slot + 1), 0.01f));
        REQUIRE_THAT(sweep.cutoffSlope, WithinAbs(0.2f * static_cast<float>(slot), 0.0001f));
        REQUIRE_THAT(sweep.resonanceSlope, WithinAbs(-0.2f * static_cast<float>(slot), 0.0001f));
    }
    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmFilterProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE_THAT(restored.currentSweepSettings().cutoffStartHz, WithinAbs(400.0f, 0.01f));
    for (int slot = 0; slot < 4; ++slot) {
        set(restored, "sweep_slot", static_cast<float>(slot));
        REQUIRE_THAT(restored.currentSweepSettings().cutoffStartHz, WithinAbs(200.0f * static_cast<float>(slot + 1), 0.01f));
    }
}

TEST_CASE("Loading a legacy state resets new slots and keeps the original sweep in A", "[plugin]") {
    OpenEdmFilterProcessor processor;
    set(processor, "sweep_slot", 3.0f);
    set(processor, "sweep_d_cut_start", 999.0f);
    juce::XmlElement legacy("openEDMFilter");
    auto* cutoff = legacy.createNewChildElement("PARAM");
    cutoff->setAttribute("id", "sweep_cut_start");
    cutoff->setAttribute("value", 345.0);
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary(legacy, saved);
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE_THAT(processor.currentSweepSettings().cutoffStartHz, WithinAbs(345.0f, 0.01f));
    set(processor, "sweep_slot", 3.0f);
    REQUIRE_THAT(processor.currentSweepSettings().cutoffStartHz, WithinAbs(80.0f, 0.01f));
}

TEST_CASE("Quick sweep presets edit only the selected slot", "[plugin]") {
    OpenEdmFilterProcessor processor;
    set(processor, "sweep_cut_start", 345.0f);
    set(processor, "sweep_slot", 2.0f);
    openedm::presets::applyPartial(openedm::presets::sweepSlopePresets().front(), processor.parameters);
    REQUIRE_THAT(processor.currentSweepSettings().cutoffStartHz, WithinAbs(80.0f, 0.01f));
    set(processor, "sweep_slot", 0.0f);
    REQUIRE_THAT(processor.currentSweepSettings().cutoffStartHz, WithinAbs(345.0f, 0.01f));
}

TEST_CASE("Input trim is saved and the gain meters follow the signal", "[plugin]") {
    OpenEdmFilterProcessor processor;
    processor.prepareToPlay(48000.0, 2048);
    set(processor, "a_bypass", 1.0f);
    set(processor, "b_bypass", 1.0f);
    set(processor, "limiter", 0.0f);
    set(processor, "input_gain", 6.0f);

    juce::AudioBuffer<float> buffer(1, 2048);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        buffer.setSample(0, sample, static_cast<float>(0.2 * std::sin(2.0 * std::numbers::pi_v<double>
                                                                      * 1000.0 * sample / 48000.0)));
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    REQUIRE_THAT(processor.displayInputPeak.load(), WithinAbs(0.4f, 0.01f));
    REQUIRE_THAT(processor.displayOutputPeak.load(), WithinAbs(0.4f, 0.02f));

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmFilterProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE_THAT(restored.parameters.getRawParameterValue("input_gain")->load(), WithinAbs(6.0f, 0.01f));
}

TEST_CASE("Spectrum analyzer finds an input tone and freeze holds its trace", "[plugin]") {
    OpenEdmFilterProcessor processor;
    processor.prepareToPlay(48000.0, 2048);
    set(processor, "a_bypass", 1.0f);
    set(processor, "b_bypass", 1.0f);
    juce::MidiBuffer midi;
    const auto renderTone = [&](double frequency) {
        juce::AudioBuffer<float> buffer(1, 2048);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(0, sample, static_cast<float>(0.25 * std::sin(2.0 * std::numbers::pi_v<double>
                                                                           * frequency * sample / 48000.0)));
        processor.processBlock(buffer, midi);
    };

    renderTone(1000.0);
    const auto strongest = std::ranges::max_element(processor.displaySpectrum,
        [](const auto& a, const auto& b) { return a.load() < b.load(); });
    const int peakBin = static_cast<int>(std::distance(processor.displaySpectrum.begin(), strongest));
    const int expectedBin = juce::roundToInt(std::log(1000.0 / 20.0) / std::log(20000.0 / 20.0)
                                             * static_cast<double>(processor.displaySpectrum.size() - 1));
    REQUIRE(strongest->load() > 0.15f);
    REQUIRE(std::abs(peakBin - expectedBin) <= 2);

    std::array<float, 128> frozen{};
    std::ranges::transform(processor.displaySpectrum, frozen.begin(), [](const auto& bin) { return bin.load(); });
    processor.spectrumFrozen.store(true);
    renderTone(3000.0);
    for (size_t bin = 0; bin < frozen.size(); ++bin)
        REQUIRE(std::abs(processor.displaySpectrum[bin].load() - frozen[bin]) < 1e-7f);
}

TEST_CASE("Spectrum analyzer clears display bins above host Nyquist", "[plugin]") {
    OpenEdmFilterProcessor processor;
    processor.prepareToPlay(8000.0, 2048);
    juce::AudioBuffer<float> buffer(1, 2048);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        buffer.setSample(0, sample, static_cast<float>(0.25 * std::cos(2.0 * std::numbers::pi_v<double>
                                                                      * 4000.0 * sample / 8000.0)));
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    const auto strongest = std::ranges::max_element(processor.displaySpectrum,
        [](const auto& a, const auto& b) { return a.load() < b.load(); });
    REQUIRE(strongest->load() > 0.1f);
    REQUIRE(processor.displaySpectrum.back().load() < 1e-6f);
}

TEST_CASE("Sweep effects change audio only after onset and respect sweep bypass", "[plugin]") {
    for (const juce::String effect : { "crush", "glitch" }) {
        OpenEdmFilterProcessor processor;
        set(processor, "a_bypass", 1.0f);
        set(processor, "b_bypass", 1.0f);
        set(processor, "limiter", 0.0f);
        set(processor, "crush_rate", 1000.0f);
        set(processor, "crush_bits", 4.0f);
        set(processor, "clock_sync", 0.0f);
        set(processor, "tempo_bpm", 240.0f);
        set(processor, "sweep_on", 1.0f);
        set(processor, "sweep_bars", static_cast<float>(openedm::params::sweepBarIndex(4)));
        set(processor, "sweep_" + effect + "_end", 1.0f);
        set(processor, "sweep_" + effect + "_onset", 0.75f);
        set(processor, "sweep_" + effect + "_slope", 0.0f);
        processor.prepareToPlay(8000.0, 800);
        juce::AudioBuffer<float> buffer(1, 800);
        juce::MidiBuffer midi;
        double earlyError = 0.0, lateError = 0.0, bypassError = 0.0;
        constexpr double pi = 3.141592653589793;
        const auto input = [&](int sample) { return 0.3f * static_cast<float>(std::sin(2.0 * pi * 211.0 * sample / 8000.0)); };
        for (int block = 0; block < 45; ++block) {
            if (block == 40) set(processor, "sweep_bypass", 1.0f);
            for (int i = 0; i < 800; ++i) buffer.setSample(0, i, input(block * 800 + i));
            processor.processBlock(buffer, midi);
            for (int i = 0; i < 800; ++i) {
                const auto error = std::abs(buffer.getSample(0, i) - input(block * 800 + i - openedm::core::FilterEngine::kLatencySamples));
                if (block == 10) earlyError += error;
                if (block == 38) lateError += error;
                if (block == 44) bypassError += error;
            }
        }
        INFO(effect);
        REQUIRE(earlyError / 800.0 < 0.001);
        REQUIRE(lateError / 800.0 > 0.01);
        REQUIRE(bypassError / 800.0 < 0.001);
    }
}

TEST_CASE("Textured factory presets keep effects late and populate every auto slot", "[plugin]") {
    OpenEdmFilterProcessor processor;
    const auto& presets = openedm::presets::factoryPresets();
    int textured = 0;
    for (size_t index = 0; index < presets.size(); ++index) {
        processor.setCurrentProgram(static_cast<int>(index));
        if (processor.currentSweepSettings().crush.end == 0.0f) continue;
        ++textured;
        float previousCrush = 0.0f;
        for (int slot = 0; slot < 4; ++slot) {
            set(processor, "sweep_slot", static_cast<float>(slot));
            const auto sweep = processor.currentSweepSettings();
            REQUIRE(sweep.enabled);
            REQUIRE(sweep.crush.end > previousCrush);
            previousCrush = sweep.crush.end;
            for (const auto envelope : { sweep.crush, sweep.glitch }) {
                REQUIRE(envelope.onset >= 0.65f);
                REQUIRE_THAT(openedm::core::AutoSweep::envelopeAt(envelope, 0.5f), WithinAbs(0.0f, 0.0001f));
                REQUIRE(openedm::core::AutoSweep::envelopeAt(envelope, 0.98f) > 0.05f);
            }
        }
    }
    REQUIRE(textured >= 4);
}

TEST_CASE("Slot automation reaches the audio engine without resetting the sweep clock", "[plugin]") {
    OpenEdmFilterProcessor processor;
    processor.prepareToPlay(48000.0, 4800);
    set(processor, "sweep_on", 1.0f);
    set(processor, "clock_sync", 0.0f);
    set(processor, "sweep_cut_start", 200.0f);
    set(processor, "sweep_cut_end", 200.0f);
    set(processor, "sweep_d_cut_start", 4000.0f);
    set(processor, "sweep_d_cut_end", 4000.0f);
    juce::AudioBuffer<float> buffer(2, 4800);
    buffer.clear();
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
    REQUIRE_THAT(processor.displayCutoffs[0].load(), WithinAbs(200.0f, 0.1f));
    const float before = processor.displaySweepPhase.load();
    set(processor, "sweep_slot", 3.0f);
    processor.processBlock(buffer, midi);
    REQUIRE_THAT(processor.displayCutoffs[0].load(), WithinAbs(4000.0f, 1.0f));
    REQUIRE_THAT(processor.displaySweepPhase.load(), WithinAbs(before * 2.0f, 0.0001f));
}

TEST_CASE("Editor slot buttons and host automation rebind the visible sweep knobs", "[plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    processor.setCurrentProgram(31);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    juce::TextButton* spectrumButton = nullptr;
    juce::TextButton* freezeButton = nullptr;
    openedm::ui::SweepPanel* panel = nullptr;
    for (auto* child : editor->getChildren()) {
        if (auto* sweep = dynamic_cast<openedm::ui::SweepPanel*>(child)) panel = sweep;
        if (auto* button = dynamic_cast<juce::TextButton*>(child)) {
            if (button->getButtonText() == "SPECTRUM") spectrumButton = button;
            if (button->getButtonText() == "FREEZE") freezeButton = button;
        }
    }
    const auto audit = [&](auto&& self, juce::Component& parent) -> void {
        std::vector<juce::Component*> visible;
        // JUCE draws the resize corner over the editor's bottom-right corner on purpose.
        std::ranges::copy_if(parent.getChildren(), std::back_inserter(visible), [](auto* child) {
            return child->isVisible() && dynamic_cast<juce::ResizableCornerComponent*>(child) == nullptr;
        });
        std::ranges::for_each(std::views::iota(size_t{ 0 }, visible.size()), [&](size_t first) {
            std::ranges::for_each(std::views::iota(first + 1, visible.size()), [&](size_t second) {
                CAPTURE(parent.getComponentID().toStdString(), visible[first]->getBounds().toString().toStdString(),
                        visible[second]->getBounds().toString().toStdString());
                CHECK_FALSE(visible[first]->getBounds().intersects(visible[second]->getBounds()));
            });
        });
        for (auto* child : parent.getChildren()) {
            if (!child->isVisible()) continue;
            if (auto* dial = dynamic_cast<juce::Slider*>(child); dial != nullptr && dial->isRotary()) {
                CAPTURE(parent.getComponentID().toStdString(), dial->getBounds().toString().toStdString());
                CHECK(dial->getHeight() - dial->getTextBoxHeight() >= 32);
                CHECK(dial->getWidth() >= 32);
            }
            if (dynamic_cast<juce::TextButton*>(child) || dynamic_cast<juce::ComboBox*>(child)) {
                CAPTURE(child->getName().toStdString(), child->getBounds().toString().toStdString());
                CHECK(child->getHeight() >= 36);
                CHECK(child->getWidth() >= 44);
                CHECK(parent.getLocalBounds().contains(child->getBounds()));
            } else self(self, *child);
        }
    };
    audit(audit, *editor);
    REQUIRE(panel != nullptr);
    REQUIRE(spectrumButton != nullptr);
    REQUIRE(freezeButton != nullptr);
    auto* factoryMenu = dynamic_cast<juce::ComboBox*>(editor->findChildWithID("factoryPreset"));
    auto* sweepMenu = dynamic_cast<juce::ComboBox*>(panel->findChildWithID("sweepPreset"));
    REQUIRE(factoryMenu != nullptr);
    REQUIRE(sweepMenu != nullptr);
    REQUIRE(factoryMenu->getRootMenu()->getNumItems() == 7);
    REQUIRE(sweepMenu->getRootMenu()->getNumItems() == 7);
    int bands = 0;
    for (auto* child : editor->getChildren()) {
        auto* band = dynamic_cast<openedm::ui::BandPanel*>(child);
        if (!band) continue;
        const auto prefix = bands++ == 0 ? juce::String("a_") : juce::String("b_");
        auto* menu = dynamic_cast<juce::ComboBox*>(band->findChildWithID(prefix + "type"));
        REQUIRE(menu != nullptr);
        REQUIRE(menu->getNumItems() == 8);
        for (int id = 1; id <= 8; ++id) {
            menu->setSelectedId(id, juce::sendNotificationSync);
            const auto legacy = processor.parameters.getRawParameterValue(prefix + "type")->load();
            const auto response = processor.parameters.getRawParameterValue(prefix + "response")->load();
            CHECK(openedm::params::effectiveType(int(legacy), int(response)) == id - 1);
        }
        menu->setSelectedId(1, juce::sendNotificationSync);
        processor.parameters.getParameter(prefix + "type")->setValueNotifyingHost(2.0f / 3);
        CHECK(menu->getSelectedId() == 3);
    }
    REQUIRE(bands == 2);
    std::vector<juce::Slider*> sliders;
    std::array<juce::TextButton*, 4> buttons{};
    for (auto* child : panel->getChildren()) {
        if (auto* button = dynamic_cast<juce::TextButton*>(child)) {
            for (int slot = 0; slot < 4; ++slot) {
                if (button->getButtonText() == "SLOT " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + slot)))
                    buttons[static_cast<size_t>(slot)] = button;
            }
        }
        if (dynamic_cast<openedm::ui::Knob*>(child) != nullptr) {
            for (auto* control : child->getChildren())
                if (auto* slider = dynamic_cast<juce::Slider*>(control)) sliders.push_back(slider);
        }
    }
    for (auto* button : buttons) REQUIRE(button != nullptr);
    for (int slot = 0; slot < 4; ++slot) {
        const auto prefix = slot == 0 ? juce::String("sweep_") : "sweep_" + juce::String::charToString(static_cast<juce::juce_wchar>('a' + slot)) + "_";
        set(processor, prefix + "cut_start", 1234.0f);
        if (slot % 2 == 0) buttons[static_cast<size_t>(slot)]->onClick();
        else set(processor, "sweep_slot", static_cast<float>(slot));
        REQUIRE(processor.currentSweepSlot() == slot);
        REQUIRE(buttons[static_cast<size_t>(slot)]->getToggleState());
        const auto knob = std::ranges::find_if(sliders, [](auto* slider) { return std::abs(slider->getValue() - 1234.0) < 0.1; });
        REQUIRE(knob != sliders.end());
        (*knob)->setValue(2345.0, juce::sendNotificationSync);
        REQUIRE_THAT(processor.currentSweepSettings().cutoffStartHz, WithinAbs(2345.0f, 0.1f));
    }
    set(processor, "sweep_slot", 0.0f);
    set(processor, "sweep_cut_start", 1111.0f);
    const auto dragging = std::ranges::find_if(sliders, [](auto* slider) { return std::abs(slider->getValue() - 1111.0) < 0.1; });
    REQUIRE(dragging != sliders.end());
    GestureCounts gestureA(*processor.parameters.getParameter("sweep_cut_start"));
    GestureCounts gestureB(*processor.parameters.getParameter("sweep_b_cut_start"));
    {
        juce::Slider::ScopedDragNotification drag(**dragging);
        set(processor, "sweep_slot", 1.0f);
        (*dragging)->setValue(3333.0, juce::sendNotificationSync);
    }
    REQUIRE(gestureA.begins == 1);
    REQUIRE(gestureA.ends == 1);
    REQUIRE(gestureB.begins == 0);
    REQUIRE(gestureB.ends == 0);
    REQUIRE_THAT(processor.currentSweepSettings().cutoffStartHz, WithinAbs(2345.0f, 0.1f));
    REQUIRE_THAT((*dragging)->getValue(), WithinAbs(2345.0, 0.1));
    if (const char* preview = std::getenv("OEF_PREVIEW")) {
        // Factory defaults at the editor's default size. Hold the title pulse so the
        // file does not change just because the capture time moved.
        OpenEdmFilterProcessor fresh;
        fresh.displayBeatPhase.store(0.0f);
        std::unique_ptr<juce::AudioProcessorEditor> shot(fresh.createEditor());
        shot->setSize(OpenEdmFilterProcessor::kDefaultEditorWidth, OpenEdmFilterProcessor::kDefaultEditorHeight);
        REQUIRE(fresh.getCurrentProgram() == 0);
        REQUIRE(shot->getWidth() == OpenEdmFilterProcessor::kDefaultEditorWidth);
        REQUIRE(shot->getHeight() == OpenEdmFilterProcessor::kDefaultEditorHeight);
        const auto snapshot = shot->createComponentSnapshot(shot->getLocalBounds(), true, 1.0f);
        REQUIRE(snapshot.getWidth() == shot->getWidth());
        REQUIRE(snapshot.getHeight() == shot->getHeight());
        juce::FileOutputStream stream{ juce::File(preview) };
        REQUIRE(stream.openedOk());
        REQUIRE(stream.setPosition(0));
        REQUIRE(stream.truncate().wasOk());
        REQUIRE(juce::PNGImageFormat().writeImageToStream(snapshot, stream));
    }
    if (const char* capture = std::getenv("OEF_CAPTURE")) {
        processor.setCurrentProgram(31);
        set(processor, "sweep_slot", 3.0f);
        set(processor, "sweep_d_bars", static_cast<float>(openedm::params::sweepBarIndex(1)));
        set(processor, "input_gain", 3.0f);
        set(processor, "a_response", 2);
        set(processor, "b_response", 4);
        processor.prepareToPlay(44100.0, 4096);
        juce::AudioBuffer<float> signal(2, 4096);
        for (int sample = 0; sample < signal.getNumSamples(); ++sample) {
            const float time = static_cast<float>(sample) / 44100.0f;
            const float value = 0.25f * std::sin(2.0f * std::numbers::pi_v<float> * 110.0f * time)
                + 0.14f * std::sin(2.0f * std::numbers::pi_v<float> * 440.0f * time)
                + 0.07f * std::sin(2.0f * std::numbers::pi_v<float> * 2000.0f * time);
            signal.setSample(0, sample, value);
            signal.setSample(1, sample, value);
        }
        juce::MidiBuffer midi;
        processor.processBlock(signal, midi);
        processor.displaySweepPhase.store(0.9f);
        auto* effects = *std::ranges::find_if(editor->getChildren(), [](auto* child) { return dynamic_cast<openedm::ui::EffectsPanel*>(child) != nullptr; });
        dynamic_cast<openedm::ui::LevelMeter*>(effects->findChildWithID("inputMeter"))->update();
        dynamic_cast<openedm::ui::LevelMeter*>(effects->findChildWithID("outputMeter"))->update();
        const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        juce::FileOutputStream stream{ juce::File(capture) };
        REQUIRE(stream.openedOk());
        REQUIRE(stream.setPosition(0));
        REQUIRE(stream.truncate().wasOk());
        REQUIRE(juce::PNGImageFormat().writeImageToStream(snapshot, stream));
    }
}

namespace {

juce::MemoryBlock stateFromXml(const juce::XmlElement& xml) {
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary(xml, saved);
    return saved;
}

class ScriptedPlayHead : public juce::AudioPlayHead {
public:
    juce::Optional<PositionInfo> getPosition() const override { return info; }
    PositionInfo info;
};

double wrappedDistance(double actual, double expected) {
    const double wrapped = expected - std::floor(expected);
    const double delta = std::abs(actual - wrapped);
    return std::min(delta, 1.0 - delta);
}

} // namespace

TEST_CASE("Sweep bar choices expose 1, 2 and 4 bars without dropping the longer frames", "[plugin]") {
    OpenEdmFilterProcessor processor;
    REQUIRE(processor.currentSweepSettings().bars == 8);
    for (const char* id : { "sweep_bars", "sweep_b_bars", "sweep_c_bars", "sweep_d_bars" }) {
        auto* parameter = processor.parameters.getParameter(id);
        auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter);
        REQUIRE(choice != nullptr);
        REQUIRE(parameter->isAutomatable());
        REQUIRE(parameter->getNumSteps() == static_cast<int>(openedm::params::kSweepBarCounts.size()));
        REQUIRE(parameter->getVersionHint() == openedm::params::kSweepBarsParameterVersion);
        REQUIRE(choice->choices[static_cast<int>(openedm::params::sweepBarIndex(1))] == "1 Bar");
        REQUIRE(choice->choices[static_cast<int>(openedm::params::sweepBarIndex(2))] == "2 Bars");
        REQUIRE(choice->choices[static_cast<int>(openedm::params::sweepBarIndex(4))] == "4 Bars");
        REQUIRE(choice->choices[static_cast<int>(openedm::params::sweepBarIndex(64))] == "64 Bars");
        REQUIRE(choice->getIndex() == openedm::params::kDefaultSweepBarIndex);
    }

    const std::array<const char*, 4> ids{ "sweep_bars", "sweep_b_bars", "sweep_c_bars", "sweep_d_bars" };
    const std::array<int, 4> lengths{ 1, 2, 4, 16 };
    for (size_t slot = 0; slot < ids.size(); ++slot) {
        set(processor, "sweep_slot", static_cast<float>(slot));
        set(processor, ids[slot], static_cast<float>(openedm::params::sweepBarIndex(lengths[slot])));
        REQUIRE(processor.currentSweepSettings().bars == lengths[slot]);
    }

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    const auto xml = juce::AudioProcessor::getXmlFromBinary(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(xml != nullptr);
    REQUIRE(xml->getIntAttribute("barsLayout") == openedm::params::kSweepBarsLayout);
    OpenEdmFilterProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    for (size_t slot = 0; slot < ids.size(); ++slot) {
        set(restored, "sweep_slot", static_cast<float>(slot));
        REQUIRE(restored.currentSweepSettings().bars == lengths[slot]);
    }
}

TEST_CASE("Legacy sweep bar indices still mean 4, 8, 16, 32 and 64 bars", "[plugin]") {
    juce::XmlElement legacy("openEDMFilter");
    const std::array<std::pair<const char*, double>, 4> slots{{
        { "sweep_bars", 0.0 }, { "sweep_b_bars", 1.0 }, { "sweep_c_bars", 2.0 }, { "sweep_d_bars", 4.0 }
    }};
    const std::array<int, 4> expected{ 4, 8, 16, 64 };
    for (const auto& [id, index] : slots) {
        auto* child = legacy.createNewChildElement("PARAM");
        child->setAttribute("id", id);
        child->setAttribute("value", index);
    }
    const auto saved = stateFromXml(legacy);
    OpenEdmFilterProcessor processor;
    set(processor, "sweep_bars", static_cast<float>(openedm::params::sweepBarIndex(1)));
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    for (size_t slot = 0; slot < expected.size(); ++slot) {
        set(processor, "sweep_slot", static_cast<float>(slot));
        REQUIRE(processor.currentSweepSettings().bars == expected[slot]);
    }

    juce::XmlElement thirtyTwo("openEDMFilter");
    auto* bars = thirtyTwo.createNewChildElement("PARAM");
    bars->setAttribute("id", "sweep_bars");
    bars->setAttribute("value", 3.0);
    const auto thirtyTwoState = stateFromXml(thirtyTwo);
    processor.setStateInformation(thirtyTwoState.getData(), static_cast<int>(thirtyTwoState.getSize()));
    REQUIRE(processor.currentSweepSettings().bars == 32);

    juce::MemoryBlock resaved;
    processor.getStateInformation(resaved);
    OpenEdmFilterProcessor again;
    again.setStateInformation(resaved.getData(), static_cast<int>(resaved.getSize()));
    REQUIRE(again.currentSweepSettings().bars == 32);

    juce::XmlElement current("openEDMFilter");
    current.setAttribute("barsLayout", openedm::params::kSweepBarsLayout);
    auto* oneBar = current.createNewChildElement("PARAM");
    oneBar->setAttribute("id", "sweep_bars");
    oneBar->setAttribute("value", 0.0);
    const auto currentState = stateFromXml(current);
    processor.setStateInformation(currentState.getData(), static_cast<int>(currentState.getSize()));
    REQUIRE(processor.currentSweepSettings().bars == 1);
}

TEST_CASE("Named factory presets keep their bar lengths", "[plugin]") {
    OpenEdmFilterProcessor processor;
    const std::array<std::pair<const char*, int>, 6> named{{
        { "Tech / HP Drop Down (4 Bar)", 4 }, { "Rolling / Acid Bar Pump (4 Bar)", 4 },
        { "Progressive / LP Sweep Up (8 Bar)", 8 }, { "Uplifting / Air HP Riser (16 Bar)", 16 },
        { "Tech / Big Room Build (32 Bar)", 32 }, { "Uplifting / Odyssey (64 Bar)", 64 }
    }};
    const auto& presets = openedm::presets::factoryPresets();
    for (const auto& [name, bars] : named) {
        const auto preset = std::ranges::find_if(presets, [&](const auto& item) { return std::string(item.name) == name; });
        REQUIRE(preset != presets.end());
        processor.setCurrentProgram(static_cast<int>(std::distance(presets.begin(), preset)));
        REQUIRE(processor.currentSweepSettings().bars == bars);
    }
}

TEST_CASE("Host transport drives 1, 2 and 4 bar frames through stop, start and tempo changes", "[plugin]") {
    constexpr double sampleRate = 48000.0;
    constexpr int block = 480;
    OpenEdmFilterProcessor processor;
    processor.prepareToPlay(sampleRate, block);
    set(processor, "sweep_on", 1.0f);
    set(processor, "clock_sync", 1.0f);
    ScriptedPlayHead playHead;
    processor.setPlayHead(&playHead);
    juce::AudioBuffer<float> buffer(2, block);
    buffer.clear();
    juce::MidiBuffer midi;

    for (int bars : { 1, 2, 4 }) {
        set(processor, "sweep_bars", static_cast<float>(openedm::params::sweepBarIndex(bars)));
        const double beatsPerBar = 4.0;
        const double totalBeats = beatsPerBar * static_cast<double>(bars);
        playHead.info.setTimeSignature(juce::AudioPlayHead::TimeSignature{ 4, 4 });
        playHead.info.setBpm(120.0);
        playHead.info.setIsPlaying(false);
        playHead.info.setPpqPosition(2.0);
        processor.processBlock(buffer, midi);
        REQUIRE(wrappedDistance(processor.displaySweepPhase.load(), 2.0 / totalBeats) < 1e-5);
        const float held = processor.displaySweepPhase.load();
        processor.processBlock(buffer, midi);
        REQUIRE_THAT(processor.displaySweepPhase.load(), WithinAbs(held, 1e-6));

        playHead.info.setIsPlaying(true);
        playHead.info.setPpqPosition(0.0);
        double bpm = 120.0;
        playHead.info.setBpm(bpm);
        for (int repeat = 0; repeat < 2; ++repeat) {
            if (repeat == 1) {
                bpm = 180.0;
                playHead.info.setBpm(bpm);
            }
            const double start = *playHead.info.getPpqPosition();
            processor.processBlock(buffer, midi);
            const double elapsed = bpm / 60.0 * (static_cast<double>(block) / sampleRate);
            REQUIRE(wrappedDistance(processor.displaySweepPhase.load(), (start + elapsed) / totalBeats) < 1e-5);
            playHead.info.setPpqPosition(start + elapsed);
        }
    }

    set(processor, "sweep_bars", static_cast<float>(openedm::params::sweepBarIndex(1)));
    set(processor, "time_sig", 0.0f);
    playHead.info.setIsPlaying(false);
    playHead.info.setTimeSignature(juce::AudioPlayHead::TimeSignature{ 7, 8 });
    playHead.info.setPpqPosition(1.75);
    processor.processBlock(buffer, midi);
    REQUIRE(wrappedDistance(processor.displaySweepPhase.load(), 0.5) < 1e-5);

    set(processor, "time_sig", 3.0f); // 5/4, which overrides the host signature
    playHead.info.setTimeSignature(juce::AudioPlayHead::TimeSignature{ 4, 4 });
    playHead.info.setPpqPosition(2.5);
    processor.processBlock(buffer, midi);
    REQUIRE(wrappedDistance(processor.displaySweepPhase.load(), 0.5) < 1e-5);

    set(processor, "clock_sync", 0.0f);
    set(processor, "tempo_bpm", 120.0f);
    set(processor, "time_sig", 0.0f);
    playHead.info.setIsPlaying(false);
    playHead.info.setPpqPosition(0.0);
    processor.processBlock(buffer, midi);
    const float internalStart = processor.displaySweepPhase.load();
    processor.processBlock(buffer, midi);
    const double internalElapsed = 120.0 / 60.0 * (static_cast<double>(block) / sampleRate) / 4.0;
    REQUIRE(wrappedDistance(processor.displaySweepPhase.load(), static_cast<double>(internalStart) + internalElapsed) < 1e-4);
    processor.setPlayHead(nullptr);
}

TEST_CASE("The bars selector lists 1, 2 and 4 bars and edits the active slot", "[plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    openedm::ui::SweepPanel* panel = nullptr;
    for (auto* child : editor->getChildren()) {
        if (auto* sweep = dynamic_cast<openedm::ui::SweepPanel*>(child)) panel = sweep;
    }
    REQUIRE(panel != nullptr);
    bool labelled = false;
    juce::ComboBox* barsBox = nullptr;
    for (auto* child : panel->getChildren()) {
        if (auto* label = dynamic_cast<juce::Label*>(child); label != nullptr && label->getText() == "BARS") {
            labelled = true;
        }
        for (auto* grandchild : child->getChildren()) {
            if (auto* box = dynamic_cast<juce::ComboBox*>(grandchild);
                box != nullptr && box->getNumItems() > 2 && box->getItemText(0) == "1 Bar") {
                barsBox = box;
            }
        }
    }
    REQUIRE(labelled);
    REQUIRE(barsBox != nullptr);
    REQUIRE(barsBox->getItemText(1) == "2 Bars");
    REQUIRE(barsBox->getItemText(2) == "4 Bars");
    REQUIRE(barsBox->getItemText(openedm::params::sweepBarIndex(8)) == "8 Bars");
    for (int bars : { 1, 2, 4 }) {
        barsBox->setSelectedItemIndex(openedm::params::sweepBarIndex(bars), juce::sendNotificationSync);
        REQUIRE(processor.currentSweepSettings().bars == bars);
        REQUIRE(barsBox->getText() == (bars == 1 ? "1 Bar" : juce::String(bars) + " Bars"));
    }
    set(processor, "sweep_slot", 2.0f);
    barsBox->setSelectedItemIndex(openedm::params::sweepBarIndex(2), juce::sendNotificationSync);
    REQUIRE(processor.currentSweepSettings().bars == 2);
    set(processor, "sweep_slot", 0.0f);
    REQUIRE(processor.currentSweepSettings().bars == 4);
}

TEST_CASE("All 96 factory presets load and report valid program names", "[plugin]") {
    OpenEdmFilterProcessor processor;
    const auto& presets = openedm::presets::factoryPresets();
    REQUIRE(presets.size() == 96);
    REQUIRE(processor.getNumPrograms() == 96);

    for (int i = 0; i < 96; ++i) {
        REQUIRE(processor.getProgramName(i).isNotEmpty());
        processor.setCurrentProgram(i);
        REQUIRE(processor.getCurrentProgram() == i);
    }

    const auto upliftingCount = std::ranges::count_if(presets, [](const auto& p) {
        return juce::String(p.name).startsWith("Uplifting / ");
    });
    REQUIRE(upliftingCount == 24);

    // Spot-check key genre presets
    const std::array<const char*, 7> genrePresets{{
        "Uplifting / Euphoria Build (16 Bar)",
        "Rolling / Acid Bar Pump (4 Bar)",
        "Progressive / Warm Sweep (16 Bar)",
        "Psy / 140 Rolling Squelch",
        "Tech / House Acid",
        "Bass & Breaks / Dubstep Wobble",
        "Classic / Phaser Notch"
    }};
    std::ranges::for_each(genrePresets, [&](const char* name) {
        const auto it = std::ranges::find_if(presets, [&](const auto& p) { return std::string(p.name) == name; });
        REQUIRE(it != presets.end());
    });

    const auto& sweepPresets = openedm::presets::sweepSlopePresets();
    REQUIRE(sweepPresets.size() == 32);
    const auto upliftingSweeps = std::ranges::count_if(sweepPresets, [](const auto& p) {
        return juce::String(p.name).startsWith("Uplifting / ");
    });
    REQUIRE(upliftingSweeps == 8);
    const std::array<const char*, 3> autoPresets{{
        "Uplifting / 138 Climax (16 Bar)",
        "Psy / Acid Laser (4 Bar)",
        "Bass & Breaks / Sub Reset (1 Bar)"
    }};
    std::ranges::for_each(autoPresets, [&](const char* name) {
        const auto it = std::ranges::find_if(sweepPresets, [&](const auto& p) { return std::string(p.name) == name; });
        REQUIRE(it != sweepPresets.end());
    });
}

TEST_CASE("Approved routing and modulation controls are automatable and recalled", "[approved][plugin]") {
    OpenEdmFilterProcessor processor;
    REQUIRE(processor.acceptsMidi());
    const std::array<std::pair<const char*, float>, 13> controls{{
        {"routing", 1}, {"parallel_blend", 0.8f}, {"fm_shape", 4}, {"fm_target", 2},
        {"stereo_mode", 2}, {"a_midi_source", 3}, {"a_midi_cc", 74}, {"a_midi_depth", 0.6f},
        {"a_step_mode", 2}, {"a_step_pulses", 5}, {"a_step_3", -0.75f},
        {"a_lane2_depth", -0.5f}, {"a_lane2_phase", 0.25f}
    }};
    for (const auto& [id, value] : controls) {
        set(processor, id, value);
        REQUIRE(processor.parameters.getParameter(id)->isAutomatable());
    }
    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmFilterProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    for (const auto& [id, value] : controls)
        REQUIRE_THAT(restored.parameters.getRawParameterValue(id)->load(), WithinAbs(value, 1e-5f));
}

TEST_CASE("MIDI velocity aftertouch and selected CC reach DSP at their sample offsets", "[approved][plugin]") {
    for (int source : {1, 2, 3}) {
        OpenEdmFilterProcessor processor;
        processor.prepareToPlay(48000.0, 4800);
        set(processor, "a_cutoff", 1000.0f);
        set(processor, "a_midi_source", static_cast<float>(source));
        set(processor, "a_midi_depth", 1.0f / 6.0f);
        set(processor, "a_midi_cc", 74.0f);
        juce::AudioBuffer<float> buffer(2, 4800);
        buffer.clear();
        juce::MidiBuffer midi;
        if (source == 1) midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(127)), 2400);
        if (source == 2) midi.addEvent(juce::MidiMessage::channelPressureChange(1, 127), 2400);
        if (source == 3) {
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 73, 127), 0);
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, 127), 2400);
        }
        processor.processBlock(buffer, midi);
        REQUIRE_THAT(processor.displayCutoffs[0].load(), WithinAbs(2000.0f, 2.0f));
    }
}

TEST_CASE("MIDI event offsets match split-block processing and note release resets velocity", "[approved][plugin]") {
    OpenEdmFilterProcessor whole, split;
    for (auto* processor : {&whole, &split}) {
        processor->prepareToPlay(48000, 4800);
        set(*processor, "a_cutoff", 1000);
        set(*processor, "a_midi_source", 1);
        set(*processor, "a_midi_depth", 1.0f / 6.0f);
    }
    juce::AudioBuffer<float> block(2, 4800), first(2, 2400), second(2, 2400);
    for (int i = 0; i < 4800; ++i) {
        const float value = 0.1f * std::sin(static_cast<float>(i) * 0.2f);
        for (int channel = 0; channel < 2; ++channel) {
            block.setSample(channel, i, value);
            (i < 2400 ? first : second).setSample(channel, i % 2400, value);
        }
    }
    juce::MidiBuffer events;
    events.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(127)), 2400);
    whole.processBlock(block, events);
    juce::MidiBuffer empty;
    split.processBlock(first, empty);
    REQUIRE_THAT(split.displayCutoffs[0].load(), WithinAbs(1000, 0.1));
    events.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(127)), 0);
    split.processBlock(second, events);
    for (int i = 0; i < 4800; ++i)
        REQUIRE_THAT(block.getSample(0, i), WithinAbs((i < 2400 ? first : second).getSample(0, i % 2400), 1e-6));
    events.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    split.processBlock(block, events);
    REQUIRE_THAT(split.displayCutoffs[0].load(), WithinAbs(1000, 0.1));
}

TEST_CASE("Old sessions reset new modulation controls to neutral defaults", "[approved][plugin]") {
    OpenEdmFilterProcessor processor;
    set(processor, "routing", 1);
    set(processor, "stereo_mode", 2);
    set(processor, "a_midi_depth", 1);
    set(processor, "a_step_mode", 2);
    set(processor, "a_response", 2);
    set(processor, "b_response", 4);
    juce::XmlElement legacy("openEDMFilter");
    auto* cutoff = legacy.createNewChildElement("PARAM");
    cutoff->setAttribute("id", "a_cutoff");
    cutoff->setAttribute("value", 1234);
    juce::MemoryBlock data;
    juce::AudioProcessor::copyXmlToBinary(legacy, data);
    processor.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    for (auto id : {"routing", "stereo_mode", "a_midi_depth", "a_step_mode", "fm_shape", "fm_target", "a_response", "b_response"})
        REQUIRE_THAT(processor.parameters.getRawParameterValue(id)->load(), WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(processor.parameters.getRawParameterValue("a_cutoff")->load(), WithinAbs(1234, 0.01));
}

TEST_CASE("Native editor opens and closes the routing and modulation controls", "[approved][plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* button = dynamic_cast<juce::TextButton*>(editor->findChildWithID("advancedModulation"));
    REQUIRE(button != nullptr);
    button->onClick();
    auto control = std::ranges::find_if(editor->getChildren(), [](auto* child) {
        return dynamic_cast<openedm::ui::ModulationPage*>(child) != nullptr;
    });
    auto sweep = std::ranges::find_if(editor->getChildren(), [](auto* child) {
        return dynamic_cast<openedm::ui::SweepPanel*>(child) != nullptr;
    });
    REQUIRE(control != editor->getChildren().end());
    REQUIRE((*control)->isVisible());
    REQUIRE((*control)->getWidth() > 500);
    REQUIRE_FALSE((*sweep)->isVisible());
    REQUIRE(button->getToggleState());
    REQUIRE(std::ranges::none_of(editor->getChildren(), [](auto* child) {
        return dynamic_cast<juce::GenericAudioProcessorEditor*>(child) != nullptr;
    }));
    const auto audit = [&](auto&& self, juce::Component& parent) -> void {
        for (auto* child : parent.getChildren()) {
            if (!child->isVisible()) continue;
            if (dynamic_cast<juce::Button*>(child) || dynamic_cast<juce::ComboBox*>(child)) {
                const auto hit = editor->getLocalArea(child, child->getLocalBounds());
                CHECK(hit.getWidth() >= 44);
                CHECK(hit.getHeight() >= 36);
            } else self(self, *child);
        }
    };
    audit(audit, **control);
    if (const char* capture = std::getenv("OEF_CAPTURE")) {
        const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
        juce::FileOutputStream stream{ juce::File(capture).getSiblingFile("native-advanced.png") };
        REQUIRE(stream.openedOk());
        REQUIRE(stream.setPosition(0));
        REQUIRE(stream.truncate().wasOk());
        REQUIRE(juce::PNGImageFormat().writeImageToStream(snapshot, stream));
    }
    button->onClick();
    REQUIRE_FALSE((*control)->isVisible());
    REQUIRE((*sweep)->isVisible());
}

TEST_CASE("Reset All Controllers only resets the addressed MIDI channel", "[approved][plugin][midi-reset]") {
    OpenEdmFilterProcessor processor;
    processor.prepareToPlay(48000.0, 4800);
    set(processor, "a_cutoff", 1000.0f);
    set(processor, "a_midi_depth", 1.0f / 6.0f);
    set(processor, "a_midi_cc", 74.0f);
    juce::AudioBuffer<float> buffer(2, 4800);
    buffer.clear();
    juce::MidiBuffer midi;
    float initialCutoff = 2000.0f;
    SECTION("channel pressure survives another channel reset") {
        set(processor, "a_midi_source", 2.0f);
        midi.addEvent(juce::MidiMessage::channelPressureChange(1, 127), 0);
        midi.addEvent(juce::MidiMessage::channelPressureChange(2, 64), 0);
    }
    SECTION("polyphonic pressure survives another channel reset") {
        set(processor, "a_midi_source", 2.0f);
        midi.addEvent(juce::MidiMessage::aftertouchChange(1, 60, 127), 0);
        midi.addEvent(juce::MidiMessage::aftertouchChange(2, 61, 64), 0);
    }
    SECTION("selected CC falls back to the latest unreset channel value") {
        set(processor, "a_midi_source", 3.0f);
        initialCutoff = 1000.0f * std::exp2(64.0f / 127.0f);
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, 127), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(2, 74, 64), 0);
    }
    processor.processBlock(buffer, midi);
    REQUIRE_THAT(processor.displayCutoffs[0].load(), WithinAbs(initialCutoff, 2.0f));
    midi.addEvent(juce::MidiMessage::controllerEvent(2, 121, 0), 0);
    processor.processBlock(buffer, midi);
    REQUIRE_THAT(processor.displayCutoffs[0].load(), WithinAbs(2000.0f, 2.0f));
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 121, 0), 0);
    processor.processBlock(buffer, midi);
    REQUIRE_THAT(processor.displayCutoffs[0].load(), WithinAbs(1000.0f, 1.0f));
}

TEST_CASE("Extended types recall on both lanes and survive stereo routing and modulation changes", "[plugin][extended]") {
    OpenEdmFilterProcessor source, processor;
    set(source, "a_response", 2);
    set(source, "b_response", 4);
    juce::MemoryBlock saved;
    source.getStateInformation(saved);
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(processor.parameters.getRawParameterValue("a_response")->load() == 2);
    REQUIRE(processor.parameters.getRawParameterValue("b_response")->load() == 4);
    for (int band = 0; band < 2; ++band) {
        auto* original = processor.parameters.getParameter(openedm::params::bandId(band, "type"));
        REQUIRE(original->getNumSteps() == 4);
        for (int step = 0; step < 4; ++step)
            CHECK_THAT(original->convertFrom0to1(float(step) / 3), WithinAbs(step, 1e-6));
    }
    processor.prepareToPlay(48000, 2048);
    set(processor, "a_lfo_depth", 0.6f);
    set(processor, "b_cutoff", 950);
    juce::AudioBuffer<float> buffer(2, 2048);
    juce::MidiBuffer midi;
    for (int routing = 0; routing < 2; ++routing) for (int stereo = 0; stereo < 3; ++stereo) {
        set(processor, "routing", float(routing));
        set(processor, "stereo_mode", float(stereo));
        for (int mode = 1; mode <= 4; ++mode) {
            set(processor, "a_response", float(mode));
            set(processor, "b_response", float(5 - mode));
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                buffer.setSample(0, i, 0.08f * std::sin(0.06f * float(i)));
                buffer.setSample(1, i, 0.05f * std::cos(0.09f * float(i)));
            }
            processor.processBlock(buffer, midi);
            CHECK(buffer.getMagnitude(0, 2048) < 2.0f);
            CHECK(buffer.getRMSLevel(1, 512, 1536) > 0.0001f);
        }
    }
    ScriptedPlayHead host;
    host.info.setIsPlaying(true);
    host.info.setBpm(120.0);
    host.info.setPpqPosition(-0.75);
    processor.setPlayHead(&host);
    processor.processBlock(buffer, midi);
    CHECK_THAT(processor.animationBeatPhase(0), WithinAbs(0.25 + 2048.0 / 24000, 0.001));
    host.info.setPpqPosition(8.5);
    host.info.setBpm(60.0);
    processor.processBlock(buffer, midi);
    CHECK_THAT(processor.animationBeatPhase(0), WithinAbs(0.5 + 2048.0 / 48000, 0.001));
    host.info.setIsPlaying(false);
    processor.processBlock(buffer, midi);
    CHECK(processor.animationBeatPhase(0) == 1);
    processor.setPlayHead(nullptr);
    processor.processBlock(buffer, midi);
    CHECK(processor.animationBeatPhase(0) < 1);
}

TEST_CASE("The editor owns a tooltip window and the listed controls have tooltips", "[plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    std::map<juce::String, int> labelsWithTips;
    const auto collect = [&](auto&& self, juce::Component& parent) -> void {
        std::ranges::for_each(parent.getChildren(), [&](juce::Component* child) {
            self(self, *child);
            if (auto* button = dynamic_cast<juce::TextButton*>(child); button != nullptr && button->getTooltip().isNotEmpty())
                ++labelsWithTips[button->getButtonText()];
            if (dynamic_cast<openedm::ui::Knob*>(child) == nullptr)
                return;
            auto* slider = dynamic_cast<juce::Slider*>(child->getChildComponent(0));
            auto* title = dynamic_cast<juce::Label*>(child->getChildComponent(1));
            if (slider->getTooltip().isNotEmpty())
                ++labelsWithTips[title->getText()];
        });
    };
    collect(collect, *editor);

    REQUIRE(std::ranges::any_of(editor->getChildren(), [](auto* child) { return dynamic_cast<juce::TooltipWindow*>(child) != nullptr; }));
    CHECK(labelsWithTips["INERTIA"] == 1);
    CHECK(labelsWithTips["BYPASS ALL"] == 1);
    CHECK(labelsWithTips["HOST SYNC"] == 1);
    CHECK(labelsWithTips["SOURCE"] == 2);
    CHECK(labelsWithTips["ONSET"] == 2);
    CHECK(labelsWithTips["DRIFT"] == 1);
    CHECK(labelsWithTips["SUB KEEP"] == 1);
    CHECK(labelsWithTips["SPREAD"] == 1);
}

TEST_CASE("Every float parameter reads back the text it displays", "[plugin][text]") {
    OpenEdmFilterProcessor processor;
    std::vector<juce::AudioProcessorParameter*> floats;
    std::ranges::copy_if(processor.getParameters(), std::back_inserter(floats),
        [](auto* parameter) { return dynamic_cast<juce::AudioParameterFloat*>(parameter) != nullptr; });
    std::vector<std::string> mismatches;
    std::ranges::for_each(floats, [&](juce::AudioProcessorParameter* parameter) {
        std::ranges::for_each(std::array{ 0.0f, 0.03f, 0.1f, 0.25f, 0.5f, 0.75f, 0.97f, 1.0f, parameter->getDefaultValue() }, [&](float normalised) {
            const auto shown = parameter->getText(normalised, 0);
            const auto readBack = parameter->getText(parameter->getValueForText(shown), 0);
            if (readBack != shown)
                mismatches.push_back((parameter->getName(40) + ": " + shown + " -> " + readBack).toStdString());
        });
    });

    REQUIRE(floats.size() > 100);
    CHECK(mismatches.empty());
}

TEST_CASE("Typed percent, kHz, seconds and dB land on the value they name", "[plugin][text]") {
    OpenEdmFilterProcessor processor;
    const auto typed = [&](const char* id, const char* text) {
        auto* parameter = processor.parameters.getParameter(id);
        return parameter->convertFrom0to1(parameter->getValueForText(text));
    };
    const auto shown = [&](const char* id, float value) {
        auto* parameter = processor.parameters.getParameter(id);
        return parameter->getText(parameter->convertTo0to1(value), 0);
    };

    CHECK_THAT(typed("mix", "50%"), WithinAbs(0.5f, 1e-4f));
    CHECK_THAT(typed("mix", "50"), WithinAbs(0.5f, 1e-4f));
    CHECK_THAT(typed("a_reso", "40"), WithinAbs(0.4f, 1e-4f));
    CHECK_THAT(typed("a_lfo_depth", "-25%"), WithinAbs(-0.25f, 1e-4f));
    CHECK_THAT(typed("a_cutoff", "2k"), WithinAbs(2000.0f, 0.5f));
    CHECK_THAT(typed("a_cutoff", "2 kHz"), WithinAbs(2000.0f, 0.5f));
    CHECK_THAT(typed("a_cutoff", "850 Hz"), WithinAbs(850.0f, 0.5f));
    CHECK_THAT(typed("a_env_release", "1.2 s"), WithinAbs(1200.0f, 0.5f));
    CHECK_THAT(typed("a_env_release", "150 ms"), WithinAbs(150.0f, 0.5f));
    CHECK_THAT(typed("output", "+3 dB"), WithinAbs(3.0f, 1e-3f));
    CHECK(shown("output", 3.0f) == "+3.0 dB");
    CHECK(shown("output", -0.04f) == "0.0 dB");
    CHECK(shown("a_env_release", 1200.0f) == "1.20 s");
    CHECK(shown("a_env_attack", 4.5f) == "4.5 ms");
    CHECK(shown("a_env_release", 160.0f) == "160 ms");
    CHECK(shown("a_cutoff", 850.0f) == "850 Hz");
    CHECK(shown("a_cutoff", 20.0f) == "20 Hz");
    CHECK(shown("fm_rate", 0.5f) == "0.5 Hz");
    CHECK(shown("a_cutoff", 6800.0f) == "6.80 kHz");
    CHECK(shown("a_lfo_depth", 0.2f) == "+20%");
}

TEST_CASE("Auto Sweep dims the cutoff and resonance it drives and says why", "[plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    std::vector<juce::Component*> bands;
    std::ranges::copy_if(editor->getChildren(), std::back_inserter(bands),
        [](auto* child) { return dynamic_cast<openedm::ui::BandPanel*>(child) != nullptr; });
    REQUIRE(bands.size() == 2);
    auto* cutoffA = bands[0]->findChildWithID("a_cutoff");
    auto* resonanceA = bands[0]->findChildWithID("a_reso");
    auto* widthA = bands[0]->findChildWithID("a_width");
    auto* cutoffB = bands[1]->findChildWithID("b_cutoff");
    const auto tooltipOf = [](juce::Component* knob) { return dynamic_cast<juce::Slider*>(knob->getChildComponent(0))->getTooltip(); };
    auto& timer = dynamic_cast<juce::Timer&>(*editor);

    set(processor, "sweep_on", 1.0f);
    timer.timerCallback();
    CHECK_THAT(cutoffA->getAlpha(), WithinAbs(openedm::theme::kInactiveAlpha, 1e-6f));
    CHECK_THAT(resonanceA->getAlpha(), WithinAbs(openedm::theme::kInactiveAlpha, 1e-6f));
    CHECK(tooltipOf(cutoffA).contains("AUTO SWEEP"));
    CHECK_THAT(widthA->getAlpha(), WithinAbs(1.0f, 1e-6f));
    CHECK_THAT(cutoffB->getAlpha(), WithinAbs(1.0f, 1e-6f));

    set(processor, "sweep_target", 1.0f);
    timer.timerCallback();
    CHECK_THAT(cutoffA->getAlpha(), WithinAbs(1.0f, 1e-6f));
    CHECK(tooltipOf(cutoffA).isEmpty());
    CHECK_THAT(cutoffB->getAlpha(), WithinAbs(openedm::theme::kInactiveAlpha, 1e-6f));

    set(processor, "sweep_on", 0.0f);
    set(processor, "b_bypass", 1.0f);
    timer.timerCallback();
    CHECK_THAT(cutoffB->getAlpha(), WithinAbs(openedm::theme::kInactiveAlpha, 1e-6f));
    CHECK(tooltipOf(cutoffB).contains("BYPASS"));
}

TEST_CASE("A click on the response display leaves a swept band alone", "[plugin][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    openedm::ui::ResponseView view(processor);
    view.setBounds(0, 0, 800, 120);
    set(processor, "sweep_on", 1.0f);
    set(processor, "b_cutoff", 1000.0f);
    // Before the first block, the swept A marker sits at the 80 Hz sweep start, left of B at 1 kHz.
    const juce::Point<float> nearA{ 190.0f, 60.0f };
    const auto now = juce::Time::getCurrentTime();
    const juce::MouseEvent press(juce::Desktop::getInstance().getMainMouseSource(), nearA, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &view, &view, now, nearA, now, 1, false);

    view.mouseDown(press);
    view.mouseUp(press);

    CHECK_THAT(processor.parameters.getRawParameterValue("a_cutoff")->load(), WithinAbs(20000.0f, 0.5f));
    CHECK(processor.parameters.getRawParameterValue("b_cutoff")->load() < 200.0f);
}

TEST_CASE("The look and feel draws disabled buttons, dials and menus dimmed", "[plugin][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    openedm::ui::EdmLookAndFeel lookAndFeel;
    juce::TextButton button("UNDO");
    juce::Slider dial(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
    juce::ComboBox menu;
    menu.addItem("Low Pass", 1);
    menu.setSelectedId(1);
    const auto ink = [&](juce::Component& control, bool isEnabled) {
        control.setLookAndFeel(&lookAndFeel);
        control.setBounds(0, 0, 80, 40);
        control.setEnabled(isEnabled);
        const auto image = control.createComponentSnapshot(control.getLocalBounds(), true, 1.0f);
        control.setLookAndFeel(nullptr);
        float total = 0.0f;
        std::ranges::for_each(std::views::iota(0, image.getWidth() * image.getHeight()), [&](int pixel) {
            const auto colour = image.getPixelAt(pixel % image.getWidth(), pixel / image.getWidth());
            total += colour.getBrightness() * colour.getFloatAlpha();
        });
        return total;
    };

    CHECK(ink(button, false) < 0.6f * ink(button, true));
    CHECK(ink(dial, false) < 0.6f * ink(dial, true));
    CHECK(ink(menu, false) < 0.6f * ink(menu, true));
}

TEST_CASE("AUTO SWEEP replaces the sweep BYPASS and engaged bypass draws as off", "[plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    std::vector<juce::TextButton*> buttons;
    const auto collect = [&](auto&& self, juce::Component& parent) -> void {
        std::ranges::for_each(parent.getChildren(), [&](juce::Component* child) {
            if (auto* button = dynamic_cast<juce::TextButton*>(child)) buttons.push_back(button);
            self(self, *child);
        });
    };
    collect(collect, *editor);
    const auto named = [&](const juce::String& text) {
        return std::ranges::count_if(buttons, [&](auto* button) { return button->getButtonText() == text; });
    };
    auto* panel = *std::ranges::find_if(editor->getChildren(), [](auto* child) { return dynamic_cast<openedm::ui::SweepPanel*>(child) != nullptr; });
    auto* autoSweep = dynamic_cast<juce::TextButton*>(panel->findChildWithID("sweep_on"));
    auto* bars = panel->findChildWithID("sweep_bars");
    auto* bypassAll = *std::ranges::find_if(buttons, [](auto* button) { return button->getButtonText() == "BYPASS ALL"; });

    CHECK(named("BYPASS ALL") == 1);
    CHECK(named("BYPASS") == 2);
    CHECK(bypassAll->findColour(juce::TextButton::buttonOnColourId) == openedm::theme::kBypassEngaged);
    CHECK_THAT(bars->getAlpha(), WithinAbs(openedm::theme::kInactiveAlpha, 1e-6f));

    set(processor, "sweep_on", 1.0f);
    set(processor, "sweep_bypass", 1.0f);
    CHECK_FALSE(autoSweep->getToggleState());
    autoSweep->onClick();
    CHECK(processor.currentSweepSettings().enabled);
    CHECK(autoSweep->getToggleState());
    CHECK_THAT(bars->getAlpha(), WithinAbs(1.0f, 1e-6f));
    autoSweep->onClick();
    CHECK_THAT(processor.parameters.getRawParameterValue("sweep_on")->load(), WithinAbs(0.0f, 1e-6f));
    CHECK_FALSE(autoSweep->getToggleState());
}

TEST_CASE("Host sync reports the host tempo and the time signature it uses", "[plugin]") {
    OpenEdmFilterProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    ScriptedPlayHead host;
    host.info.setIsPlaying(true);
    host.info.setBpm(100.0);
    host.info.setPpqPosition(0.0);
    host.info.setTimeSignature(juce::AudioPlayHead::TimeSignature{ 3, 4 });
    processor.setPlayHead(&host);
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midi;
    auto* signatures = dynamic_cast<juce::AudioParameterChoice*>(processor.parameters.getParameter("time_sig"));

    processor.processBlock(buffer, midi);
    CHECK(signatures->choices[0] == "Host");
    CHECK(processor.displayHostTempo.load());
    CHECK(processor.displayTimeSignature.load() == juce::AudioPlayHead::TimeSignature{ 3, 4 });

    set(processor, "time_sig", 3.0f);
    processor.processBlock(buffer, midi);
    CHECK(processor.displayTimeSignature.load() == juce::AudioPlayHead::TimeSignature{ 5, 4 });

    set(processor, "time_sig", 0.0f);
    set(processor, "clock_sync", 0.0f);
    processor.processBlock(buffer, midi);
    CHECK_FALSE(processor.displayHostTempo.load());
    CHECK(processor.displayTimeSignature.load() == juce::AudioPlayHead::TimeSignature{ 4, 4 });
    processor.setPlayHead(nullptr);
}

TEST_CASE("TEMPO dims while the host tempo runs the clock", "[plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    ScriptedPlayHead host;
    host.info.setBpm(126.0);
    processor.setPlayHead(&host);
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midi;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* panel = *std::ranges::find_if(editor->getChildren(), [](auto* child) { return dynamic_cast<openedm::ui::SweepPanel*>(child) != nullptr; });
    auto* tempo = panel->findChildWithID("tempo_bpm");
    auto& timer = dynamic_cast<juce::Timer&>(*editor);

    processor.processBlock(buffer, midi);
    timer.timerCallback();
    CHECK_THAT(tempo->getAlpha(), WithinAbs(openedm::theme::kInactiveAlpha, 1e-6f));
    CHECK(dynamic_cast<juce::Slider*>(tempo->getChildComponent(0))->getTooltip().contains("HOST SYNC"));

    set(processor, "clock_sync", 0.0f);
    processor.processBlock(buffer, midi);
    timer.timerCallback();
    CHECK_THAT(tempo->getAlpha(), WithinAbs(1.0f, 1e-6f));
    processor.setPlayHead(nullptr);
}

TEST_CASE("Response readouts sit beside their own markers and grid labels do not touch", "[plugin][ui]") {
    using openedm::ui::ResponseView;
    const juce::Rectangle<float> plot{ 14.0f, 12.0f, 900.0f, 92.0f };

    const auto bandA = ResponseView::readoutBounds({ 914.0f, 40.0f }, 120.0f, plot, {});
    const auto bandB = ResponseView::readoutBounds({ 14.0f, 40.0f }, 110.0f, plot, bandA);
    const auto crowded = ResponseView::readoutBounds({ 905.0f, 41.0f }, 120.0f, plot, bandA);

    CHECK(bandA.getRight() <= 914.0f);
    CHECK(bandB.getX() >= 14.0f);
    CHECK(bandB.getRight() < bandA.getX());
    CHECK_FALSE(crowded.intersects(bandA));
    CHECK(plot.contains(crowded));
    CHECK(ResponseView::labelledDecibels(plot) == std::vector<float>{ 0.0f, 12.0f, -12.0f, -24.0f, -36.0f });
    CHECK(ResponseView::labelledDecibels(plot.withHeight(49.0f)) == std::vector<float>{ 0.0f, -24.0f });
    CHECK(ResponseView::labelledDecibels(plot.withHeight(32.0f)) == std::vector<float>{ 0.0f });
}

TEST_CASE("Peak meters keep the maximum since the editor last read them", "[plugin]") {
    OpenEdmFilterProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    set(processor, "a_bypass", 1.0f);
    set(processor, "b_bypass", 1.0f);
    set(processor, "limiter", 0.0f);
    juce::MidiBuffer midi;
    const auto render = [&](float level) {
        juce::AudioBuffer<float> buffer(1, 512);
        buffer.clear();
        buffer.setSample(0, 100, level);
        processor.processBlock(buffer, midi);
    };

    render(0.4f);
    render(0.1f);
    CHECK_THAT(processor.displayInputPeak.exchange(0.0f), WithinAbs(0.4f, 1e-6f));
    render(0.1f);
    CHECK_THAT(processor.displayInputPeak.load(), WithinAbs(0.1f, 1e-6f));
}

TEST_CASE("Level meters hold a peak for one second and latch clips until clicked", "[plugin][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    std::atomic<float> peak{ 0.0f };
    openedm::ui::LevelMeter meter(peak, "Input", 30, openedm::theme::kAccentOut);
    const auto frames = [&](int count) { std::ranges::for_each(std::views::iota(0, count), [&](int) { meter.update(); }); };
    const auto now = juce::Time::getCurrentTime();
    const juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(), {}, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &meter, &meter, now, {}, now, 1, false);

    peak = 0.5f;
    frames(30);
    CHECK_THAT(meter.heldPeak(), WithinAbs(0.5f, 1e-6f));
    frames(1);
    CHECK(meter.heldPeak() < 0.5f);
    CHECK_FALSE(meter.isClipped());
    peak = 1.2f;
    frames(1);
    CHECK(meter.isClipped());
    frames(60);
    CHECK(meter.isClipped());
    meter.mouseDown(click);
    CHECK_FALSE(meter.isClipped());
}

TEST_CASE("Input and output meters show at the default and the minimum size", "[plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* effects = *std::ranges::find_if(editor->getChildren(), [](auto* child) { return dynamic_cast<openedm::ui::EffectsPanel*>(child) != nullptr; });
    auto* input = effects->findChildWithID("inputMeter");
    auto* output = effects->findChildWithID("outputMeter");

    CHECK(input->isVisible());
    CHECK(input->getHeight() >= 40);
    CHECK(output->getHeight() >= 40);
    editor->setSize(OpenEdmFilterProcessor::kMinEditorWidth, OpenEdmFilterProcessor::kMinEditorHeight);
    CHECK(input->getHeight() >= 40);
    CHECK(output->getHeight() >= 40);
    CHECK(effects->getLocalBounds().contains(output->getBounds()));
}

TEST_CASE("CEILING, FREEZE and the whole editor dim when their switch is off", "[plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* effects = *std::ranges::find_if(editor->getChildren(), [](auto* child) { return dynamic_cast<openedm::ui::EffectsPanel*>(child) != nullptr; });
    auto* freeze = *std::ranges::find_if(editor->getChildren(), [](auto* child) {
        auto* button = dynamic_cast<juce::TextButton*>(child);
        return button != nullptr && button->getButtonText() == "FREEZE";
    });
    auto* spectrum = dynamic_cast<juce::TextButton*>(*std::ranges::find_if(editor->getChildren(), [](auto* child) {
        auto* button = dynamic_cast<juce::TextButton*>(child);
        return button != nullptr && button->getButtonText() == "SPECTRUM";
    }));
    auto& editorTimer = dynamic_cast<juce::Timer&>(*editor);
    auto& effectsTimer = dynamic_cast<juce::Timer&>(*effects);

    set(processor, "limiter", 0.0f);
    effectsTimer.timerCallback();
    CHECK_THAT(effects->findChildWithID("ceiling")->getAlpha(), WithinAbs(openedm::theme::kInactiveAlpha, 1e-6f));
    spectrum->setToggleState(false, juce::sendNotificationSync);
    editorTimer.timerCallback();
    CHECK_THAT(freeze->getAlpha(), WithinAbs(openedm::theme::kInactiveAlpha, 1e-6f));
    CHECK(dynamic_cast<juce::TextButton*>(freeze)->getTooltip().contains("SPECTRUM"));
    set(processor, "master_bypass", 1.0f);
    editorTimer.timerCallback();
    CHECK_THAT(effects->getAlpha(), WithinAbs(openedm::theme::kInactiveAlpha, 1e-6f));
}

TEST_CASE("Parameter groups keep every ID at its old index", "[plugin]") {
    OpenEdmFilterProcessor processor;
    const auto& all = processor.getParameters();
    const std::array<int, 14> starts{ 0, 13, 26, 43, 58, 85, 117, 122, 150, 178, 179, 181, 184, 185 };
    std::vector<juce::String> ids;
    std::ranges::transform(starts, std::back_inserter(ids),
        [&](int index) { return dynamic_cast<juce::RangedAudioParameter*>(all[index])->paramID; });

    CHECK(all.size() == 186);
    CHECK(ids == std::vector<juce::String>{ "a_bypass", "b_bypass", "fm_rate", "clock_sync", "sweep_b_target",
        "sweep_crush_start", "routing", "a_midi_source", "b_midi_source", "input_gain", "a_response", "sub_preserve",
        "a_env_input", "b_env_input" });
    CHECK(processor.getParameterTree().getSubgroups(false).size() == 13);
    CHECK(processor.getParameterTree().getParameters(false).isEmpty());
}

TEST_CASE("The modulation page has a native control for every routing, step and MIDI parameter", "[plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    processor.modulationPageVisible = true;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* page = *std::ranges::find_if(editor->getChildren(), [](auto* child) { return dynamic_cast<openedm::ui::ModulationPage*>(child) != nullptr; });
    std::vector<juce::String> bound;
    const auto collect = [&](auto&& self, juce::Component& parent) -> void {
        std::ranges::for_each(parent.getChildren(), [&](juce::Component* child) {
            if (dynamic_cast<openedm::ui::Knob*>(child) != nullptr || dynamic_cast<openedm::ui::Choice*>(child) != nullptr)
                bound.push_back(child->getComponentID());
            self(self, *child);
        });
    };
    collect(collect, *page);
    std::ranges::sort(bound);
    std::vector<juce::String> expected{ "routing", "parallel_blend", "stereo_mode", "fm_shape", "fm_target" };
    std::ranges::for_each(std::array{ "a_", "b_" }, [&](const char* prefix) {
        std::ranges::for_each(std::array{ "midi_source", "midi_cc", "midi_depth", "step_mode", "step_length", "step_pulses",
                                          "step_rotation", "step_beats", "step_depth", "lane2_depth", "lane2_phase", "lane2_offset" },
                              [&](const char* id) { expected.push_back(juce::String(prefix) + id); });
    });
    std::ranges::sort(expected);
    CHECK(page->isVisible());
    CHECK(bound == expected);

    auto* bandA = *std::ranges::find_if(page->getChildren(), [](auto* child) { return dynamic_cast<openedm::ui::BandModulation*>(child) != nullptr; });
    auto* grid = dynamic_cast<openedm::ui::StepGrid*>(bandA->findChildWithID("a_step_grid"));
    REQUIRE(grid != nullptr);
    const auto now = juce::Time::getCurrentTime();
    const auto mouseAt = [&](juce::Point<float> position) {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                grid, grid, now, position, now, 1, false);
    };
    const float top = 6.0f;
    const float firstStep = 6.0f;
    const float thirdStep = 4.0f + (static_cast<float>(grid->getWidth()) - 8.0f) * 2.5f / 16.0f;
    grid->mouseDown(mouseAt({ firstStep, top }));
    grid->mouseDrag(mouseAt({ thirdStep, top }));
    grid->mouseUp(mouseAt({ thirdStep, top }));
    processor.flushEdits();
    CHECK(processor.parameters.getRawParameterValue("a_step_1")->load() > 0.9f);
    CHECK(processor.parameters.getRawParameterValue("a_step_2")->load() > 0.9f);
    CHECK(processor.parameters.getRawParameterValue("a_step_3")->load() > 0.9f);
    CHECK_THAT(processor.parameters.getRawParameterValue("a_step_4")->load(), WithinAbs(0.0f, 1e-6f));
    CHECK(processor.canUndoEdit());

    set(processor, "a_step_mode", 0.0f);
    dynamic_cast<juce::Timer&>(*editor).timerCallback();
    CHECK_THAT(grid->getAlpha(), WithinAbs(openedm::theme::kInactiveAlpha, 1e-6f));
    CHECK_THAT(page->findChildWithID("parallel_blend")->getAlpha(), WithinAbs(openedm::theme::kInactiveAlpha, 1e-6f));
    set(processor, "a_step_mode", 1.0f);
    set(processor, "routing", 1.0f);
    dynamic_cast<juce::Timer&>(*editor).timerCallback();
    CHECK_THAT(grid->getAlpha(), WithinAbs(1.0f, 1e-6f));
    CHECK_THAT(page->findChildWithID("parallel_blend")->getAlpha(), WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("Every slider, menu and button has a title for screen readers", "[plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmFilterProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    std::vector<std::string> untitled;
    std::map<juce::String, juce::String> titles;
    const auto collect = [&](auto&& self, juce::Component& parent) -> void {
        std::ranges::for_each(parent.getChildren(), [&](juce::Component* child) {
            const bool isControl = dynamic_cast<juce::Slider*>(child) != nullptr || dynamic_cast<juce::ComboBox*>(child) != nullptr
                || dynamic_cast<juce::TextButton*>(child) != nullptr;
            if (isControl && child->getTitle().isEmpty())
                untitled.push_back((parent.getComponentID() + "/" + child->getComponentID()).toStdString());
            if (auto* button = dynamic_cast<juce::TextButton*>(child)) titles[button->getButtonText()] = button->getTitle();
            self(self, *child);
        });
    };
    collect(collect, *editor);

    CHECK(untitled.empty());
    CHECK(titles["<"] == "Previous preset");
    CHECK(titles["A>B"] == "Copy A to B");
    CHECK(titles["LINK A"] == "Link crush rate to Filter A");
}
