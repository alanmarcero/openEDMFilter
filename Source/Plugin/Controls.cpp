#include "Controls.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "Theme.h"
#include <algorithm>
#include <tuple>

namespace openedm::ui {

namespace {

constexpr int kTitleHeight = 15;
constexpr int kValueHeight = 15;
constexpr int kControlHeight = 36;
constexpr int kCellGap = 4;
constexpr float kTitleFontSize = 11.0f;

} // namespace

Knob::Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent)
    : state_(state),
      accent_(accent),
      slider_(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      title_({}, title.toUpperCase()),
      attachment_(std::make_unique<Apvts::SliderAttachment>(state, parameterId, slider_)) {
    setComponentID(parameterId);
    slider_.setTitle(title);
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
    // A thin border leaves room for titles such as SUB KEEP on narrow knobs.
    title_.setBorderSize({ 1, 1, 1, 1 });
    addAndMakeVisible(slider_);
    addAndMakeVisible(title_);
    addMouseListener(this, true);
}

Knob::~Knob() {
    removeMouseListener(this);
}

void Knob::paintOverChildren(juce::Graphics& g) {
    if (isParamLocked(state_, getComponentID()))
        drawPadlockBadge(g, { static_cast<float>(getWidth()) - 13.0f,
                              static_cast<float>(slider_.getY()) + 1.0f, 11.0f, 12.0f }, accent_);
}

void Knob::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu())
        showParamLockMenu(*this, state_, { getComponentID() }, slider_.getTitle());
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

void Choice::LockableComboBox::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu())
        return;
    juce::ComboBox::mouseDown(event);
}

Choice::Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent)
    : state_(state),
      accent_(accent),
      title_({}, title.toUpperCase()) {
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId))) {
        box_.addItemList(parameter->choices, 1);
    }
    attachment_ = std::make_unique<Apvts::ComboBoxAttachment>(state, parameterId, box_);
    setComponentID(parameterId);
    box_.setTitle(title.isNotEmpty() ? title : state.getParameter(parameterId)->getName(64));
    box_.setColour(juce::ComboBox::outlineColourId, accent);
    title_.setFont(juce::FontOptions(kTitleFontSize, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, theme::kTextDim);
    title_.setJustificationType(juce::Justification::centred);
    title_.setBorderSize({ 1, 1, 1, 1 });
    addAndMakeVisible(box_);
    addChildComponent(title_);
    title_.setVisible(title.isNotEmpty());
    addMouseListener(this, true);
}

Choice::~Choice() {
    removeMouseListener(this);
}

void Choice::paintOverChildren(juce::Graphics& g) {
    if (isParamLocked(state_, getComponentID()))
        drawPadlockBadge(g, { static_cast<float>(box_.getRight()) - 30.0f,
                              static_cast<float>(box_.getY()) + 2.0f, 11.0f, 12.0f }, accent_);
}

void Choice::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu())
        showParamLockMenu(*this, state_, { getComponentID() }, box_.getTitle());
}

void Choice::bind(Apvts& state, const juce::String& parameterId) {
    attachment_.reset();
    setComponentID(parameterId);
    // A box with no visible title uses the parameter name as its screen reader title.
    if (!title_.isVisible())
        box_.setTitle(state.getParameter(parameterId)->getName(64));
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
    : state_(state),
      band_(band),
      accent_(accent),
      original_(static_cast<juce::AudioParameterChoice&>(*state.getParameter(params::bandId(band, params::kType)))),
      response_(static_cast<juce::AudioParameterChoice&>(*state.getParameter(params::bandId(band, params::kExtendedType)))),
      originalAttachment_(original_, [this](float) { sync(); }, nullptr),
      responseAttachment_(response_, [this](float) { sync(); }, nullptr) {
    addItemList(original_.choices, 1);
    for (int i = 1; i < response_.choices.size(); ++i)
        addItem(response_.choices[i], original_.choices.size() + i);
    setColour(juce::ComboBox::outlineColourId, accent);
    setComponentID(params::bandId(band, params::kType));
    setName(band == 0 ? "Filter A type" : "Filter B type");
    setTitle(getName());
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

void FilterTypeMenu::paintOverChildren(juce::Graphics& g) {
    if (isParamLocked(state_, params::bandId(band_, params::kType))
        || isParamLocked(state_, params::bandId(band_, params::kExtendedType))) {
        drawPadlockBadge(g, { static_cast<float>(getWidth()) - 30.0f, 2.0f, 11.0f, 12.0f }, accent_);
    }
}

void FilterTypeMenu::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu()) {
        showParamLockMenu(*this, state_,
                          { params::bandId(band_, params::kType), params::bandId(band_, params::kExtendedType) },
                          getName());
        return;
    }
    juce::ComboBox::mouseDown(event);
}

