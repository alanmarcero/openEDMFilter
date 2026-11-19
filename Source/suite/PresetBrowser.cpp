#include "PresetBrowser.h"

namespace suite {

namespace {
constexpr int kRowHeight = 24;
constexpr int kStarWidth = 28;
} // namespace

PresetBrowser::PresetBrowser(PresetStore& store, Factory factory, Colours colours)
    : store_(store), factory_(std::move(factory)), colours_(colours) {
    search_.setComponentID("presetSearch");
    search_.setTextToShowWhenEmpty("Search presets", colours_.dim);
    search_.setColour(juce::TextEditor::backgroundColourId, colours_.panel);
    search_.setColour(juce::TextEditor::textColourId, colours_.text);
    search_.setColour(juce::TextEditor::outlineColourId, colours_.edge);
    search_.onTextChange = [this] { rebuild(); };

    name_.setComponentID("presetName");
    name_.setTextToShowWhenEmpty("Preset name", colours_.dim);
    name_.setColour(juce::TextEditor::backgroundColourId, colours_.panel);
    name_.setColour(juce::TextEditor::textColourId, colours_.text);
    name_.setColour(juce::TextEditor::outlineColourId, colours_.edge);

    const auto styleButton = [this](juce::TextButton& button, const juce::String& id) {
        button.setComponentID(id);
        button.setColour(juce::TextButton::buttonOnColourId, colours_.accent);
        addAndMakeVisible(button);
    };
    styleButton(save_, "presetSave");
    styleButton(rename_, "presetRename");
    styleButton(remove_, "presetDelete");
    styleButton(favorite_, "presetFavorite");
    save_.onClick = [this] {
        const auto presetName = name_.getText().trim();
        if (presetName.isEmpty() || captureState == nullptr)
            return;
        if (store_.save(presetName, captureState())) {
            rebuild();
            for (int i = 0; i < static_cast<int>(rows_.size()); ++i)
                if (!rows_[static_cast<size_t>(i)].factory && rows_[static_cast<size_t>(i)].name.equalsIgnoreCase(presetName))
                    list_.selectRow(i);
        }
    };
    rename_.onClick = [this] {
        const auto* row = selectedRow();
        if (row == nullptr || row->factory)
            return;
        const auto presetName = name_.getText().trim();
        if (presetName.isEmpty())
            return;
        if (store_.rename(row->name, presetName))
            rebuild();
    };
    remove_.onClick = [this] {
        const auto* row = selectedRow();
        if (row == nullptr || row->factory)
            return;
        if (store_.remove(row->name)) {
            name_.clear();
            rebuild();
        }
    };
    favorite_.onClick = [this] {
        const auto* row = selectedRow();
        if (row == nullptr)
            return;
        store_.setFavorite(row->factory, row->name, !row->favorite);
        rebuild();
    };

    list_.setComponentID("presetList");
    list_.setRowHeight(kRowHeight);
    list_.setColour(juce::ListBox::backgroundColourId, colours_.background);
    list_.setColour(juce::ListBox::outlineColourId, colours_.edge);
    addAndMakeVisible(search_);
    addAndMakeVisible(name_);
    addAndMakeVisible(list_);
    rebuild();
}

PresetBrowser::~PresetBrowser() {
    list_.setModel(nullptr);
}

void PresetBrowser::setSearchText(const juce::String& text) {
    search_.setText(text, juce::dontSendNotification);
    rebuild();
}

juce::StringArray PresetBrowser::listedNames() const {
    juce::StringArray names;
    for (const auto& row : rows_)
        names.add(row.name);
    return names;
}

void PresetBrowser::paint(juce::Graphics& g) {
    g.setColour(colours_.panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(colours_.edge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
    g.setColour(colours_.accent);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("PRESETS", 16, 6, getWidth() - 24, 18, juce::Justification::centredLeft);
}

void PresetBrowser::resized() {
    auto bounds = getLocalBounds().reduced(8).withTrimmedTop(24);
    search_.setBounds(bounds.removeFromTop(28));
    bounds.removeFromTop(4);
    auto buttons = bounds.removeFromBottom(36);
    favorite_.setBounds(buttons.removeFromRight(96));
    buttons.removeFromRight(4);
    const int buttonWidth = juce::jmax(44, (buttons.getWidth() - 8) / 3);
    save_.setBounds(buttons.removeFromLeft(buttonWidth));
    buttons.removeFromLeft(4);
    rename_.setBounds(buttons.removeFromLeft(buttonWidth));
    buttons.removeFromLeft(4);
    remove_.setBounds(buttons);
    bounds.removeFromBottom(4);
    name_.setBounds(bounds.removeFromBottom(28));
    bounds.removeFromBottom(4);
    list_.setBounds(bounds);
}

void PresetBrowser::rebuild() {
    const auto query = search_.getText().trim();
    const auto matches = [&](const juce::String& presetName) {
        return query.isEmpty() || presetName.containsIgnoreCase(query);
    };
    const int previous = list_.getSelectedRow();
    juce::String previousName;
    if (previous >= 0 && previous < static_cast<int>(rows_.size()))
        previousName = rows_[static_cast<size_t>(previous)].name;

    rows_.clear();
    std::vector<Row> factoryRows;
    std::vector<Row> userRows;
    for (int i = 0; i < static_cast<int>(factory_.size()); ++i) {
        if (!matches(factory_[static_cast<size_t>(i)]))
            continue;
        factoryRows.push_back({ true, store_.isFavorite(true, factory_[static_cast<size_t>(i)]), factory_[static_cast<size_t>(i)], i });
    }
    for (const auto& entry : store_.entries()) {
        if (!matches(entry.name))
            continue;
        userRows.push_back({ false, entry.favorite, entry.name, -1 });
    }
    const auto takeFavorites = [](std::vector<Row>& source, std::vector<Row>& destination) {
        for (auto& row : source)
            if (row.favorite)
                destination.push_back(row);
    };
    takeFavorites(factoryRows, rows_);
    takeFavorites(userRows, rows_);
    for (auto& row : factoryRows)
        if (!row.favorite)
            rows_.push_back(std::move(row));
    for (auto& row : userRows)
        if (!row.favorite)
            rows_.push_back(std::move(row));

    list_.updateContent();
    int reselect = -1;
    if (previousName.isNotEmpty()) {
        for (int i = 0; i < static_cast<int>(rows_.size()); ++i)
            if (rows_[static_cast<size_t>(i)].name == previousName)
                reselect = i;
    }
    if (reselect >= 0)
        list_.selectRow(reselect);
    else
        updateButtons();
    repaint();
}

void PresetBrowser::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool selected) {
    if (rowNumber < 0 || rowNumber >= static_cast<int>(rows_.size()))
        return;
    const auto& row = rows_[static_cast<size_t>(rowNumber)];
    if (selected)
        g.fillAll(colours_.edge);
    g.setColour(row.favorite ? colours_.accent : colours_.dim);
    g.fillEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre({ 14.0f, height * 0.5f }));
    g.setColour(selected ? colours_.text : (row.factory ? colours_.dim : colours_.text));
    g.setFont(juce::FontOptions(13.0f));
    auto text = juce::Rectangle<int>(kStarWidth, 0, width - kStarWidth - (row.factory ? 64 : 8), height);
    g.drawText(row.name, text, juce::Justification::centredLeft, true);
    if (row.factory) {
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(colours_.dim);
        g.drawText("FACTORY", width - 68, 0, 60, height, juce::Justification::centredRight);
    }
}

void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent& event) {
    if (row < 0 || row >= static_cast<int>(rows_.size()))
        return;
    if (event.x < kStarWidth) {
        auto& item = rows_[static_cast<size_t>(row)];
        store_.setFavorite(item.factory, item.name, !item.favorite);
        rebuild();
        return;
    }
    name_.setText(rows_[static_cast<size_t>(row)].name, juce::dontSendNotification);
    updateButtons();
    loadRow(row);
}

