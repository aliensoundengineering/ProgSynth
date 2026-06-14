#pragma once

#include "../lang/CompiledPatch.h"

namespace progsynth {

class Oscillator {
public:
    void prepare(double sampleRate);
    void reset();
    void setWave(WaveKind w) { wave = w; }
    void setFrequency(double hz) { freqHz = hz; }
    
    void setPulseWidth(double w) {
        pulseWidth = w < 0.01 ? 0.01 : (w > 0.99 ? 0.99 : w);
    }

    float tick();   // produce one sample, advance phase

private:
    double sampleRate = 44100.0;
    double phase = 0.0;          // [0, 1)
    double freqHz = 440.0;
    double pulseWidth = 0.5;     // duty (square) / symmetry (tri)
    WaveKind wave = WaveKind::Saw;
};

} // namespace progsynth
