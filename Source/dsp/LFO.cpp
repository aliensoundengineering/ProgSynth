#include "LFO.h"

#include <cmath>

namespace progsynth {

void LFO::prepare(double sr) { sampleRate = sr; }
void LFO::reset(double p) {
    phase = p;
    if (phase < 0.0) phase += 1.0;
    if (phase >= 1.0) phase -= 1.0;
    value = 0.0f;
}

float LFO::advance(int samples) {
    if (sampleRate <= 0.0) return 0.0f;
    phase += (freqHz * (double)samples) / sampleRate;
    while (phase >= 1.0) phase -= 1.0;
    while (phase <  0.0) phase += 1.0;

    double v = 0.0;
    switch (wave) {
        case WaveKind::Sine:
            v = std::sin(2.0 * 3.14159265358979323846 * phase);
            break;
        case WaveKind::Tri:
            v = (phase < 0.5) ? (4.0 * phase - 1.0) : (3.0 - 4.0 * phase);
            break;
        case WaveKind::Saw:
            v = 2.0 * phase - 1.0;
            break;
        case WaveKind::Square:
            v = (phase < 0.5) ? 1.0 : -1.0;
            break;
    }
    value = (float)v;
    return value;
}

} // namespace progsynth
