#pragma once

#include <array>
#include <juce_data_structures/juce_data_structures.h>

namespace suite {

// Two full parameter snapshots. The active slot is the one the listener hears.
class ABCompare {
public:
    void reset(const juce::ValueTree& snapshot);
    [[nodiscard]] int active() const noexcept { return active_; }
    void setActive(int slot) noexcept;
    void remember(const juce::ValueTree& snapshot, int slot);
    [[nodiscard]] juce::ValueTree slot(int index) const;

    [[nodiscard]] juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& tree, const juce::ValueTree& fallback);

private:
    std::array<juce::ValueTree, 2> slots_;
    int active_ = 0;
};

} // namespace suite
