#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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
        set(processor, "sweep_bars", 0.0f);
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
    openedm::ui::SweepPanel* panel = nullptr;
    for (auto* child : editor->getChildren()) {
        if (auto* sweep = dynamic_cast<openedm::ui::SweepPanel*>(child)) panel = sweep;
    }
    REQUIRE(panel != nullptr);
    std::vector<juce::Slider*> sliders;
    std::array<juce::TextButton*, 4> buttons{};
    for (auto* child : panel->getChildren()) {
        if (auto* button = dynamic_cast<juce::TextButton*>(child)) {
            for (int slot = 0; slot < 4; ++slot) {
                if (button->getButtonText() == juce::String::charToString(static_cast<juce::juce_wchar>('A' + slot)))
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
    if (const char* capture = std::getenv("OEF_CAPTURE")) {
        processor.setCurrentProgram(31);
        set(processor, "sweep_slot", 3.0f);
        processor.displaySweepPhase.store(0.9f);
        const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        juce::FileOutputStream stream{ juce::File(capture) };
        REQUIRE(stream.openedOk());
        REQUIRE(stream.setPosition(0));
        REQUIRE(stream.truncate().wasOk());
        REQUIRE(juce::PNGImageFormat().writeImageToStream(snapshot, stream));
    }
}
