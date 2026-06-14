#include "Presets.h"

#include <algorithm>

namespace progsynth {

// ---------------------------------------------------------------------------
// Factory presets
// ---------------------------------------------------------------------------
//
// The set below is designed so that every block, wave, unit, modulation
// source, sync literal, distortion shape, etc. described in LANGUAGE.md is
// used by at least one preset.
//
//   1  Warm Pad        let, comments, osc1/2/3 (saw/square), filter lp,
//                      ampEnv, fltEnv, lfo1 (sync=off, retrigger=off),
//                      lfo1 -> osc3.pw (PWM), chorus, reverb,
//                      units st/cent/Hz/kHz/ms/s/dB/%
//   2  Acid Bass       sub alias, velocity, fltEnv, distortion soft,
//                      compressor
//   3  FM-ish Lead     sine+tri+square, eq, flanger, delay sync triplet
//                      (1/8t)
//   4  Plucked Keys    static pw (narrow pulse), delay sync dotted (1/8.),
//                      distortion soft, reverb
//   5  Noise Hat       noise white, filter hp, velocity, eq, compressor
//   6  Wind Drone      noise pink, lfo1 sync dotted (1/2.), lfo2 free,
//                      retrigger=on, non-zero phase
//   7  Bell            ampEnv as expression input, eq cuts, reverb
//   8  Synth Brass     note input, % unit on keytrack
//   9  Chiptune Lead   gate input, distortion hard, lfo1 routed to filter
//  10  Dubstep Wobble  '/' arithmetic, parentheses, lfo2 sync (1/4),
//                      distortion hard, delay sync triplet (1/4t)
//  11  PWM Sweep        pulse-width sweep: lfo1 -> osc1.pw (PWM), isolated
//                      square so the duty-cycle timbre change is obvious
//
// Everything compiles cleanly against the compiler in Source/lang/.
// ---------------------------------------------------------------------------

static const char* k1_WarmPad = R"(# Warm Pad - lush detuned saws, slow LFO swell, chorus + reverb.

let detune = 7cent
let swell  = lfo1 * 300Hz

osc1 { wave = saw,    freq = pitch + detune, level = 0.5 }
osc2 { wave = saw,    freq = pitch - detune, level = 0.5 }
osc3 { wave = square, freq = pitch - 12st,   level = 0.25, pw = 0.5 + lfo1 * 0.25 }

filter {
    type     = lp,
    cutoff   = 800Hz + 1.2kHz * fltEnv + swell,
    res      = 0.3,
    env      = 0.7,
    keytrack = 0.4
}

ampEnv { a = 250ms, d = 600ms, s = 0.85, r = 800ms }
fltEnv { a = 120ms, d = 1.2s,  s = 0.4,  r = 700ms }

lfo1 { wave = sine, rate = 0.4Hz, sync = off, retrigger = off, phase = 0 }

chorus { rate = 0.7Hz, depth = 0.35, centreDelay = 9ms, feedback = 0.15, mix = 0.55 }
reverb { mix = 0.45, size = 0.85, damping = 0.4, width = 100% }
master { volume = -8dB }
)";

static const char* k2_AcidBass = R"(# Acid Bass - squelchy resonant bass with soft drive.

osc1 { wave = saw, freq = pitch,         level = 0.8 }
osc2 { wave = sub, freq = pitch - 12st,  level = 0.5 }
osc3 { wave = saw, freq = pitch + 3cent, level = 0.3 }

filter {
    type     = lp,
    cutoff   = 120Hz + 4kHz * fltEnv + 600Hz * velocity,
    res      = 0.85,
    env      = 0.9,
    keytrack = 0.6
}

ampEnv { a = 2ms, d = 180ms, s = 0,    r = 80ms  }
fltEnv { a = 1ms, d = 220ms, s = 0.05, r = 120ms }

distortion { shape = soft, drive = 9dB, mix = 0.7 }
compressor { threshold = -14dB, ratio = 6, attack = 4ms, release = 90ms, makeup = 3dB }
master     { volume = -4dB }
)";

