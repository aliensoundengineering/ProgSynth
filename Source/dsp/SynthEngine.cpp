#include "SynthEngine.h"
#include "Voice.h"

#include <algorithm>
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

    // Prepare the FX chain (always — bypass is decided per-block from the patch).
    juce::dsp::ProcessSpec stereoSpec;
    stereoSpec.sampleRate       = sr;
    stereoSpec.maximumBlockSize = (juce::uint32) std::max(1, block);
    stereoSpec.numChannels      = 2;

    fxReverb.prepare(stereoSpec);   fxReverb.reset();
    fxChorus.prepare(stereoSpec);   fxChorus.reset();
    fxFlanger.prepare(stereoSpec);  fxFlanger.reset();
    fxComp.prepare(stereoSpec);     fxComp.reset();

    juce::dsp::ProcessSpec monoSpec = stereoSpec;
    monoSpec.numChannels = 1;
    eqLowL.prepare(monoSpec);   eqLowR.prepare(monoSpec);
    eqMidL.prepare(monoSpec);   eqMidR.prepare(monoSpec);
    eqHighL.prepare(monoSpec);  eqHighR.prepare(monoSpec);
    eqLowL.reset();  eqLowR.reset();
    eqMidL.reset();  eqMidR.reset();
    eqHighL.reset(); eqHighR.reset();

    // 4 seconds of stereo delay headroom.
    dlyMaxSamples = std::max(1, (int) std::ceil(sr * 4.0));
    dlyBufL.assign((size_t) dlyMaxSamples, 0.0f);
    dlyBufR.assign((size_t) dlyMaxSamples, 0.0f);
    dlyWriteIdx = 0;

    fxPrepared = true;
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

