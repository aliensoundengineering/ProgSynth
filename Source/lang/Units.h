#pragma once

#include <string>

namespace progsynth {

enum class Unit {
    None,    // bare number
    Hz,
    KHz,
    Ms,
    S,
    St,      // semitone
    Cent,
    Db,
    Percent
};

inline const char* unitName(Unit u) {
    switch (u) {
        case Unit::None:    return "";
        case Unit::Hz:      return "Hz";
        case Unit::KHz:     return "kHz";
        case Unit::Ms:      return "ms";
        case Unit::S:       return "s";
        case Unit::St:      return "st";
        case Unit::Cent:    return "cent";
        case Unit::Db:      return "dB";
        case Unit::Percent: return "%";
    }
    return "";
}

// Parameter kind drives unit coercion at compile time.
enum class ParamKind {
    Frequency,    // canonical Hz (cutoff, lfo.rate)
    Pitch,        // canonical semitone, evaluated then midi->Hz at the end (osc.freq)
    Time,         // canonical seconds (ADSR a/d/r)
    Level,        // unitless 0..1 (level, sustain, res, phase, keytrack)
    SignedLevel,  // -1..+1 (filter env amount)
    Volume,       // 0..1, accepts dB
};

} // namespace progsynth