static const char* k3_FmLead = R"(# FM-ish Lead - bright detuned lead with rhythmic triplet delay.

let spread = 10cent

osc1 { wave = sine,   freq = pitch,          level = 0.4  }
osc2 { wave = tri,    freq = pitch + spread, level = 0.5  }
osc3 { wave = square, freq = pitch + 12st,   level = 0.25 }

filter { type = lp, cutoff = 3.5kHz, res = 0.25 }
ampEnv { a = 8ms, d = 150ms, s = 0.7, r = 220ms }
fltEnv { a = 5ms, d = 200ms, s = 0.5, r = 200ms }

eq {
    lowFreq  = 150Hz,  lowGain  = -2dB,
    midFreq  = 1.8kHz, midQ     = 0.9, midGain = 3dB,
    highFreq = 7kHz,   highGain = 4dB
}

flanger { rate = 0.25Hz, depth = 0.6, centreDelay = 2.5ms, feedback = 0.55, mix = 0.4 }
delay   { time = 1/8t,   sync  = on,  feedback   = 0.45,   mix      = 0.3 }
master  { volume = -6dB }
)";

static const char* k4_Plucked = R"(# Plucked Keys - short pluck with dotted-eighth delay.

osc1 { wave = square, freq = pitch,        level = 0.55, pw = 0.3 }
osc2 { wave = sine,   freq = pitch + 7st,  level = 0.3  }
osc3 { wave = tri,    freq = pitch - 12st, level = 0.25 }

filter { type = lp, cutoff = 1.8kHz + 1.5kHz * fltEnv, res = 0.35, env = 0.5 }

ampEnv { a = 1ms, d = 250ms, s = 0, r = 200ms }
fltEnv { a = 1ms, d = 180ms, s = 0, r = 150ms }

distortion { shape = soft, drive = 5dB, mix = 0.35 }
delay      { time = 1/8.,  sync  = on,  feedback = 0.5, mix = 0.35 }
reverb     { mix = 0.2, size = 0.55, damping = 0.55, width = 0.85 }
master     { volume = -5dB }
)";

static const char* k5_NoiseHat = R"(# Noise Hat - closed hi-hat from white noise.

# silence the default oscillators
osc1 { wave = sine, freq = pitch, level = 0 }

noise { type = white, level = velocity * 0.9 }

filter { type = hp, cutoff = 6kHz, res = 0.2, keytrack = 0 }
ampEnv { a = 0.5ms, d = 60ms, s = 0, r = 40ms }

eq {
    lowFreq  = 200Hz, lowGain  = -12dB,
    midFreq  = 4kHz,  midQ     = 1.2, midGain = 0dB,
    highFreq = 9kHz,  highGain = 6dB
}

compressor { threshold = -20dB, ratio = 3, attack = 1ms, release = 50ms, makeup = 2dB }
master     { volume = -10dB }
)";

static const char* k6_WindDrone = R"(# Wind Drone - evolving pink noise over a deep sub.

let sweep = lfo1 * 400Hz
let drift = lfo2 * 80Hz

osc1  { wave = sine, freq = pitch - 24st, level = 0.2 }
noise { type = pink, level = 0.65 }

filter { type = lp, cutoff = 700Hz + sweep + drift, res = 0.45, keytrack = 0.1 }
ampEnv { a = 1.2s, d = 800ms, s = 0.9, r = 2s }

lfo1 { wave = tri, rate = 1/2.,   sync = on,  retrigger = off, phase = 0.25 }
lfo2 { wave = saw, rate = 0.15Hz, sync = off, retrigger = on, phase = 0 }

chorus { rate = 0.3Hz, depth = 0.4, centreDelay = 18ms, feedback = 0.2, mix = 0.6 }
reverb { mix = 0.55, size = 0.9, damping = 0.3, width = 1.0 }
master { volume = -9dB }
)";

static const char* k7_Bell = R"(# Bell - inharmonic partials with long reverb tail.

