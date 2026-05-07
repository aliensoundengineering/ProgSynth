#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace progsynth {

class ReplPane : public juce::Component, private juce::TextEditor::Listener {
public:
    ReplPane();
    ~ReplPane() override;

    // Called when user submits a non-empty line (presses Return).
    std::function<void(const juce::String&)> onSubmit;

    void echo(const juce::String& line);   // append a response line in the prompt's history label

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    void textEditorReturnKeyPressed(juce::TextEditor&) override;

    juce::TextEditor input;
    juce::Label      prompt;
    juce::TextEditor history;
};

} // namespace progsynth
