#pragma once

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

    using TreeSource = std::function<juce::ValueTree()>;
    using TreeSink = std::function<void(const juce::ValueTree&)>;
    using TreeWriter = std::function<void(juce::ValueTree&)>;

    // All hooks are optional. savePreset adds the loaded preset to each snapshot and loadPreset reads it
    // back, so undo and A/B bring back the preset name. markEdited runs before a gesture edit is stored.
    // Slot trees ride along so a copy or toggle undoes cleanly.
    void setHooks(TreeWriter savePreset, TreeSink loadPreset, std::function<void()> markEdited,
                  std::function<int()> getAbSlot, std::function<void(int)> setAbSlot,
                  TreeSource copySlots, TreeSink restoreSlots);

    [[nodiscard]] juce::UndoManager& manager() noexcept { return undo_; }
    [[nodiscard]] bool canUndo() const noexcept { return undo_.canUndo(); }
    [[nodiscard]] bool canRedo() const noexcept { return undo_.canRedo(); }
    void undo() { undo_.undo(); }
    void redo() { undo_.redo(); }
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
    [[nodiscard]] juce::ValueTree captureUndo() const;
    [[nodiscard]] juce::RangedAudioParameter* findParameter(const juce::String& id) const;

    juce::AudioProcessor& processor_;
    juce::UndoManager undo_;
    juce::ValueTree before_;
    int openGestures_ = 0;
    bool commitPending_ = false;
    bool applying_ = false;
    TreeWriter savePreset_;
    TreeSink loadPreset_;
    std::function<void()> markEdited_;
    std::function<int()> getAbSlot_;
    std::function<void(int)> setAbSlot_;
    TreeSource copySlots_;
    TreeSink restoreSlots_;
};

} // namespace suite
