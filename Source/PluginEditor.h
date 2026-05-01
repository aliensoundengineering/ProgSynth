#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class ProgSynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ProgSynthAudioProcessorEditor(ProgSynthAudioProcessor&);
    ~ProgSynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ProgSynthAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgSynthAudioProcessorEditor)
};