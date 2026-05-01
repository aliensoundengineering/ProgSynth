#include "PluginEditor.h"

ProgSynthAudioProcessorEditor::ProgSynthAudioProcessorEditor(ProgSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(600, 400);
}

ProgSynthAudioProcessorEditor::~ProgSynthAudioProcessorEditor() = default;

void ProgSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkslateblue);

    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawFittedText("ProgSynth", getLocalBounds(),
        juce::Justification::centred, 1);
}

void ProgSynthAudioProcessorEditor::resized() {}