void FilterTypeMenu::sync() {
    setSelectedId(params::effectiveType(original_.getIndex(), response_.getIndex()) + 1, juce::dontSendNotification);
}

Toggle::Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent,
               const juce::String& spokenTitle)
    : state_(state),
      accent_(accent),
      button_(text),
      attachment_(state, parameterId, button_) {
    setComponentID(parameterId);
    button_.setTitle(spokenTitle.isNotEmpty() ? spokenTitle : text);
    button_.setClickingTogglesState(true);
    button_.setColour(juce::TextButton::buttonOnColourId, accent);
    addAndMakeVisible(button_);
    addMouseListener(this, true);
}

Toggle::~Toggle() {
    removeMouseListener(this);
}

void Toggle::paintOverChildren(juce::Graphics& g) {
    if (isParamLocked(state_, getComponentID()))
        drawPadlockBadge(g, { static_cast<float>(button_.getRight()) - 11.0f,
                              static_cast<float>(button_.getY()) + 2.0f, 9.0f, 10.0f }, accent_);
}

void Toggle::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu())
        showParamLockMenu(*this, state_, { getComponentID() }, button_.getTitle());
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
    // The first part holds the tooltip to restore. A slider can rebuild its value box, so every part gets set each time.
    auto& properties = control.getProperties();
    const auto apply = [&](auto&& self, juce::Component& part) -> void {
        if (auto* client = dynamic_cast<juce::SettableTooltipClient*>(&part)) {
            if (!properties.contains("activeTooltip"))
                properties.set("activeTooltip", client->getTooltip());
            const juce::String activeTooltip = properties["activeTooltip"];
            client->setTooltip(isInactive ? enableHint : activeTooltip);
        }
        std::ranges::for_each(part.getChildren(), [&](juce::Component* child) { self(self, *child); });
    };
    apply(apply, control);
}

LevelMeter::LevelMeter(std::atomic<float>& peak, const juce::String& name, int framesPerSecond, juce::Colour accent)
    : peak_(peak),
      name_(name),
      holdFrames_(framesPerSecond),
      fallPerFrame_(std::pow(10.0f, -1.0f / static_cast<float>(framesPerSecond))),
      accent_(accent) {
    setComponentID(name.toLowerCase() + "Meter");
    setTitle(name + " meter");
}

void LevelMeter::update() {
    const float peak = peak_.exchange(0.0f);
    const auto before = std::tuple(level_, held_, isClipped_);
    // The bar falls 20 dB per second and empties below -60 dBFS. The hold mark stays for 1 s, then follows the bar.
    const float fallen = level_ * fallPerFrame_;
    level_ = std::max(peak, fallen < 0.001f ? 0.0f : fallen);
    if (peak >= held_) {
        held_ = peak;
        framesLeft_ = holdFrames_;
    }
    if (peak < held_ && --framesLeft_ <= 0)
        held_ = level_;
    isClipped_ = isClipped_ || peak > 1.0f;
    if (std::tuple(level_, held_, isClipped_) != before)
        repaint();
}

juce::String LevelMeter::getTooltip() {
    const juce::String heldDb(juce::Decibels::gainToDecibels(held_), 1);
    const auto heldText = held_ > 0.0f ? heldDb + " dBFS" : juce::String("-INF dBFS");
    return name_ + " peak " + heldText + ". A red top shows a peak above 0 dBFS. Click to clear it.";
}

