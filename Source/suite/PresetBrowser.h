#pragma once

#include "PresetStore.h"
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>

namespace suite {

struct Colours {
    juce::Colour background { 0xff0d1317 };
    juce::Colour panel { 0xff152026 };
    juce::Colour edge { 0xff283842 };
    juce::Colour text { 0xffe6f0f5 };
    juce::Colour dim { 0xffa9bac1 };
    juce::Colour accent { 0xff48f0c8 };
};

// Factory and user presets, with search and favorites. Factory rows cannot be renamed or deleted.
class PresetBrowser : public juce::Component, private juce::ListBoxModel {
public:
    using Factory = std::vector<juce::String>;

    PresetBrowser(PresetStore& store, Factory factory, Colours colours);
    ~PresetBrowser() override;

    std::function<void(int programIndex)> onFactoryPreset;
    std::function<void(const juce::String& name, const juce::ValueTree& snapshot)> onUserPreset;
    std::function<void(const juce::String& name)> onSaved;
    std::function<juce::ValueTree()> captureState;

    void setSearchText(const juce::String& text);
    // Loads the row before or after the named preset in list order and wraps at the ends.
    void loadNeighbour(const juce::String& currentName, int direction);
    [[nodiscard]] juce::StringArray listedNames() const;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct Row {
        bool factory = false;
        bool favorite = false;
        juce::String name;
        int program = -1;
    };

    int getNumRows() override { return static_cast<int>(rows_.size()); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& event) override;
    void selectedRowsChanged(int lastRow) override;

    void rebuild();
    void loadRow(int row);
    void updateButtons();
    [[nodiscard]] const Row* selectedRow() const;

    PresetStore& store_;
    Factory factory_;
    Colours colours_;
    std::vector<Row> rows_;
    juce::TextEditor search_;
    juce::TextEditor name_;
    juce::TextButton save_{ "SAVE" };
    juce::TextButton rename_{ "RENAME" };
    juce::TextButton remove_{ "DELETE" };
    juce::TextButton favorite_{ "FAVORITE" };
    juce::ListBox list_{ "Presets", this };
};

} // namespace suite
