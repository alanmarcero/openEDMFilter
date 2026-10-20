#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace openedm::ui {

using Apvts = juce::AudioProcessorValueTreeState;

class Knob : public juce::Component {
public:
    Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent);
    void resized() override;

private:
    juce::Slider slider_;
    juce::Label title_;
    Apvts::SliderAttachment attachment_;
};

class Choice : public juce::Component {
public:
    // An empty title draws the box alone, for compact header rows.
    Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent);
    void resized() override;

private:
    juce::ComboBox box_;
    juce::Label title_;
    std::unique_ptr<Apvts::ComboBoxAttachment> attachment_;
};

class Toggle : public juce::Component {
public:
    Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent);
    void resized() override;

private:
    juce::TextButton button_;
    Apvts::ButtonAttachment attachment_;
};

// Places components left to right in equal-width cells.
void layoutRow(juce::Rectangle<int> area, std::initializer_list<juce::Component*> components);

// Fills a rounded panel and draws its title in the accent colour.
void drawPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent);

// Draws a small dim caption, used to name groups of controls inside a panel.
void drawCaption(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text);

} // namespace openedm::ui
