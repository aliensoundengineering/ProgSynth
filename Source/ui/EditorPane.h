#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

namespace progsynth {

class EditorPane : public juce::Component {
public:
    EditorPane();
    ~EditorPane() override;

    juce::String getText() const;
    void setText(const juce::String& s);

    // Called when the user presses Ctrl+Enter (or Cmd+Enter on macOS).
    std::function<void()> onCompileRequested;

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    class EditorWithKeys : public juce::CodeEditorComponent {
    public:
        EditorWithKeys(juce::CodeDocument& d, juce::CodeTokeniser* t)
            : juce::CodeEditorComponent(d, t) {}
        std::function<void()> onCompile;
        bool keyPressed(const juce::KeyPress& k) override {
            const bool ctrl = k.getModifiers().isCommandDown()
                           || k.getModifiers().isCtrlDown();
            if (ctrl && k.getKeyCode() == juce::KeyPress::returnKey) {
                if (onCompile) onCompile();
                return true;
            }
            return juce::CodeEditorComponent::keyPressed(k);
        }
    };

    juce::CodeDocument document;
    std::unique_ptr<EditorWithKeys> editor;
};

} // namespace progsynth
