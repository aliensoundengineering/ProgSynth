#include "PluginProcessor.h"
#include "PluginEditor.h"

ProgSynthAudioProcessor::ProgSynthAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{}

ProgSynthAudioProcessor::~ProgSynthAudioProcessor() = default;

void ProgSynthAudioProcessor::prepareToPlay(double, int) {}
void ProgSynthAudioProcessor::releaseResources() {}

bool ProgSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::stereo();
}

void ProgSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* ProgSynthAudioProcessor::createEditor()
{
    return new ProgSynthAudioProcessorEditor(*this);
}

void ProgSynthAudioProcessor::getStateInformation(juce::MemoryBlock&) {}
void ProgSynthAudioProcessor::setStateInformation(const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ProgSynthAudioProcessor();
}