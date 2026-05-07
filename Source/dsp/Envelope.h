#pragma once

namespace progsynth {

// Linear-time ADSR. value in [0,1].
class Envelope {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare(double sampleRate) { sr = sampleRate; }
    void setADSR(double a, double d, double s, double r);

    void noteOn();
    void noteOff();
    void hardReset();

    // Advance by N samples. Returns current value at end of block.
    float advance(int samples);

    bool isActive() const { return stage != Stage::Idle; }
    float currentValue() const { return value; }

private:
    double sr = 44100.0;
    double aSec = 0.005, dSec = 0.2, rSec = 0.3;
    double sustainLevel = 0.7;

    Stage  stage = Stage::Idle;
    float  value = 0.0f;
};

} // namespace progsynth
