#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <utility>
#include <vector>

namespace openedm::presets {

// Values are in parameter units (Hz, ms, dB, 0..1, or a choice index).
// A factory preset sets the parameters it lists and resets every other parameter to its default.
struct FactoryPreset {
    const char* name;
    std::vector<std::pair<juce::String, float>> values;
};

const std::vector<FactoryPreset>& factoryPresets();
const std::vector<FactoryPreset>& sweepSlopePresets();

void populateComboMenu(juce::ComboBox& box, const std::vector<FactoryPreset>& items, bool fullItemText = true);
void apply(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state);
void applyPartial(const FactoryPreset& preset, juce::AudioProcessorValueTreeState& state);

} // namespace openedm::presets
