#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace suite {

// Undo, redo, A/B and the preset browser button. The host editor places this in its header.
class SuiteBar : public juce::Component {
public:
    SuiteBar();

    std::function<void()> onUndo;
    std::function<void()> onRedo;
    std::function<void()> onShowA;
    std::function<void()> onShowB;
    std::function<void()> onCopyAToB;
    std::function<void()> onCopyBToA;
    std::function<void()> onPresets;

    void setHistoryEnabled(bool canUndo, bool canRedo);
    void setActiveSlot(int slot);
    [[nodiscard]] int preferredWidth() const noexcept { return 408; }

    void resized() override;

private:
    juce::TextButton undo_{ "UNDO" };
    juce::TextButton redo_{ "REDO" };
    juce::TextButton slotA_{ "A" };
    juce::TextButton slotB_{ "B" };
    juce::TextButton copyAToB_{ "A>B" };
    juce::TextButton copyBToA_{ "B>A" };
    juce::TextButton presets_{ "PRESETS" };
};

} // namespace suite
