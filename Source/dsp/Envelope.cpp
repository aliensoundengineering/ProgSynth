#include "Envelope.h"

#include <algorithm>

namespace progsynth {

void Envelope::setADSR(double a, double d, double s, double r) {
    aSec = std::max(0.0001, a);
    dSec = std::max(0.0001, d);
    sustainLevel = std::clamp(s, 0.0, 1.0);
    rSec = std::max(0.0001, r);
}

void Envelope::noteOn() {
    stage = Stage::Attack;
}

void Envelope::noteOff() {
    if (stage != Stage::Idle)
        stage = Stage::Release;
}

void Envelope::hardReset() {
    stage = Stage::Idle;
    value = 0.0f;
}

float Envelope::advance(int samples) {
    if (samples <= 0 || sr <= 0.0) return value;

    double v = value;
    for (int i = 0; i < samples; ++i) {
        switch (stage) {
            case Stage::Idle:
                v = 0.0;
                break;
            case Stage::Attack: {
                double inc = 1.0 / (aSec * sr);
                v += inc;
                if (v >= 1.0) { v = 1.0; stage = Stage::Decay; }
                break;
            }
            case Stage::Decay: {
                double dec = (1.0 - sustainLevel) / (dSec * sr);
                v -= dec;
                if (v <= sustainLevel) { v = sustainLevel; stage = Stage::Sustain; }
                break;
            }
            case Stage::Sustain:
                v = sustainLevel;
                break;
            case Stage::Release: {
                double dec = std::max(1e-9, value > 0 ? (double)value : 1.0) / (rSec * sr);
                // simpler: ramp from current down to 0 over rSec, regardless of where we started
                dec = 1.0 / (rSec * sr);
                v -= dec;
                if (v <= 0.0) { v = 0.0; stage = Stage::Idle; }
                break;
            }
        }
    }
    value = (float)v;
    return value;
}

} // namespace progsynth
