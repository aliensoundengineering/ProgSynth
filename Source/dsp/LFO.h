#pragma once

#include "../lang/CompiledPatch.h"

namespace progsynth {

class LFO {
public:
    void prepare(double sampleRate);
    void reset(double startPhase01 = 0.0);
    void setWave(WaveKind w) { wave = w; }
    void setFrequency(double hz) { freqHz = hz; }
    void retrigger(double startPhase01 = 0.0) { phase = startPhase01; }

    // Advance by N samples (N == control block size). Returns current value (-1..+1).
    float advance(int samples);

    float currentValue() const { return value; }

private:
    double sampleRate = 44100.0;
    double phase = 0.0;
    double freqHz = 1.0;
    float  value = 0.0f;
    WaveKind wave = WaveKind::Sine;
};

} // namespace progsynth
