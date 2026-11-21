#include "Controls.h"
#include "Theme.h"
#include "Parameters.h"

namespace openedm::ui {

namespace {

constexpr int kTitleHeight = 15;
constexpr int kValueHeight = 15;
constexpr int kControlHeight = 36;
constexpr int kCellGap = 4;
constexpr float kTitleFontSize = 11.0f;

} // namespace

Knob::Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent)
    : slider_(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      title_({}, title.toUpperCase()),
      attachment_(std::make_unique<Apvts::SliderAttachment>(state, parameterId, slider_)) {
    setComponentID(parameterId);
    slider_.onDragStart = [this] { dragging_ = true; };
    slider_.onDragEnd = [this] {
        dragging_ = false;
        if (pendingState_ != nullptr) {
            auto* pendingState = std::exchange(pendingState_, nullptr);
            bind(*pendingState, std::exchange(pendingId_, {}));
        }
    };
    slider_.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, kValueHeight);
    slider_.setPopupDisplayEnabled(false, false, nullptr);
    title_.setFont(juce::FontOptions(kTitleFontSize, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, theme::kTextDim);
    title_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(slider_);
    addAndMakeVisible(title_);
}

void Knob::bind(Apvts& state, const juce::String& parameterId) {
    if (dragging_) {
        pendingState_ = &state;
        pendingId_ = parameterId;
        return;
    }
    attachment_.reset();
    setComponentID(parameterId);
    if (auto* param = state.getParameter(parameterId)) {
        slider_.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
    }
    attachment_ = std::make_unique<Apvts::SliderAttachment>(state, parameterId, slider_);
}

void Knob::setTooltip(const juce::String& text) {
    slider_.setTooltip(text);
    title_.setTooltip(text);
    // The slider copies its tooltip to the value box only when it makes the box.
    std::ranges::for_each(slider_.getChildren(), [&](juce::Component* part) {
        if (auto* valueBox = dynamic_cast<juce::Label*>(part))
            valueBox->setTooltip(text);
    });
}

void Knob::resized() {
    auto bounds = getLocalBounds();
    title_.setBounds(bounds.removeFromTop(kTitleHeight));
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, bounds.getWidth(), kValueHeight);
    slider_.setBounds(bounds);
}

Choice::Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent)
    : title_({}, title.toUpperCase()) {
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId))) {
        box_.addItemList(parameter->choices, 1);
    }
    attachment_ = std::make_unique<Apvts::ComboBoxAttachment>(state, parameterId, box_);
    setComponentID(parameterId);
    box_.setColour(juce::ComboBox::outlineColourId, accent);
    title_.setFont(juce::FontOptions(kTitleFontSize, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, theme::kTextDim);
    title_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(box_);
    addChildComponent(title_);
    title_.setVisible(title.isNotEmpty());
}

void Choice::bind(Apvts& state, const juce::String& parameterId) {
    attachment_.reset();
    setComponentID(parameterId);
    attachment_ = std::make_unique<Apvts::ComboBoxAttachment>(state, parameterId, box_);
}

void Choice::setTooltip(const juce::String& text) {
    box_.setTooltip(text);
}

void Choice::resized() {
    auto bounds = getLocalBounds();
    if (title_.isVisible()) {
        title_.setBounds(bounds.removeFromTop(kTitleHeight));
    }
    box_.setBounds(bounds.withSizeKeepingCentre(bounds.getWidth(), juce::jmin(kControlHeight, bounds.getHeight())));
}