void SynthEngine::applyEffects(juce::AudioBuffer<float>& buffer) {
    if (!fxPrepared) return;
    auto patch = currentPatch();
    if (!patch) return;

    const int N = buffer.getNumSamples();
    const int C = buffer.getNumChannels();
    if (N <= 0 || C <= 0) return;

    ExprInputs in{};   // FX expressions are global; only constants meaningfully evaluate.

    // 1) Distortion --------------------------------------------------------
    if (patch->distortion.enabled) {
        double drive = std::max(1e-6, patch->distortion.drive.evaluate(in));
        double mix   = std::clamp(patch->distortion.mix.evaluate(in), 0.0, 1.0);
        const bool hard = patch->distortion.shape == DistortionShape::Hard;

        for (int ch = 0; ch < C; ++ch) {
            float* d = buffer.getWritePointer(ch);
            for (int i = 0; i < N; ++i) {
                const float dry = d[i];
                const float pre = (float)(drive * dry);
                const float wet = hard ? std::clamp(pre, -1.0f, 1.0f)
                                       : std::tanh(pre);
                d[i] = (float)((1.0 - mix) * dry + mix * wet);
            }
        }
    }

    // 2) Three-band EQ -----------------------------------------------------
    if (patch->eq.enabled) {
        const double sr     = sampleRate;
        const double maxF   = sr * 0.49;
        const double lf     = std::clamp(patch->eq.lowFreq.evaluate(in),  20.0, maxF);
        const double lg     = std::max(0.0001, patch->eq.lowGain.evaluate(in));
        const double mf     = std::clamp(patch->eq.midFreq.evaluate(in),  20.0, maxF);
        const double mq     = std::max(0.1,    patch->eq.midQ.evaluate(in));
        const double mg     = std::max(0.0001, patch->eq.midGain.evaluate(in));
        const double hf     = std::clamp(patch->eq.highFreq.evaluate(in), 20.0, maxF);
        const double hg     = std::max(0.0001, patch->eq.highGain.evaluate(in));

        auto lowCo  = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, lf, 0.7f, (float) lg);
        auto midCo  = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, mf, (float) mq, (float) mg);
        auto highCo = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, hf, 0.7f, (float) hg);

        *eqLowL.coefficients  = *lowCo;   *eqLowR.coefficients  = *lowCo;
        *eqMidL.coefficients  = *midCo;   *eqMidR.coefficients  = *midCo;
        *eqHighL.coefficients = *highCo;  *eqHighR.coefficients = *highCo;

        float* L = buffer.getWritePointer(0);
        float* R = (C > 1) ? buffer.getWritePointer(1) : nullptr;
        for (int i = 0; i < N; ++i) {
            float s = L[i];
            s = eqLowL.processSample(s);
            s = eqMidL.processSample(s);
            s = eqHighL.processSample(s);
            L[i] = s;
            if (R) {
                float r = R[i];
                r = eqLowR.processSample(r);
                r = eqMidR.processSample(r);
                r = eqHighR.processSample(r);
                R[i] = r;
            }
        }
    }

    // 3) Compressor --------------------------------------------------------
    if (patch->compressor.enabled) {
        const double thrLin = std::max(1e-9, patch->compressor.threshold.evaluate(in));
        const double ratio  = std::max(1.0,  patch->compressor.ratio.evaluate(in));
        const double atk_s  = std::max(0.0001, patch->compressor.attack.evaluate(in));
        const double rel_s  = std::max(0.001,  patch->compressor.release.evaluate(in));
        const double makeup = std::max(0.0,    patch->compressor.makeup.evaluate(in));

        fxComp.setThreshold((float)(20.0 * std::log10(thrLin)));
        fxComp.setRatio    ((float) ratio);
        fxComp.setAttack   ((float)(atk_s * 1000.0));
        fxComp.setRelease  ((float)(rel_s * 1000.0));

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        fxComp.process(ctx);

        if (makeup != 1.0) buffer.applyGain((float) makeup);
    }

    // 4) Chorus ------------------------------------------------------------
    if (patch->chorus.enabled) {
        const double rate   = std::max(0.01, patch->chorus.rate.evaluate(in));
        const double depth  = std::clamp(patch->chorus.depth.evaluate(in), 0.0, 1.0);
        const double centre = std::clamp(patch->chorus.centreDelay.evaluate(in), 0.0, 0.1);
        const double fb     = std::clamp(patch->chorus.feedback.evaluate(in), -0.95, 0.95);
        const double mix    = std::clamp(patch->chorus.mix.evaluate(in), 0.0, 1.0);
        fxChorus.setRate       ((float) rate);
        fxChorus.setDepth      ((float) depth);
        fxChorus.setCentreDelay((float)(centre * 1000.0));
        fxChorus.setFeedback   ((float) fb);
        fxChorus.setMix        ((float) mix);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        fxChorus.process(ctx);
    }

    // 5) Flanger (juce::dsp::Chorus tuned to short delays / strong feedback) -
    if (patch->flanger.enabled) {
        const double rate   = std::max(0.01, patch->flanger.rate.evaluate(in));
        const double depth  = std::clamp(patch->flanger.depth.evaluate(in), 0.0, 1.0);
        const double centre = std::clamp(patch->flanger.centreDelay.evaluate(in), 0.0, 0.02);
        const double fb     = std::clamp(patch->flanger.feedback.evaluate(in), -0.95, 0.95);
        const double mix    = std::clamp(patch->flanger.mix.evaluate(in), 0.0, 1.0);
        fxFlanger.setRate       ((float) rate);
        fxFlanger.setDepth      ((float) depth);
        fxFlanger.setCentreDelay((float)(centre * 1000.0));
        fxFlanger.setFeedback   ((float) fb);
        fxFlanger.setMix        ((float) mix);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        fxFlanger.process(ctx);
    }

    // 6) Stereo feedback delay --------------------------------------------
    if (patch->delay.enabled && dlyMaxSamples > 1) {
        double t;
        if (patch->delay.sync && patch->delay.syncRate.valid) {
            const double hz = patch->delay.syncRate.toHz(bpm.load());
            t = (hz > 0.0) ? (1.0 / hz) : 0.25;
        } else {
            t = std::max(0.001, patch->delay.time.evaluate(in));
        }
        const double fb  = std::clamp(patch->delay.feedback.evaluate(in), 0.0, 0.95);
        const double mix = std::clamp(patch->delay.mix.evaluate(in), 0.0, 1.0);
        const int delaySamples = std::clamp((int) std::round(t * sampleRate),
                                            1, dlyMaxSamples - 1);

        float* L = buffer.getWritePointer(0);
        float* R = (C > 1) ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < N; ++i) {
            int readIdx = dlyWriteIdx - delaySamples;
            if (readIdx < 0) readIdx += dlyMaxSamples;

            const float dryL = L[i];
            const float dryR = R ? R[i] : dryL;

            const float wetL = dlyBufL[(size_t) readIdx];
            const float wetR = dlyBufR[(size_t) readIdx];

            dlyBufL[(size_t) dlyWriteIdx] = dryL + wetL * (float) fb;
            dlyBufR[(size_t) dlyWriteIdx] = dryR + wetR * (float) fb;

            L[i] = dryL * (float)(1.0 - mix) + wetL * (float) mix;
            if (R) R[i] = dryR * (float)(1.0 - mix) + wetR * (float) mix;

            if (++dlyWriteIdx >= dlyMaxSamples) dlyWriteIdx = 0;
        }
    }

    // 7) Reverb ------------------------------------------------------------
    if (patch->reverb.enabled) {
        juce::Reverb::Parameters p;
        p.roomSize   = (float) std::clamp(patch->reverb.size.evaluate(in),    0.0, 1.0);
        p.damping    = (float) std::clamp(patch->reverb.damping.evaluate(in), 0.0, 1.0);
        p.width      = (float) std::clamp(patch->reverb.width.evaluate(in),   0.0, 1.0);
        const double mix = std::clamp(patch->reverb.mix.evaluate(in), 0.0, 1.0);
        p.wetLevel   = (float) mix;
        p.dryLevel   = (float)(1.0 - mix);
        p.freezeMode = 0.0f;
        fxReverb.setParameters(p);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        fxReverb.process(ctx);
    }
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
    applyEffects(buffer);
    applyMaster(buffer);
}

} // namespace progsynth
