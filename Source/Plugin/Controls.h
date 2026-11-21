#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace openedm::ui {

using Apvts = juce::AudioProcessorValueTreeState;

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
    // A screen reader says spokenTitle, or the text when spokenTitle is empty.
    Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent,
           const juce::String& spokenTitle = {});
    void resized() override;
    void setTooltip(const juce::String& text);

private:
    juce::TextButton button_;
    Apvts::ButtonAttachment attachment_;
};

// A thin vertical dBFS peak meter with a 1 s peak hold. Its top turns red above 0 dBFS until a click.
class LevelMeter : public juce::Component, public juce::TooltipClient {
public:
    LevelMeter(std::atomic<float>& peak, const juce::String& name, int framesPerSecond, juce::Colour accent);
    // Takes the peak since the last call. Call it once per editor frame.
    void update();
    juce::String getTooltip() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    [[nodiscard]] float heldPeak() const noexcept { return held_; }
    [[nodiscard]] bool isClipped() const noexcept { return isClipped_; }

private:
    std::atomic<float>& peak_;
    juce::String name_;
    int holdFrames_;
    float fallPerFrame_;
    juce::Colour accent_;
    float level_ = 0.0f;
    float held_ = 0.0f;
    int framesLeft_ = 0;
    bool isClipped_ = false;
};

// Draws a control at theme::kInactiveAlpha while its setting has no effect.
// Its tooltip then says what turns it on. The original tooltip returns when the control is active.
void setInactive(juce::Component& control, bool isInactive, const juce::String& enableHint);

// Places components left to right in equal-width cells.
void layoutRow(juce::Rectangle<int> area, std::initializer_list<juce::Component*> components);

// Fills a rounded panel and draws its title in the accent colour.
void drawPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent);

// Draws a small dim caption, used to name groups of controls inside a panel.
void drawCaption(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text);

} // namespace openedm::ui
