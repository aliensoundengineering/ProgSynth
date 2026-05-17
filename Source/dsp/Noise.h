#pragma once

#include <cstdint>

#include "../lang/CompiledPatch.h"

namespace progsynth {

class Noise {
public:
    void prepare(double sampleRate);
    void reset();
    void setKind(NoiseKind k) { kind = k; }
    float tick();   // produce one sample in [-1, 1]

private:
    double sampleRate = 44100.0;
    NoiseKind kind = NoiseKind::White;

    // xorshift32 PRNG state
    uint32_t rng = 0x9E3779B9u;
    inline float nextWhite() noexcept;

    // Paul Kellet's economical pink-noise filter state
    double b0 = 0.0, b1 = 0.0, b2 = 0.0, b3 = 0.0,
           b4 = 0.0, b5 = 0.0, b6 = 0.0;
};

} // namespace progsynth
