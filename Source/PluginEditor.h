#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>

#include "PluginProcessor.h"
#include "ui/EditorPane.h"
#include "ui/StatusPane.h"
#include "ui/ReplPane.h"
#include "ui/SpectrumPane.h"

class ProgSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit ProgSynthAudioProcessorEditor(ProgSynthAudioProcessor&);
    ~ProgSynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void compileFromEditor();
    void handleReplCommand(const juce::String& line);
    void timerCallback() override;

    ProgSynthAudioProcessor& processorRef;

    progsynth::EditorPane    editor;
    progsynth::StatusPane    status;
    progsynth::SpectrumPane  spectrum;
    progsynth::ReplPane      repl;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgSynthAudioProcessorEditor)
};