void PresetBrowser::selectedRowsChanged(int) {
    if (const auto* row = selectedRow())
        name_.setText(row->name, juce::dontSendNotification);
    updateButtons();
}

void PresetBrowser::loadRow(int row) {
    if (row < 0 || row >= static_cast<int>(rows_.size()))
        return;
    const auto& item = rows_[static_cast<size_t>(row)];
    if (item.factory) {
        if (onFactoryPreset)
            onFactoryPreset(item.program);
        return;
    }
    if (auto snapshot = store_.load(item.name); snapshot.has_value() && onUserPreset)
        onUserPreset(*snapshot);
}

const PresetBrowser::Row* PresetBrowser::selectedRow() const {
    const int row = list_.getSelectedRow();
    if (row < 0 || row >= static_cast<int>(rows_.size()))
        return nullptr;
    return &rows_[static_cast<size_t>(row)];
}

void PresetBrowser::updateButtons() {
    const auto* row = selectedRow();
    const bool user = row != nullptr && !row->factory;
    rename_.setEnabled(user);
    remove_.setEnabled(user);
    favorite_.setEnabled(row != nullptr);
    favorite_.setToggleState(row != nullptr && row->favorite, juce::dontSendNotification);
    favorite_.setButtonText(row != nullptr && row->favorite ? "UNFAV" : "FAVORITE");
}

} // namespace suite
