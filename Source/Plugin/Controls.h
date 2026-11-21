#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace openedm::ui {

using Apvts = juce::AudioProcessorValueTreeState;

inline constexpr float kInactiveAlpha = 0.4f;

class Knob : public juce::Component {
public:
    Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent);
    void resized() override;
    void bind(Apvts& state, const juce::String& parameterId);
    void setTooltip(const juce::String& text);

private:
    juce::Slider slider_;
    juce::Label title_;
    std::unique_ptr<Apvts::SliderAttachment> attachment_;
    bool dragging_ = false;
    Apvts* pendingState_ = nullptr;
    juce::String pendingId_;
};

class Choice : public juce::Component {
public:
    // An empty title draws the box alone, for compact header rows.
    Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent);
    void resized() override;
    void bind(Apvts& state, const juce::String& parameterId);
    void setTooltip(const juce::String& text);

private:
    juce::ComboBox box_;
    juce::Label title_;
    std::unique_ptr<Apvts::ComboBoxAttachment> attachment_;
};

class FilterTypeMenu : public juce::ComboBox {
public:
    FilterTypeMenu(Apvts& state, int band, juce::Colour accent);
private:
    juce::AudioParameterChoice& original_;
    juce::AudioParameterChoice& response_;
    juce::ParameterAttachment originalAttachment_, responseAttachment_;
    void sync();
};

class Toggle : public juce::Component {
public:
    Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent);
    void resized() override;
    void setTooltip(const juce::String& text);

private:
    juce::TextButton button_;
    Apvts::ButtonAttachment attachment_;
};

// Draws a control at kInactiveAlpha while its setting has no effect.
// Its tooltip then says what turns it on. The original tooltip comes back when it is active again.
void setInactive(juce::Component& control, bool isInactive, const juce::String& enableHint);

// Places components left to right in equal-width cells.
void layoutRow(juce::Rectangle<int> area, std::initializer_list<juce::Component*> components);

// Fills a rounded panel and draws its title in the accent colour.
void drawPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent);

// Draws a small dim caption, used to name groups of controls inside a panel.
void drawCaption(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text);

} // namespace openedm::ui
