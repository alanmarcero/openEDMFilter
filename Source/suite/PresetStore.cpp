#include "PresetStore.h"
#include <algorithm>

namespace suite {

juce::File PresetStore::defaultDirectory(const juce::String& pluginName) {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Alan Marcero")
        .getChildFile(pluginName)
        .getChildFile("Presets");
}

PresetStore::PresetStore(juce::File directory)
    : directory_(std::move(directory)) {}

juce::String PresetStore::sanitize(juce::String name) {
    name = name.trim().substring(0, 80);
    juce::String cleaned;
    for (int i = 0; i < name.length(); ++i) {
        const auto character = name[i];
        const bool keep = juce::CharacterFunctions::isLetterOrDigit(character) || character == ' ' || character == '-' || character == '_';
        cleaned += keep ? juce::String::charToString(character) : "_";
    }
    cleaned = cleaned.trim();
    if (cleaned.isEmpty() || cleaned == "." || cleaned == "..")
        return {};
    return cleaned;
}

juce::File PresetStore::fileFor(const juce::String& name) const {
    return directory_.getChildFile(sanitize(name) + ".preset");
}

juce::String PresetStore::favoriteKey(bool factory, const juce::String& name) {
    return (factory ? "F\t" : "U\t") + name;
}

juce::StringArray PresetStore::readFavorites() const {
    const auto file = directory_.getChildFile("favorites.txt");
    juce::StringArray lines;
    file.readLines(lines);
    lines.removeEmptyStrings();
    return lines;
}

bool PresetStore::writeFavorites(const juce::StringArray& lines) const {
    if (!directory_.createDirectory())
        return false;
    return directory_.getChildFile("favorites.txt").replaceWithText(lines.joinIntoString("\n") + (lines.isEmpty() ? "" : "\n"));
}

bool PresetStore::isFavorite(bool factory, const juce::String& name) const {
    return readFavorites().contains(favoriteKey(factory, name));
}

std::vector<PresetStore::Entry> PresetStore::entries() const {
    std::vector<Entry> result;
    if (!directory_.isDirectory())
        return result;
    const auto favorites = readFavorites();
    for (const auto& file : directory_.findChildFiles(juce::File::findFiles, false, "*.preset")) {
        const auto xml = juce::XmlDocument::parse(file);
        juce::String name = file.getFileNameWithoutExtension();
        if (xml != nullptr && xml->hasTagName("SuitePreset"))
            name = xml->getStringAttribute("name", name);
        result.push_back({ name, favorites.contains(favoriteKey(false, name)) });
    }
    std::ranges::sort(result, [](const Entry& a, const Entry& b) { return a.name.compareIgnoreCase(b.name) < 0; });
    return result;
}

bool PresetStore::save(const juce::String& name, const juce::ValueTree& snapshot) {
    const auto cleaned = sanitize(name);
    if (cleaned.isEmpty() || !directory_.createDirectory())
        return false;
    juce::XmlElement xml("SuitePreset");
    xml.setAttribute("name", cleaned);
    for (int i = 0; i < snapshot.getNumChildren(); ++i) {
        const auto child = snapshot.getChild(i);
        if (!child.hasType("PARAM"))
            continue;
        auto* param = xml.createNewChildElement("PARAM");
        param->setAttribute("id", child.getProperty("id").toString());
        param->setAttribute("value", static_cast<double>(child.getProperty("value")));
    }
    return xml.writeTo(directory_.getChildFile(cleaned + ".preset"));
}

bool PresetStore::rename(const juce::String& from, const juce::String& to) {
    const auto sourceName = sanitize(from);
    const auto targetName = sanitize(to);
    if (sourceName.isEmpty() || targetName.isEmpty())
        return false;
    const auto source = directory_.getChildFile(sourceName + ".preset");
    const auto target = directory_.getChildFile(targetName + ".preset");
    if (!source.existsAsFile() || (target != source && target.existsAsFile()))
        return false;
    auto snapshot = load(sourceName);
    if (!snapshot.has_value() || !save(targetName, *snapshot))
        return false;
    if (source != target)
        source.deleteFile();
    const auto oldKey = favoriteKey(false, sourceName);
    if (readFavorites().contains(oldKey)) {
        auto lines = readFavorites();
        lines.removeString(oldKey);
        lines.addIfNotAlreadyThere(favoriteKey(false, targetName));
        writeFavorites(lines);
    }
    return true;
}

bool PresetStore::remove(const juce::String& name) {
    const auto file = fileFor(name);
    if (!file.existsAsFile() || !file.deleteFile())
        return false;
    auto lines = readFavorites();
    const auto key = favoriteKey(false, sanitize(name));
    if (lines.contains(key)) {
        lines.removeString(key);
        writeFavorites(lines);
    }
    return true;
}

bool PresetStore::setFavorite(bool factory, const juce::String& name, bool favorite) {
    if (name.isEmpty())
        return false;
    auto lines = readFavorites();
    const auto key = favoriteKey(factory, factory ? name : (sanitize(name).isEmpty() ? name : sanitize(name)));
    if (favorite == lines.contains(key))
        return true;
    if (favorite)
        lines.add(key);
    else
        lines.removeString(key);
    return writeFavorites(lines);
}

std::optional<juce::ValueTree> PresetStore::load(const juce::String& name) const {
    const auto xml = juce::XmlDocument::parse(fileFor(name));
    if (xml == nullptr || !xml->hasTagName("SuitePreset"))
        return std::nullopt;
    juce::ValueTree tree("Snapshot");
    for (auto* param = xml->getFirstChildElement(); param != nullptr; param = param->getNextElement()) {
        if (!param->hasTagName("PARAM"))
            continue;
        juce::ValueTree child("PARAM");
        child.setProperty("id", param->getStringAttribute("id"), nullptr);
        child.setProperty("value", param->getDoubleAttribute("value"), nullptr);
        tree.appendChild(child, nullptr);
    }
    return tree;
}

} // namespace suite
