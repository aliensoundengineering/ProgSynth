#include "Oscillator.h"

#include <cmath>

namespace progsynth {

namespace {
// PolyBLEP: classic anti-aliasing residual for naive saw/square.
// dt = phase increment per sample (= freq / sampleRate)
inline double polyBlep(double t, double dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}
} // namespace

void Oscillator::prepare(double sr) {
    sampleRate = sr;
}

void Oscillator::reset() {
    phase = 0.0;
}

float Oscillator::tick() {
    if (sampleRate <= 0.0) return 0.0f;

    const double dt = freqHz / sampleRate;
    double v = 0.0;

    switch (wave) {
        case WaveKind::Sine:
            v = std::sin(2.0 * 3.14159265358979323846 * phase);
            break;
        case WaveKind::Tri: {
            // unipolar tri then scale to [-1,1]
            double tri = (phase < 0.5) ? (4.0 * phase - 1.0)
                                       : (3.0 - 4.0 * phase);
            v = tri;
            break;
        }
        case WaveKind::Saw: {
            double naive = 2.0 * phase - 1.0;
            naive -= polyBlep(phase, dt);
            v = naive;
            break;
        }
        case WaveKind::Square: {
            double naive = (phase < 0.5) ? 1.0 : -1.0;
            naive += polyBlep(phase, dt);
            double phase2 = phase + 0.5;
            if (phase2 >= 1.0) phase2 -= 1.0;
            naive -= polyBlep(phase2, dt);
            v = naive;
            break;
        }
    }

    phase += dt;
    if (phase >= 1.0) phase -= 1.0;
    if (phase < 0.0)  phase += 1.0;

    return (float)v;
}

} // namespace progsynth
