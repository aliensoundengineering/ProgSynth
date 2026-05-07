#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <atomic>

#include "../lang/CompiledPatch.h"
#include "../lang/ExprTree.h"
#include "Oscillator.h"
#include "LFO.h"
#include "Envelope.h"

namespace progsynth {

class SynthEngine;   // fwd

class ProgSound : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override    { return true; }
    bool appliesToChannel(int) override { return true; }
};

class ProgVoice : public juce::SynthesiserVoice {
public:
    explicit ProgVoice(SynthEngine& engine);

    void prepare(double sr, int blockSize);

    // Only one kind of sound is ever added; accept it unconditionally to avoid
    // any RTTI / cross-module dynamic_cast surprise.
    bool canPlaySound(juce::SynthesiserSound*) override { return true; }

    void startNote(int midiNote, float vel,
                   juce::SynthesiserSound*, int) override;
    void stopNote(float vel, bool allowTailOff) override;
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& out,
                         int startSample, int numSamples) override;

private:
    void controlTick(const CompiledPatch& patch);

    SynthEngine& engine;

    Oscillator osc1, osc2, osc3;
    LFO        lfo1, lfo2;
    Envelope   ampEnv, fltEnv;
    juce::dsp::StateVariableTPTFilter<float> filter;

    double sampleRate = 44100.0;

    static constexpr int kControlBlock = 32;
    int controlCounter = 0;

    // current voice state
    int   midiNote = 60;
    float velocity = 0.0f;
    float gate     = 0.0f;

    // resolved per-control-block parameter values
    float osc1Lvl = 0.7f, osc2Lvl = 0.0f, osc3Lvl = 0.0f;
    float curCutoffHz = 2000.0f;
    float curRes      = 0.2f;

    // a small temp mono buffer for voice output before adding to the dest
    juce::AudioBuffer<float> tempMono;
};

} // namespace progsynth