void LevelMeter::paint(juce::Graphics& g) {
    auto bar = getLocalBounds().toFloat();
    const auto lamp = bar.removeFromTop(bar.getWidth());
    bar.removeFromTop(2.0f);
    g.setColour(isClipped_ ? juce::Colour(0xffff665e) : theme::kTrack);
    g.fillRoundedRectangle(lamp, 1.5f);
    g.setColour(theme::kTrack);
    g.fillRoundedRectangle(bar, 2.0f);
    const auto heightFor = [&](float gain) {
        const float db = juce::Decibels::gainToDecibels(gain, -60.0f);
        return bar.getHeight() * juce::jlimit(0.0f, 1.0f, (db + 48.0f) / 48.0f);
    };
    g.setColour(accent_);
    g.fillRoundedRectangle(bar.withTop(bar.getBottom() - heightFor(level_)), 2.0f);
    g.setColour(theme::kText);
    g.fillRect(bar.withTop(bar.getBottom() - heightFor(held_) - 1.0f).withHeight(2.0f));
}

void LevelMeter::mouseDown(const juce::MouseEvent&) {
    isClipped_ = false;
    repaint();
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

bool isParamLocked(Apvts& state, const juce::String& paramId) {
    if (auto* processor = dynamic_cast<OpenEdmFilterProcessor*>(&state.processor))
        return processor->isParamLocked(paramId);
    return false;
}

void drawPadlockBadge(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent) {
    const auto pill = bounds.expanded(1.0f);
    g.setColour(theme::kBackground.withAlpha(0.92f));
    g.fillRoundedRectangle(pill, 2.5f);
    g.setColour(accent.withAlpha(0.6f));
    g.drawRoundedRectangle(pill, 2.5f, 1.0f);

    const float bodyW = bounds.getWidth() * 0.72f;
    const float bodyH = bounds.getHeight() * 0.48f;
    const float bodyX = bounds.getCentreX() - bodyW * 0.5f;
    const float bodyY = bounds.getBottom() - bodyH - 1.0f;
    const float shackleW = bodyW * 0.62f;
    const float shackleH = bounds.getHeight() * 0.40f;
    const float shackleX = bounds.getCentreX() - shackleW * 0.5f;
    const float shackleY = bodyY - shackleH * 0.72f;

    juce::Path shackle;
    shackle.addArc(shackleX, shackleY, shackleW, shackleH, -juce::MathConstants<float>::halfPi,
                   juce::MathConstants<float>::halfPi, true);
    g.setColour(accent);
    g.strokePath(shackle, juce::PathStrokeType(1.4f));
    g.fillRoundedRectangle(bodyX, bodyY, bodyW, bodyH, 1.5f);
}

void showParamLockMenu(juce::Component& anchor, Apvts& state, std::vector<juce::String> paramIds,
                       const juce::String& displayName,
                       std::vector<juce::String> groupParamIds,
                       const juce::String& groupDisplayName) {
    auto* processor = dynamic_cast<OpenEdmFilterProcessor*>(&state.processor);
    if (processor == nullptr || paramIds.empty())
        return;

    const bool locked = std::ranges::any_of(paramIds, [&](const auto& id) { return processor->isParamLocked(id); });
    const bool groupLocked = !groupParamIds.empty()
        && std::ranges::all_of(groupParamIds, [&](const auto& id) { return processor->isParamLocked(id); });
    juce::String label = displayName;
    if (label.isEmpty()) {
        if (auto* param = state.getParameter(paramIds.front()))
            label = param->getName(64);
        else
            label = paramIds.front();
    }

    juce::PopupMenu menu;
    menu.addItem(1, (locked ? "Unlock " : "Lock ") + label);
    if (!groupParamIds.empty())
        menu.addItem(3, (groupLocked ? "Unlock all " : "Lock all ") + groupDisplayName);
    if (processor->hasAnyLockedParams()) {
        menu.addSeparator();
        menu.addItem(2, "Unlock all parameters");
    }

    const juce::Component::SafePointer<juce::Component> safeAnchor(&anchor);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&anchor),
                       [processor, ids = std::move(paramIds), locked,
                        groupIds = std::move(groupParamIds), groupLocked, safeAnchor](int result) {
        if (result == 1) {
            for (const auto& id : ids)
                processor->setParamLocked(id, !locked);
        } else if (result == 3) {
            for (const auto& id : groupIds)
                processor->setParamLocked(id, !groupLocked);
        } else if (result == 2) {
            processor->unlockAllParams();
        } else {
            return;
        }
        if (safeAnchor != nullptr) {
            if (auto* top = safeAnchor->getTopLevelComponent())
                top->repaint();
            else
                safeAnchor->repaint();
        }
    });
}

} // namespace openedm::ui
