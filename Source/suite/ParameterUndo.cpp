#include "ParameterUndo.h"

namespace suite {

namespace {

bool sameTree(const juce::ValueTree& a, const juce::ValueTree& b) {
    if (a.getNumProperties() != b.getNumProperties() || a.getNumChildren() != b.getNumChildren())
        return false;
    for (int i = 0; i < a.getNumProperties(); ++i) {
        const auto name = a.getPropertyName(i);
        if (a.getProperty(name) != b.getProperty(name))
            return false;
    }
    for (int i = 0; i < a.getNumChildren(); ++i)
        if (!sameTree(a.getChild(i), b.getChild(i)))
            return false;
    return true;
}

} // namespace

class ParameterUndo::SnapshotAction : public juce::UndoableAction {
public:
    SnapshotAction(ParameterUndo& owner, juce::ValueTree before, juce::ValueTree after)
        : owner_(owner), before_(std::move(before)), after_(std::move(after)) {}

    bool perform() override {
        owner_.restore(after_);
        return true;
    }

    bool undo() override {
        owner_.restore(before_);
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    ParameterUndo& owner_;
    juce::ValueTree before_;
    juce::ValueTree after_;
};

ParameterUndo::ParameterUndo(juce::AudioProcessor& processor)
    : processor_(processor) {
    for (auto* parameter : processor_.getParameters())
        parameter->addListener(this);
}

ParameterUndo::~ParameterUndo() {
    cancelPendingUpdate();
    for (auto* parameter : processor_.getParameters())
        parameter->removeListener(this);
    undo_.clearUndoHistory();
}

void ParameterUndo::setHooks(std::function<int()> getProgram, std::function<void(int)> setProgram,
                             std::function<int()> getAbSlot, std::function<void(int)> setAbSlot,
                             TreeSource copySlots, TreeSink restoreSlots) {
    getProgram_ = std::move(getProgram);
    setProgram_ = std::move(setProgram);
    getAbSlot_ = std::move(getAbSlot);
    setAbSlot_ = std::move(setAbSlot);
    copySlots_ = std::move(copySlots);
    restoreSlots_ = std::move(restoreSlots);
}

void ParameterUndo::clear() {
    cancelPendingUpdate();
    commitPending_ = false;
    openGestures_ = 0;
    undo_.clearUndoHistory();
}

juce::RangedAudioParameter* ParameterUndo::findParameter(const juce::String& id) const {
    for (auto* parameter : processor_.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter); ranged != nullptr && ranged->paramID == id)
            return ranged;
    return nullptr;
}

juce::ValueTree ParameterUndo::captureParameters() const {
    juce::ValueTree tree("Snapshot");
    if (getProgram_)
        tree.setProperty("program", getProgram_(), nullptr);
    for (auto* parameter : processor_.getParameters()) {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter)) {
            juce::ValueTree child("PARAM");
            child.setProperty("id", ranged->paramID, nullptr);
            child.setProperty("value", ranged->convertFrom0to1(ranged->getValue()), nullptr);
            tree.appendChild(child, nullptr);
        }
    }
    return tree;
}

juce::ValueTree ParameterUndo::captureUndo() const {
    auto tree = captureParameters();
    if (getAbSlot_)
        tree.setProperty("abSlot", getAbSlot_(), nullptr);
    if (copySlots_)
        tree.appendChild(copySlots_(), nullptr);
    return tree;
}

void ParameterUndo::restore(const juce::ValueTree& snapshot) {
    const juce::ScopedValueSetter<bool> guard(applying_, true);
    for (int i = 0; i < snapshot.getNumChildren(); ++i) {
        const auto child = snapshot.getChild(i);
        if (!child.hasType("PARAM"))
            continue;
        auto* ranged = findParameter(child.getProperty("id").toString());
        if (ranged == nullptr)
            continue;
        const float value = static_cast<float>(static_cast<double>(child.getProperty("value")));
        if (!juce::approximatelyEqual(ranged->convertFrom0to1(ranged->getValue()), value))
            ranged->setValueNotifyingHost(ranged->convertTo0to1(value));
    }
    if (snapshot.hasProperty("program") && setProgram_)
        setProgram_(static_cast<int>(snapshot.getProperty("program")));
    if (snapshot.hasProperty("abSlot") && setAbSlot_)
        setAbSlot_(juce::jlimit(0, 1, static_cast<int>(snapshot.getProperty("abSlot"))));
    if (restoreSlots_)
        if (auto slots = snapshot.getChildWithName("AB"); slots.isValid())
            restoreSlots_(slots);
}

void ParameterUndo::performEdit(const std::function<void()>& edit, const juce::String& name) {
    if (applying_) {
        edit();
        return;
    }
    const auto before = captureUndo();
    {
        const juce::ScopedValueSetter<bool> guard(applying_, true);
        edit();
    }
    const auto after = captureUndo();
    if (!sameTree(before, after)) {
        undo_.beginNewTransaction(name);
        undo_.perform(new SnapshotAction(*this, before, after), name);
    }
}

void ParameterUndo::parameterGestureChanged(int, bool starting) {
    if (applying_)
        return;
    if (starting) {
        if (openGestures_ == 0 && !commitPending_)
            before_ = captureUndo();
        ++openGestures_;
        cancelPendingUpdate();
        commitPending_ = false;
        return;
    }
    if (openGestures_ == 0)
        return;
    if (--openGestures_ == 0)
        scheduleCommit();
}

void ParameterUndo::scheduleCommit() {
    commitPending_ = true;
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        triggerAsyncUpdate();
    else
        handleAsyncUpdate();
}

void ParameterUndo::handleAsyncUpdate() {
    if (!commitPending_ || openGestures_ != 0 || applying_)
        return;
    commitPending_ = false;
    const auto after = captureUndo();
    if (!sameTree(before_, after)) {
        undo_.beginNewTransaction("Edit");
        undo_.perform(new SnapshotAction(*this, before_, after), "Edit");
    }
}

void ParameterUndo::flush() {
    if (commitPending_)
        handleAsyncUpdate();
}

} // namespace suite
