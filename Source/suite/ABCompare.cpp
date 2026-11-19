#include "ABCompare.h"

namespace suite {

void ABCompare::reset(const juce::ValueTree& snapshot) {
    slots_[0] = snapshot.createCopy();
    slots_[1] = snapshot.createCopy();
    active_ = 0;
}

void ABCompare::setActive(int slot) noexcept {
    active_ = juce::jlimit(0, 1, slot);
}

void ABCompare::remember(const juce::ValueTree& snapshot, int slot) {
    slots_[static_cast<size_t>(juce::jlimit(0, 1, slot))] = snapshot.createCopy();
}

juce::ValueTree ABCompare::slot(int index) const {
    return slots_[static_cast<size_t>(juce::jlimit(0, 1, index))].createCopy();
}

juce::ValueTree ABCompare::toValueTree() const {
    juce::ValueTree tree("AB");
    tree.setProperty("active", active_, nullptr);
    for (int i = 0; i < 2; ++i) {
        auto child = slots_[static_cast<size_t>(i)].createCopy();
        if (!child.isValid())
            child = juce::ValueTree("Snapshot");
        child.setProperty("slot", i, nullptr);
        tree.appendChild(child, nullptr);
    }
    return tree;
}

void ABCompare::fromValueTree(const juce::ValueTree& tree, const juce::ValueTree& fallback) {
    if (!tree.hasType("AB") || tree.getNumChildren() < 2) {
        reset(fallback);
        return;
    }
    slots_[0] = tree.getChild(0).createCopy();
    slots_[1] = tree.getChild(1).createCopy();
    active_ = juce::jlimit(0, 1, static_cast<int>(tree.getProperty("active", 0)));
}

} // namespace suite
