#pragma once

#include <array>
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>

namespace suite {

// One UndoManager step per gesture. Drags, and several complete edits in the same
// UI event, share a step. Host automation never opens a gesture, so it is not recorded.
class ParameterUndo : private juce::AudioProcessorParameter::Listener,
                      private juce::AsyncUpdater {
public:
    explicit ParameterUndo(juce::AudioProcessor& processor);
    ~ParameterUndo() override;

    using TreeSink = std::function<void(const juce::ValueTree&)>;
    using TreeWriter = std::function<void(juce::ValueTree&)>;

    // All hooks are optional. savePreset adds the loaded preset to each snapshot and loadPreset reads it
    // back, so undo and A/B bring back the preset name. markEdited runs before a gesture edit is stored.
    void setHooks(TreeWriter savePreset, TreeSink loadPreset, std::function<void()> markEdited);

    // Each A/B slot has its own history, so an undo never applies a state from the other slot.
    void selectHistory(int slot) noexcept { activeHistory_ = static_cast<size_t>(juce::jlimit(0, 1, slot)); }
    void clearHistory(int slot) { histories_[static_cast<size_t>(juce::jlimit(0, 1, slot))].clearUndoHistory(); }
    [[nodiscard]] juce::UndoManager& manager() noexcept { return histories_[activeHistory_]; }
    [[nodiscard]] bool canUndo() const noexcept { return histories_[activeHistory_].canUndo(); }
    [[nodiscard]] bool canRedo() const noexcept { return histories_[activeHistory_].canRedo(); }
    void undo() { histories_[activeHistory_].undo(); }
    void redo() { histories_[activeHistory_].redo(); }
    void clear();

    // Runs the edit, then stores one undo step when the snapshot changed.
    void performEdit(const std::function<void()>& edit, const juce::String& name);

    [[nodiscard]] juce::ValueTree captureParameters() const;
    void restore(const juce::ValueTree& snapshot);

    // Gesture commits are posted to the message thread so a single click can group them.
    // Tests call this instead of spinning the message loop.
    void flush();

private:
    class SnapshotAction;

    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override;
    void handleAsyncUpdate() override;
    void scheduleCommit();
    [[nodiscard]] juce::RangedAudioParameter* findParameter(const juce::String& id) const;

    juce::AudioProcessor& processor_;
    std::array<juce::UndoManager, 2> histories_;
    size_t activeHistory_ = 0;
    juce::ValueTree before_;
    int openGestures_ = 0;
    bool commitPending_ = false;
    bool applying_ = false;
    TreeWriter savePreset_;
    TreeSink loadPreset_;
    std::function<void()> markEdited_;
};

} // namespace suite
