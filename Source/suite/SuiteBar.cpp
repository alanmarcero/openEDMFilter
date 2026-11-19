#include "SuiteBar.h"

namespace suite {

SuiteBar::SuiteBar() {
    const auto add = [this](juce::TextButton& button, const juce::String& id, const juce::String& tip, std::function<void()>* callback) {
        button.setComponentID(id);
        button.setTooltip(tip);
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff48f0c8));
        button.onClick = [callback] { if (callback != nullptr && *callback) (*callback)(); };
        addAndMakeVisible(button);
    };
    add(undo_, "undoButton", "Undo the last edit  |  Ctrl/Cmd+Z", &onUndo);
    add(redo_, "redoButton", "Redo the last edit  |  Shift+Ctrl/Cmd+Z", &onRedo);
    add(slotA_, "abA", "Hear slot A", &onShowA);
    add(slotB_, "abB", "Hear slot B", &onShowB);
    add(copyAToB_, "copyAToB", "Copy slot A onto slot B", &onCopyAToB);
    add(copyBToA_, "copyBToA", "Copy slot B onto slot A", &onCopyBToA);
    add(presets_, "presetBrowserButton", "Factory and user presets", &onPresets);
    slotA_.setClickingTogglesState(false);
    slotB_.setClickingTogglesState(false);
    setActiveSlot(0);
}

void SuiteBar::setHistoryEnabled(bool canUndo, bool canRedo) {
    undo_.setEnabled(canUndo);
    redo_.setEnabled(canRedo);
}

void SuiteBar::setActiveSlot(int slot) {
    slotA_.setToggleState(slot == 0, juce::dontSendNotification);
    slotB_.setToggleState(slot == 1, juce::dontSendNotification);
}

void SuiteBar::resized() {
    const int preferred[] = { 54, 54, 44, 44, 50, 50, 88 };
    juce::Component* controls[] = { &undo_, &redo_, &slotA_, &slotB_, &copyAToB_, &copyBToA_, &presets_ };
    constexpr int count = 7;
    constexpr int gap = 4;
    int used[count];
    int sum = 0;
    for (int width : preferred)
        sum += width;
    const int gaps = gap * (count - 1);
    if (sum + gaps <= getWidth()) {
        for (int i = 0; i < count; ++i)
            used[i] = preferred[i];
    } else {
        const int each = juce::jmax(1, (getWidth() - gaps) / count);
        for (int i = 0; i < count; ++i)
            used[i] = each;
    }
    auto row = getLocalBounds().withHeight(juce::jmin(36, getHeight()));
    for (int i = 0; i < count; ++i) {
        auto cell = row.removeFromLeft(juce::jmin(used[i], row.getWidth()));
        controls[i]->setBounds(cell);
        if (row.getWidth() > 0)
            row.removeFromLeft(juce::jmin(gap, row.getWidth()));
    }
}

} // namespace suite
