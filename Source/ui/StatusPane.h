#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "../lang/CompiledPatch.h"

namespace progsynth {

class StatusPane : public juce::Component {
public:
    StatusPane();
    ~StatusPane() override;

    void showCompileSuccess(const CompiledPatch& patch);
    void showCompileErrors(const juce::StringArray& lines);
    void showInfo(const juce::String& line);

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    juce::TextEditor text;

    juce::String renderDiagram(const CompiledPatch&) const;
};

} // namespace progsynth
