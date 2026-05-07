#include "ReplPane.h"
#include "Theme.h"

namespace progsynth {

ReplPane::ReplPane() {
    history.setMultiLine(true);
    history.setReadOnly(true);
    history.setScrollbarsShown(true);
    history.setCaretVisible(false);
    history.setFont(Theme::monoFont(13.0f));
    history.setColour(juce::TextEditor::backgroundColourId, Theme::bg());
    history.setColour(juce::TextEditor::textColourId,       Theme::fgDim());
    history.setColour(juce::TextEditor::outlineColourId,    juce::Colours::transparentBlack);
    history.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(history);

    prompt.setText("> ", juce::dontSendNotification);
    prompt.setFont(Theme::monoFont(14.0f));
    prompt.setColour(juce::Label::textColourId, Theme::accent());
    prompt.setColour(juce::Label::backgroundColourId, Theme::bg());
    addAndMakeVisible(prompt);

    input.setMultiLine(false);
    input.setReturnKeyStartsNewLine(false);
    input.setFont(Theme::monoFont(14.0f));
    input.setColour(juce::TextEditor::backgroundColourId, Theme::bg());
    input.setColour(juce::TextEditor::textColourId,       Theme::fg());
    input.setColour(juce::TextEditor::outlineColourId,    juce::Colours::transparentBlack);
    input.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    input.setColour(juce::CaretComponent::caretColourId,  Theme::accent());
    input.addListener(this);
    addAndMakeVisible(input);
}

ReplPane::~ReplPane() = default;

void ReplPane::resized() {
    auto r = getLocalBounds().reduced(4);
    auto promptRow = r.removeFromBottom(22);
    history.setBounds(r);
    int promptW = 18;
    prompt.setBounds(promptRow.removeFromLeft(promptW));
    input.setBounds(promptRow);
}

void ReplPane::paint(juce::Graphics& g) {
    g.fillAll(Theme::bg());
    g.setColour(Theme::gridLine());
    g.drawHorizontalLine(0, 0.0f, (float)getWidth());
}

void ReplPane::echo(const juce::String& line) {
    auto current = history.getText();
    if (current.isNotEmpty()) current << "\n";
    current << line;
    history.setText(current, false);
    history.moveCaretToEnd();
}

void ReplPane::textEditorReturnKeyPressed(juce::TextEditor& ed) {
    auto line = ed.getText().trim();
    ed.clear();
    if (line.isEmpty()) return;
    echo("> " + line);
    if (onSubmit) onSubmit(line);
}

} // namespace progsynth