osc1 { wave = sine, freq = pitch,                  level = 0.5 }
osc2 { wave = sine, freq = pitch + 19st + 3cent,   level = 0.3 * ampEnv }
osc3 { wave = tri,  freq = pitch + 28st,           level = 0.15 }

filter { type = lp, cutoff = 5kHz, res = 0.1 }
ampEnv { a = 1ms, d = 1.5s, s = 0, r = 1.8s }

eq {
    lowFreq  = 250Hz, lowGain  = -3dB,
    midFreq  = 900Hz, midQ     = 1.5, midGain = -6dB,
    highFreq = 5kHz,  highGain = 2dB
}

reverb { mix = 0.5, size = 0.95, damping = 0.25, width = 100% }
master { volume = -7dB }
)";

static const char* k8_SynthBrass = R"(# Synth Brass - velocity-driven brass.

osc1 { wave = saw, freq = note,          level = 0.55 }
osc2 { wave = saw, freq = note + 12cent, level = 0.55 }
osc3 { wave = saw, freq = note - 12st,   level = 0.2  }

filter {
    type     = lp,
    cutoff   = 400Hz + 2.5kHz * fltEnv + velocity * 1.5kHz,
    res      = 0.25,
    env      = 0.85,
    keytrack = 50%
}

ampEnv { a = 30ms, d = 250ms, s = 0.75, r = 250ms }
fltEnv { a = 40ms, d = 400ms, s = 0.55, r = 300ms }

eq {
    lowFreq  = 180Hz,  lowGain  = 2dB,
    midFreq  = 1.2kHz, midQ     = 0.8, midGain = 3dB,
    highFreq = 6kHz,   highGain = -1dB
}

compressor { threshold = -16dB, ratio = 4, attack = 8ms, release = 120ms, makeup = 2dB }
master     { volume = -5dB }
)";

static const char* k9_Chiptune = R"(# Chiptune Lead - 8-bit lead, hard-clipped square + tri.

osc1 { wave = square, freq = pitch,         level = 0.6 * gate }
osc2 { wave = tri,    freq = pitch + 5cent, level = 0.35 }
osc3 { wave = square, freq = pitch + 7st,   level = 0.25 }

filter { type = lp, cutoff = 4kHz + lfo1 * 200Hz, res = 0, keytrack = 0.2 }
ampEnv { a = 1ms, d = 80ms, s = 0.9, r = 30ms }
fltEnv { a = 1ms, d = 60ms, s = 0.5, r = 30ms }

lfo1 { wave = square, rate = 8Hz, sync = off, retrigger = on, phase = 0 }

distortion { shape = hard, drive = 6dB, mix = 1.0 }
master     { volume = -6dB }
)";

static const char* k11_PwmDemo = R"(# PWM Sweep
#
# One raw square oscillator, nothing else in the way. lfo1 slowly sweeps its
# pulse width (pw) from a thin ~10% pulse, through the symmetric 50% square,
# out to a thin ~90% pulse, and back. The filter is wide open so you hear the
# oscillator itself, not the processing.
#
# Listen for:
#   pw = 0.5        -> a full, hollow square (odd harmonics only)
#   pw -> 0.1 / 0.9 -> thinner, brighter, more nasal (denser harmonics)
#   the moving width -> the classic chorus-like "PWM" animation
#
# To A/B static widths instead, replace the osc1 line with one of these and
# re-compile (Ctrl+Enter):
#   osc1 { wave = square, freq = pitch, level = 0.8, pw = 0.5  }   # square
#   osc1 { wave = square, freq = pitch, level = 0.8, pw = 0.25 }   # narrow
#   osc1 { wave = square, freq = pitch, level = 0.8, pw = 0.1  }   # thin pulse
# Try it on a `tri` osc too: there pw skews the triangle toward a ramp.

lfo1 { wave = tri, rate = 0.25Hz, retrigger = on, phase = 0 }

osc1 { wave = square, freq = pitch, level = 0.8, pw = 0.5 + lfo1 * 0.4 }

