#include "EditorPane.h"
#include "Theme.h"

namespace progsynth {

EditorPane::EditorPane() {
    editor = std::make_unique<EditorWithKeys>(document, nullptr);
    editor->setFont(Theme::monoFont(14.0f));
    editor->setLineNumbersShown(true);
    editor->setColour(juce::CodeEditorComponent::backgroundColourId, Theme::bg());
    editor->setColour(juce::CodeEditorComponent::lineNumberBackgroundId, Theme::bgPanel());
    editor->setColour(juce::CodeEditorComponent::lineNumberTextId, Theme::fgDim());
    editor->setColour(juce::CodeEditorComponent::defaultTextColourId, Theme::fg());
    editor->setColour(juce::CodeEditorComponent::highlightColourId,
                      Theme::fg().withAlpha(0.25f));
    editor->setColour(juce::CaretComponent::caretColourId, Theme::accent());
    editor->onCompile = [this]() { if (onCompileRequested) onCompileRequested(); };
    addAndMakeVisible(*editor);
}

EditorPane::~EditorPane() = default;

juce::String EditorPane::getText() const {
    return document.getAllContent();
}

void EditorPane::setText(const juce::String& s) {
    document.replaceAllContent(s);
}

void EditorPane::resized() {
    if (editor) editor->setBounds(getLocalBounds());
}

void EditorPane::paint(juce::Graphics& g) {
    g.fillAll(Theme::bg());
}

} // namespace progsynth
