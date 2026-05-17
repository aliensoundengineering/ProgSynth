#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace progsynth {

struct Preset {
    juce::String name;
    juce::String script;
    bool         isFactory = false;
};

// The 10 baked-in presets that together exercise every command in the patch language.
std::vector<Preset> getFactoryPresets();

class PresetManager {
public:
    PresetManager();

    const std::vector<Preset>& all() const { return cache; }
    const Preset* find(const juce::String& name) const;

    // Persist `script` under `name`. Returns false if `name` collides with a
    // factory preset or is otherwise invalid.
    bool saveUser(const juce::String& name, const juce::String& script);

    // Re-scan the on-disk preset folder.
    void refresh();

    static juce::File getUserPresetDir();

private:
    void rebuildCache();

    std::vector<Preset> factory;
    std::vector<Preset> user;
    std::vector<Preset> cache;
};

} // namespace progsynth