filter { type = lp, cutoff = 7kHz, res = 0.1 }
ampEnv { a = 4ms, d = 120ms, s = 1.0, r = 250ms }
master { volume = -9dB }
)";

static const char* k10_Wobble = R"(# Dubstep Wobble - quarter-note LFO wobble, hard drive, triplet delay.

let bipolar  = lfo2
let unipolar = (bipolar + 1) / 2

osc1 { wave = saw,    freq = pitch - 12st,          level = 0.7 }
osc2 { wave = square, freq = pitch - 12st + 4cent,  level = 0.5 }
osc3 { wave = saw,    freq = pitch - 24st,          level = 0.3 }

filter {
    type     = lp,
    cutoff   = 200Hz + unipolar * 2.8kHz,
    res      = 0.7,
    env      = 0.3,
    keytrack = 0.2
}

ampEnv { a = 5ms, d = 200ms, s = 0.9, r = 150ms }
fltEnv { a = 1ms, d = 150ms, s = 0.6, r = 100ms }

lfo2 { wave = sine, rate = 1/4, sync = on, retrigger = on, phase = 0 }

distortion { shape = hard, drive = 12dB, mix = 0.8 }
delay      { time = 1/4t,   sync  = on,  feedback = 0.35, mix = 0.25 }
master     { volume = -3dB }
)";

std::vector<Preset> getFactoryPresets() {
    return {
        { "Warm Pad",        k1_WarmPad,    true },
        { "Acid Bass",       k2_AcidBass,   true },
        { "FM-ish Lead",     k3_FmLead,     true },
        { "Plucked Keys",    k4_Plucked,    true },
        { "Noise Hat",       k5_NoiseHat,   true },
        { "Wind Drone",      k6_WindDrone,  true },
        { "Bell",            k7_Bell,       true },
        { "Synth Brass",     k8_SynthBrass, true },
        { "Chiptune Lead",   k9_Chiptune,   true },
        { "Dubstep Wobble",  k10_Wobble,    true },
        { "PWM Demo",        k11_PwmDemo,   true },
    };
}

// ---------------------------------------------------------------------------
// PresetManager
// ---------------------------------------------------------------------------

PresetManager::PresetManager() {
    factory = getFactoryPresets();
    refresh();
}

juce::File PresetManager::getUserPresetDir() {
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("ProgSynth")
                   .getChildFile("Presets");
    if (!dir.exists()) dir.createDirectory();
    return dir;
}

void PresetManager::refresh() {
    user.clear();

    auto dir = getUserPresetDir();
    juce::Array<juce::File> files;
    dir.findChildFiles(files, juce::File::findFiles, false, "*.patch");

    for (auto& f : files) {
        Preset p;
        p.name      = f.getFileNameWithoutExtension();
        p.script    = f.loadFileAsString();
        p.isFactory = false;
        user.push_back(std::move(p));
    }

    std::sort(user.begin(), user.end(),
              [](const Preset& a, const Preset& b) {
                  return a.name.compareIgnoreCase(b.name) < 0;
              });

    rebuildCache();
}

void PresetManager::rebuildCache() {
    cache.clear();
    cache.reserve(factory.size() + user.size());
    cache.insert(cache.end(), factory.begin(), factory.end());
    cache.insert(cache.end(), user.begin(), user.end());
}

const Preset* PresetManager::find(const juce::String& name) const {
    for (auto& p : cache)
        if (p.name.equalsIgnoreCase(name))
            return &p;
    return nullptr;
}

bool PresetManager::saveUser(const juce::String& name, const juce::String& script) {
    auto trimmed = name.trim();
    if (trimmed.isEmpty()) return false;

    for (auto& p : factory)
        if (p.name.equalsIgnoreCase(trimmed)) return false;

    auto safe = trimmed.replaceCharacters("\\/:*?\"<>|", "_________");

    auto f = getUserPresetDir().getChildFile(safe + ".patch");
    if (!f.replaceWithText(script)) return false;

    refresh();
    return true;
}

} // namespace progsynth
