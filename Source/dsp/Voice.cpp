#include "Voice.h"
#include "SynthEngine.h"

#include <algorithm>
#include <cmath>

namespace progsynth {

ProgVoice::ProgVoice(SynthEngine& e) : engine(e) {}

void ProgVoice::prepare(double sr, int blockSize) {
    sampleRate = sr;
    osc1.prepare(sr); osc2.prepare(sr); osc3.prepare(sr);
    lfo1.prepare(sr); lfo2.prepare(sr);
    ampEnv.prepare(sr); fltEnv.prepare(sr);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = (juce::uint32)blockSize;
    spec.numChannels = 1;
    filter.prepare(spec);
    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    tempMono.setSize(1, blockSize, false, true, true);
}

void ProgVoice::startNote(int n, float vel,
                          juce::SynthesiserSound*, int)
{
    midiNote = n;
    velocity = vel;
    gate     = 1.0f;

    auto patch = engine.currentPatch();
    if (!patch) patch = std::make_shared<CompiledPatch>();

    osc1.setWave(patch->osc1.wave); osc1.reset();
    osc2.setWave(patch->osc2.wave); osc2.reset();
    osc3.setWave(patch->osc3.wave); osc3.reset();

    lfo1.setWave(patch->lfo1.wave);
    lfo2.setWave(patch->lfo2.wave);
    if (patch->lfo1.retrigger) lfo1.reset(0.0);
    if (patch->lfo2.retrigger) lfo2.reset(0.0);

    filter.setType(patch->filter.type == FilterKind::LP
                   ? juce::dsp::StateVariableTPTFilterType::lowpass
                   : juce::dsp::StateVariableTPTFilterType::highpass);
    filter.reset();

    // ADSR are evaluated once at note-on (with velocity available).
    ExprInputs in{};
    in.v[ExprInputs::Pitch]    = (double)midiNote;
    in.v[ExprInputs::NoteHz]   = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
    in.v[ExprInputs::Velocity] = velocity;
    in.v[ExprInputs::Gate]     = 1.0;

    double aA = patch->ampEnv.a.evaluate(in);
    double aD = patch->ampEnv.d.evaluate(in);
    double aS = patch->ampEnv.s.evaluate(in);
    double aR = patch->ampEnv.r.evaluate(in);
    ampEnv.setADSR(aA, aD, aS, aR);

    double fA = patch->fltEnv.a.evaluate(in);
    double fD = patch->fltEnv.d.evaluate(in);
    double fS = patch->fltEnv.s.evaluate(in);
    double fR = patch->fltEnv.r.evaluate(in);
    fltEnv.setADSR(fA, fD, fS, fR);

    ampEnv.hardReset();
    fltEnv.hardReset();
    ampEnv.noteOn();
    fltEnv.noteOn();

    controlCounter = 0;
}

void ProgVoice::stopNote(float, bool allowTailOff) {
    gate = 0.0f;
    if (allowTailOff) {
        ampEnv.noteOff();
        fltEnv.noteOff();
    } else {
        ampEnv.hardReset();
        fltEnv.hardReset();
        clearCurrentNote();
    }
}

void ProgVoice::controlTick(const CompiledPatch& patch) {
    ExprInputs in{};
    in.v[ExprInputs::Pitch]    = (double)midiNote;
    in.v[ExprInputs::NoteHz]   = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
    in.v[ExprInputs::Velocity] = velocity;
    in.v[ExprInputs::Gate]     = gate;
    in.v[ExprInputs::AmpEnv]   = ampEnv.currentValue();
    in.v[ExprInputs::FltEnv]   = fltEnv.currentValue();
    in.v[ExprInputs::Lfo1]     = lfo1.currentValue();
    in.v[ExprInputs::Lfo2]     = lfo2.currentValue();

    // Update LFO rates from patch.
    double bpm = engine.getBpm();
    auto lfoHz = [&](const LfoPatch& lp, const Expression& rate) -> double {
        if (lp.sync && lp.syncRate.valid) return lp.syncRate.toHz(bpm);
        return std::max(0.0001, rate.evaluate(in));
    };
    lfo1.setFrequency(lfoHz(patch.lfo1, patch.lfo1.rate));
    lfo2.setFrequency(lfoHz(patch.lfo2, patch.lfo2.rate));

    // Set oscillator frequencies and levels.
    osc1.setFrequency(std::max(0.01, patch.osc1.freq.evaluate(in)));
    osc2.setFrequency(std::max(0.01, patch.osc2.freq.evaluate(in)));
    osc3.setFrequency(std::max(0.01, patch.osc3.freq.evaluate(in)));
    osc1Lvl = (float)std::clamp(patch.osc1.level.evaluate(in), 0.0, 1.0);
    osc2Lvl = (float)std::clamp(patch.osc2.level.evaluate(in), 0.0, 1.0);
    osc3Lvl = (float)std::clamp(patch.osc3.level.evaluate(in), 0.0, 1.0);

    // Filter: cutoff with keytrack and env amount applied here.
    double cutoff   = patch.filter.cutoff.evaluate(in);
    double keytrack = std::clamp(patch.filter.keytrack.evaluate(in), 0.0, 1.0);
    double env      = std::clamp(patch.filter.env.evaluate(in), -1.0, 1.0);
    double res      = std::clamp(patch.filter.res.evaluate(in), 0.0, 0.99);

    if (keytrack > 0.0) {
        cutoff *= std::pow(2.0, (midiNote - 60.0) * keytrack / 12.0);
    }
    cutoff += env * 5000.0 * fltEnv.currentValue();

    cutoff = std::clamp(cutoff, 20.0, sampleRate * 0.45);
    curCutoffHz = (float)cutoff;
    curRes      = (float)res;

    filter.setCutoffFrequency(curCutoffHz);
    filter.setResonance(0.05f + 1.4f * curRes);   // map 0..1 -> 0.05..1.45
}

void ProgVoice::renderNextBlock(juce::AudioBuffer<float>& outBuffer,
                                int startSample, int numSamples)
{
    if (numSamples <= 0) return;
    if (!isVoiceActive()) return;

    auto patchPtr = engine.currentPatch();
    if (!patchPtr) return;

    if (tempMono.getNumSamples() < numSamples) {
        tempMono.setSize(1, numSamples, false, true, true);
    }
    float* mono = tempMono.getWritePointer(0);

    const CompiledPatch& patch = *patchPtr;

    int written = 0;
    while (written < numSamples) {
        if (controlCounter <= 0) {
            lfo1.advance(kControlBlock);
            lfo2.advance(kControlBlock);
            controlTick(patch);
            controlCounter = kControlBlock;
        }

        int n = std::min(controlCounter, numSamples - written);

        for (int i = 0; i < n; ++i) {
            float s1 = osc1.tick() * osc1Lvl;
            float s2 = osc2.tick() * osc2Lvl;
            float s3 = osc3.tick() * osc3Lvl;
            mono[written + i] = s1 + s2 + s3;
        }

        for (int i = 0; i < n; ++i) {
            mono[written + i] = filter.processSample(0, mono[written + i]);
        }

        ampEnv.advance(n);
        fltEnv.advance(n);
        float ampVal = ampEnv.currentValue();
        for (int i = 0; i < n; ++i) {
            mono[written + i] *= ampVal;
        }

        written += n;
        controlCounter -= n;
    }

    for (int ch = 0; ch < outBuffer.getNumChannels(); ++ch) {
        outBuffer.addFrom(ch, startSample, tempMono, 0, 0, numSamples);
    }

    if (!ampEnv.isActive()) {
        clearCurrentNote();
    }
}

} // namespace progsynth
