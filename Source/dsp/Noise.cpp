#include "Noise.h"

namespace progsynth {

void Noise::prepare(double sr) {
    sampleRate = sr;
    reset();
}

void Noise::reset() {
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0;
}

inline float Noise::nextWhite() noexcept {
    // xorshift32: cheap, deterministic per-voice, no allocation.
    uint32_t x = rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng = x;
    // Map to [-1, 1)
    return (float)((int32_t)x) * (1.0f / 2147483648.0f);
}

float Noise::tick() {
    float w = nextWhite();
    if (kind == NoiseKind::White) return w;

    // Paul Kellet's "economy" pink filter (≈ -3 dB/oct, ~0.1 dB ripple).
    b0 = 0.99886 * b0 + w * 0.0555179;
    b1 = 0.99332 * b1 + w * 0.0750759;
    b2 = 0.96900 * b2 + w * 0.1538520;
    b3 = 0.86650 * b3 + w * 0.3104856;
    b4 = 0.55000 * b4 + w * 0.5329522;
    b5 = -0.7616 * b5 - w * 0.0168980;
    double pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362;
    b6 = w * 0.115926;
    // The classic sum is roughly ±5 in peak; scale to ~[-1, 1].
    return (float)(pink * 0.11);
}

} // namespace progsynth
