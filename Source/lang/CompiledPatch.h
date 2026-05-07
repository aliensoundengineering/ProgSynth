#pragma once

#include <string>
#include <vector>
#include "ExprTree.h"

namespace progsynth {

enum class WaveKind { Sine, Tri, Saw, Square };
enum class FilterKind { LP, HP };

struct SyncRate {
    int  num = 1;
    int  den = 4;
    bool dotted = false;
    bool triplet = false;
    bool valid = false;

    // Convert to Hz given a tempo in BPM.
    // 1/n note period = (60/BPM) * 4/n * (num)
    // freq = BPM/60 * (den / (4 * num)) [scaled by dotted/triplet]
    double toHz(double bpm) const noexcept {
        if (!valid || num <= 0 || den <= 0 || bpm <= 0.0) return 1.0;
        double freq = (bpm / 60.0) * ((double)den / (4.0 * (double)num));
        if (dotted)  freq *= 2.0 / 3.0;   // 1.5x duration
        if (triplet) freq *= 3.0 / 2.0;   // 2/3 duration
        return freq;
    }
};

struct OscPatch {
    WaveKind   wave = WaveKind::Saw;
    Expression freq;        // pitch-context expression (output: Hz)
    Expression level;       // 0..1
};

struct FilterPatch {
    FilterKind type = FilterKind::LP;
    Expression cutoff;       // Hz
    Expression res;          // 0..1
    Expression env;          // -1..+1, signed amount of fltEnv added (Hz scale handled in voice)
    Expression keytrack;     // 0..1
};

struct EnvPatch {
    Expression a;    // seconds
    Expression d;    // seconds
    Expression s;    // 0..1
    Expression r;    // seconds
};

struct LfoPatch {
    WaveKind   wave   = WaveKind::Sine;
    Expression rate;          // Hz, or fallback derived from sync
    SyncRate   syncRate;
    bool       sync = false;
    bool       retrigger = true;
    Expression phase;         // 0..1
};

struct CompiledPatch {
    OscPatch    osc1, osc2, osc3;
    FilterPatch filter;
    EnvPatch    ampEnv;
    EnvPatch    fltEnv;
    LfoPatch    lfo1, lfo2;
    Expression  masterVolume;  // 0..1 linear gain

    // diagnostics
    std::vector<std::string> warnings;
    std::vector<std::string> routings;     // human-readable, e.g. "lfo2 -> filter.cutoff"
    int activeRoutings = 0;
};

} // namespace progsynth
