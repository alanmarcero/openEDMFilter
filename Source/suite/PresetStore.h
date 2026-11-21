#pragma once

#include <optional>
#include <vector>
#include <juce_data_structures/juce_data_structures.h>

namespace suite {

// User presets and favorites under ~/Documents/Alan Marcero/<plugin>/Presets.
// Factory presets are not stored here and are never renamed or deleted.
class PresetStore {
public:
    static juce::File defaultDirectory(const juce::String& pluginName);

    explicit PresetStore(juce::File directory);

    struct Entry {
        juce::String name;
        bool favorite = false;
    };

    [[nodiscard]] juce::File directory() const { return directory_; }
    [[nodiscard]] std::vector<Entry> entries() const;
    [[nodiscard]] bool isFavorite(bool factory, const juce::String& name) const;

    bool save(const juce::String& name, const juce::ValueTree& snapshot);
    bool rename(const juce::String& from, const juce::String& to);
    bool remove(const juce::String& name);
    bool setFavorite(bool factory, const juce::String& name, bool favorite);
    [[nodiscard]] std::optional<juce::ValueTree> load(const juce::String& name) const;
    // The name the store saves and lists a preset under.
    [[nodiscard]] static juce::String sanitize(juce::String name);

private:
    [[nodiscard]] juce::File fileFor(const juce::String& name) const;
    [[nodiscard]] juce::StringArray readFavorites() const;
    bool writeFavorites(const juce::StringArray& lines) const;
    [[nodiscard]] static juce::String favoriteKey(bool factory, const juce::String& name);

    juce::File directory_;
};

} // namespace suite
