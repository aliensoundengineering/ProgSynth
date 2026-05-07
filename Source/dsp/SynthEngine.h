#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <atomic>
#include <vector>

#include "../lang/CompiledPatch.h"

namespace progsynth {

class ProgVoice;   // fwd

class SynthEngine {
public:
    SynthEngine();
    ~SynthEngine();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();

    // UI thread: install a new compiled patch atomically.
    void setPatch(std::shared_ptr<const CompiledPatch> patch);

    // Audio thread: snapshot the current patch.
    std::shared_ptr<const CompiledPatch> currentPatch() const;

    // Audio thread: process a block.
    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    // Split form: caller can tap the buffer between the two for spectrum/etc.
    void renderSynth(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);
    void applyMaster(juce::AudioBuffer<float>& buffer);

    void setBpm(double v) { bpm.store(v); }
    double getBpm() const { return bpm.load(); }

    // UI-thread diagnostics: number of voices with currentlyPlayingNote >= 0,
    // and a slow-decay peak meter of the last block's output magnitude.
    int   activeVoices() const;
    float lastBlockPeak() const { return lastPeak.load(std::memory_order_relaxed); }
    bool  hasPatchInstalled() const {
        juce::SpinLock::ScopedLockType lock(patchLock);
        return live != nullptr;
    }
    float currentMasterGain() const {
        juce::SpinLock::ScopedLockType lock(patchLock);
        if (!live) return -1.0f;
        ExprInputs in{};
        return (float) live->masterVolume.evaluate(in);
    }

    static constexpr int kVoices = 16;

private:
    // Patch handoff: UI thread updates under a spin-lock; audio thread reads
    // a copy of the shared_ptr under the same lock.  A short critical section.
    mutable juce::SpinLock                 patchLock;
    std::shared_ptr<const CompiledPatch>   live;

    juce::Synthesiser synth;

    // Direct (non-RTTI) handles to the voices so prepare() is guaranteed
    // to reach each one — juce::Synthesiser owns them.
    std::vector<ProgVoice*> progVoices;

    double sampleRate = 44100.0;
    int    blockSize  = 512;
    std::atomic<double> bpm{120.0};
    std::atomic<float>  lastPeak{0.0f};
};

} // namespace progsynth