FilterTypeMenu::FilterTypeMenu(Apvts& state, int band, juce::Colour accent)
    : original_(static_cast<juce::AudioParameterChoice&>(*state.getParameter(params::bandId(band, params::kType)))),
      response_(static_cast<juce::AudioParameterChoice&>(*state.getParameter(params::bandId(band, params::kExtendedType)))),
      originalAttachment_(original_, [this](float) { sync(); }, nullptr),
      responseAttachment_(response_, [this](float) { sync(); }, nullptr) {
    addItemList(original_.choices, 1);
    for (int i = 1; i < response_.choices.size(); ++i)
        addItem(response_.choices[i], original_.choices.size() + i);
    setColour(juce::ComboBox::outlineColourId, accent);
    setComponentID(params::bandId(band, params::kType));
    setName(band == 0 ? "Filter A type" : "Filter B type");
    onChange = [this] {
        const int index = getSelectedId() - 1;
        if (index < 0) return;
        if (index < original_.choices.size()) {
            originalAttachment_.setValueAsCompleteGesture(static_cast<float>(index));
            responseAttachment_.setValueAsCompleteGesture(0);
        } else {
            responseAttachment_.setValueAsCompleteGesture(static_cast<float>(index - original_.choices.size() + 1));
        }
        sync();
    };
    sync();
}

void FilterTypeMenu::sync() {
    setSelectedId(params::effectiveType(original_.getIndex(), response_.getIndex()) + 1, juce::dontSendNotification);
}

Toggle::Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent)
    : button_(text),
      attachment_(state, parameterId, button_) {
    setComponentID(parameterId);
    button_.setClickingTogglesState(true);
    button_.setColour(juce::TextButton::buttonOnColourId, accent);
    addAndMakeVisible(button_);
}

void Toggle::resized() {
    const auto bounds = getLocalBounds();
    button_.setBounds(bounds.withSizeKeepingCentre(bounds.getWidth(), juce::jmin(kControlHeight, bounds.getHeight())));
}

void Toggle::setTooltip(const juce::String& text) {
    button_.setTooltip(text);
}

void setInactive(juce::Component& control, bool isInactive, const juce::String& enableHint) {
    control.setAlpha(isInactive ? theme::kInactiveAlpha : 1.0f);
    std::vector<juce::SettableTooltipClient*> parts;
    const auto collect = [&](auto&& self, juce::Component& part) -> void {
        if (auto* client = dynamic_cast<juce::SettableTooltipClient*>(&part))
            parts.push_back(client);
        std::ranges::for_each(part.getChildren(), [&](juce::Component* child) { self(self, *child); });
    };
    collect(collect, control);
    if (parts.empty())
        return;

    // A slider can rebuild its value box, so every part gets the tooltip each time.
    auto& properties = control.getProperties();
    if (!properties.contains("activeTooltip"))
        properties.set("activeTooltip", parts.front()->getTooltip());
    const juce::String activeTooltip = properties["activeTooltip"];
    const auto& tooltip = isInactive ? enableHint : activeTooltip;
    std::ranges::for_each(parts, [&](juce::SettableTooltipClient* part) { part->setTooltip(tooltip); });
}

void layoutRow(juce::Rectangle<int> area, std::initializer_list<juce::Component*> components) {
    const int count = static_cast<int>(components.size());
    const int cellWidth = (area.getWidth() - kCellGap * (count - 1)) / count;
    std::ranges::for_each(components, [&](juce::Component* component) {
        component->setBounds(area.removeFromLeft(cellWidth));
        area.removeFromLeft(kCellGap);
    });
}

void drawPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent) {
    const auto bounds = area.toFloat();
    g.setGradientFill(juce::ColourGradient(theme::kPanel.brighter(0.06f), bounds.getTopLeft(), theme::kPanel.darker(0.16f), bounds.getBottomLeft(), false));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);
    g.setColour(accent);
    g.fillRoundedRectangle(bounds.getX() + 12.0f, bounds.getY() + 11.0f, 3.0f, 14.0f, 1.5f);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(title.toUpperCase(), area.getX() + 22, area.getY() + 8, std::max(80, area.getWidth() - 32), 20, juce::Justification::centredLeft);
}

void drawCaption(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text) {
    g.setColour(theme::kTextDim.withAlpha(0.7f));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText(text.toUpperCase(), area.removeFromTop(12), juce::Justification::centredLeft);
}

} // namespace openedm::ui
