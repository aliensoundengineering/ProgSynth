#include "SynthEngine.h"
#include "Voice.h"

#include <cmath>

namespace progsynth {

SynthEngine::SynthEngine() {
    synth.addSound(new ProgSound());
    for (int i = 0; i < kVoices; ++i) {
        auto* v = new ProgVoice(*this);
        progVoices.push_back(v);
        synth.addVoice(v);    // synth takes ownership
    }
}

SynthEngine::~SynthEngine() = default;

void SynthEngine::prepareToPlay(double sr, int block) {
    sampleRate = sr;
    blockSize  = block;
    synth.setCurrentPlaybackSampleRate(sr);
    synth.setMinimumRenderingSubdivisionSize(1, true);
    for (auto* v : progVoices) {
        v->prepare(sr, block);
    }
}

void SynthEngine::releaseResources() {}

void SynthEngine::setPatch(std::shared_ptr<const CompiledPatch> patch) {
    juce::SpinLock::ScopedLockType lock(patchLock);
    live = std::move(patch);
}

std::shared_ptr<const CompiledPatch> SynthEngine::currentPatch() const {
    juce::SpinLock::ScopedLockType lock(patchLock);
    return live;
}

int SynthEngine::activeVoices() const {
    int n = 0;
    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* v = synth.getVoice(i))
            if (v->isVoiceActive())
                ++n;
    }
    return n;
}

void SynthEngine::renderSynth(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    buffer.clear();
    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());

    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        peak = std::max(peak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    }
    float prev    = lastPeak.load(std::memory_order_relaxed);
    float decayed = prev * 0.85f;
    lastPeak.store(std::max(peak, decayed), std::memory_order_relaxed);
}

void SynthEngine::applyMaster(juce::AudioBuffer<float>& buffer) {
    if (auto patch = currentPatch()) {
        ExprInputs in{};
        double g = patch->masterVolume.evaluate(in);
        if (!std::isfinite(g) || g < 0.0) g = 1.0;   // never silence by accident
        if (g > 4.0) g = 4.0;
        buffer.applyGain((float)g);
    }
}

void SynthEngine::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    renderSynth(buffer, midi);
    applyMaster(buffer);
}

} // namespace progsynth
