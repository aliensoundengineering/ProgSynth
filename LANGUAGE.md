# ProgSynth Patch Language — Reference

ProgSynth describes a synthesizer voice as a small declarative program. A *patch* is a list of **blocks** (`osc1`, `filter`, `ampEnv`, …), each of which contains a comma‑separated list of `name = expression` assignments. Top‑level `let` statements introduce reusable named expressions.

This document is a strict reference: every block, every parameter, every unit, every built‑in identifier, every operator, and the rules the compiler enforces.

---

## 1. Lexical structure

### 1.1 Whitespace and comments

Whitespace (spaces, tabs, carriage returns, newlines) is insignificant — it merely separates tokens. Line comments start with `#` and run to the end of the line.

```
# This is a comment.
osc1 { wave = saw }   # trailing comment
```

Block comments are not supported; chain `#` lines.

### 1.2 Identifiers

Identifiers begin with a letter or underscore and continue with letters, digits, or underscores. They are case‑sensitive. `lfo1` and `LFO1` are different identifiers; only `lfo1` is recognised as a built‑in block name.

The single keyword is `let`. All other names — block names (`osc1`, `filter`, …), parameter names (`wave`, `cutoff`, …), enum values (`saw`, `lp`, `on`, …), and built‑in inputs (`pitch`, `velocity`, …) — are ordinary identifiers, distinguished only by where they appear.

### 1.3 Numbers and units

A numeric literal is one or more digits, an optional fractional part, and an optional unit suffix appended without intervening whitespace.

```
4         440       0.5       2.71828
4Hz       2.5kHz    250ms     1.2s
12st      50cent    -6dB      75%
```

Recognised units:

| Suffix | Unit              |
|--------|-------------------|
| `Hz`   | hertz             |
| `kHz`  | kilohertz         |
| `ms`   | millisecond       |
| `s`    | second            |
| `st`   | semitone          |
| `cent` | cent (1/100 st)   |
| `dB`   | decibel           |
| `%`    | percent           |

A numeric literal without a suffix is *unitless* and acquires meaning from the parameter it is assigned to (Section 4).

### 1.4 Sync literals

A *sync literal* describes a musical note value: an integer numerator, a `/`, an integer denominator, and an optional `t` (triplet) or `.` (dotted) suffix.

```
1/4    1/8    1/16
1/4.   1/8.            # dotted
1/4t   1/8t            # triplet
```

Sync literals are valid **only** as the value of `rate` inside an `lfo1` or `lfo2` block. Anywhere else the compiler reports an error.

### 1.5 Operators and punctuation

```
+  -  *  /
=  ,  .
{ } ( )
```

`+ - * /` are binary arithmetic operators (and `-` is also unary). `=` separates a parameter name from its value. `,` separates assignments inside a block. `.` qualifies an identifier (used by the parser for built‑ins; see Section 5.6). `{` and `}` delimit a block. `(` and `)` group an expression.

### 1.6 The grammar

```
program    ::= statement*
statement  ::= block | let
let        ::= "let" identifier "=" expression
block      ::= identifier "{" assignments "}"
assignments::= ε | assignment ("," assignment)*
assignment ::= identifier "=" expression

expression ::= add
add        ::= mul (("+"|"-") mul)*
mul        ::= unary (("*"|"/") unary)*
unary      ::= "-" unary | primary
primary    ::= number
             | sync_literal
             | identifier ("." identifier)?
             | "(" expression ")"
```

Newlines have no syntactic role — assignments are separated by commas, statements by the brace structure.

---

## 2. Top‑level statements

A program is a flat sequence of `let` statements and blocks, in any order. Each block name may appear **at most once** (a second `osc1 { … }` does not extend the first; the second silently replaces individual parameters as it is processed).

### 2.1 `let` bindings

```
let depth = 200Hz
let wobble = lfo1 * depth
filter { cutoff = 1kHz + wobble }
```

A `let` introduces an alias for an expression. References to the alias are resolved by *textual substitution* at compile time, so the alias inherits the type of the surrounding context. A `let` may reference another `let`, but cycles are forbidden — the compiler reports `recursive 'let' binding`.

`let` bindings may use any built‑in identifier (`lfo1`, `velocity`, …) and any unit. They cannot refer to qualified names (`osc1.freq`).

### 2.2 Blocks

| Block name | Purpose                         |
|------------|---------------------------------|
| `osc1`     | Oscillator 1                    |
| `osc2`     | Oscillator 2                    |
| `osc3`     | Oscillator 3                    |
| `filter`   | Voice filter                    |
| `ampEnv`   | Amplitude envelope (ADSR)       |
| `fltEnv`   | Filter envelope (ADSR)          |
| `lfo1`     | Low‑frequency oscillator 1      |
| `lfo2`     | Low‑frequency oscillator 2      |
| `master`   | Master section                  |

Any other block name produces `unknown block 'X'`.

---

## 3. Block parameters

For each block this section lists every parameter, its expected expression kind, and (where relevant) its valid range. Parameters not assigned use the defaults shown in Section 6.

### 3.1 `osc1`, `osc2`, `osc3`

| Parameter | Kind        | Description                                                |
|-----------|-------------|------------------------------------------------------------|
| `wave`    | enum        | `sine`, `tri`, `saw`, `square` (alias `sub` ≡ `square`)    |
| `freq`    | pitch expr  | Pitch context — output is converted to Hz at evaluation    |
| `level`   | level expr  | 0..1 mix into the voice bus (clipped at runtime)           |

```
osc1 { wave = saw, freq = pitch, level = 0.7 }
osc2 { wave = saw, freq = pitch + 7st, level = 0.5 }
osc3 { wave = square, freq = pitch - 12st, level = 0.4 }
```

### 3.2 `filter`

| Parameter  | Kind             | Description                                          |
|------------|------------------|------------------------------------------------------|
| `type`     | enum             | `lp` or `hp`                                         |
| `cutoff`   | frequency expr   | Hz                                                   |
| `res`      | level expr       | 0..1 (resonance)                                     |
| `env`      | signed level     | -1..+1 (amount of `fltEnv` added to the cutoff)      |
| `keytrack` | level expr       | 0..1 (cutoff tracks `pitch`)                         |

```
filter {
    type     = lp,
    cutoff   = 800Hz,
    res      = 0.4,
    env      = 0.6,
    keytrack = 0.25
}
```

### 3.3 `ampEnv`, `fltEnv`

| Parameter | Kind       | Description |
|-----------|------------|-------------|
| `a`       | time expr  | attack      |
| `d`       | time expr  | decay       |
| `s`       | level expr | sustain     |
| `r`       | time expr  | release     |

```
ampEnv { a = 5ms, d = 200ms, s = 0.7, r = 300ms }
fltEnv { a = 1ms, d = 80ms,  s = 0,   r = 80ms  }
```

### 3.4 `lfo1`, `lfo2`

| Parameter   | Kind                   | Description                                                  |
|-------------|------------------------|--------------------------------------------------------------|
| `wave`      | enum                   | `sine`, `tri`, `saw`, `square`                               |
| `rate`      | frequency expr OR sync | Hz, *or* a sync literal like `1/4`                           |
| `sync`      | enum                   | `on` / `off` (whether to honour the sync literal)            |
| `retrigger` | enum                   | `on` / `off` (reset phase on note‑on)                        |
| `phase`     | level expr             | 0..1, initial phase offset                                   |

```
lfo1 { wave = sine, rate = 5Hz, retrigger = on, phase = 0 }
lfo2 { wave = tri,  rate = 1/8, sync = on }
```

If `rate` is a sync literal, the compiler stores the rational rate in `lfo*.syncRate` and the runtime converts it to Hz using the host BPM.

### 3.5 `master`

| Parameter | Kind        | Description                                          |
|-----------|-------------|------------------------------------------------------|
| `volume`  | volume expr | Linear gain 0..∞, accepts unitless / `%` / `dB`      |

```
master { volume = -3dB }
```

---

## 4. Parameter kinds and unit coercion

Every parameter has a *kind* which decides which units are accepted and how a unit‑bearing literal is converted to the runtime canonical unit.

| Kind          | Canonical | Accepted units                              |
|---------------|-----------|---------------------------------------------|
| `Frequency`   | Hz        | unitless, `Hz`, `kHz`                       |
| `Pitch`       | semitone  | unitless, `st`, `cent`, `Hz`, `kHz`         |
| `Time`        | second    | unitless, `s`, `ms`                         |
| `Level`       | unitless  | unitless, `%`                               |
| `SignedLevel` | unitless  | unitless, `%`                               |
| `Volume`      | linear    | unitless, `%`, `dB`                         |

A unit not in the accepted list produces `invalid unit '…' for X parameter`. Unitless literals are always accepted and are treated as the canonical unit.

Notes:

* **Pitch**. The expression is evaluated in semitone space; at the very end the runtime applies `freq = 440 · 2^((midi-69)/12)`. So `pitch + 7st` legally means *seven semitones above the current note*. Hz literals inside a pitch expression are converted on the spot to MIDI semitones via `12·log₂(Hz/440) + 69`.
* **Volume / dB**. Decibels are exponential: `-6dB` evaluates to `10^(-6/20) ≈ 0.501`. The compiler folds the unary minus into the literal *before* the exponential conversion, so `-6dB` and `0dB - 6dB` are not the same: the former is the dB literal `-6`, the latter is the linear value `1.0 - 0.501`.
* **Time**. Negative times are accepted by the compiler; the audio engine clamps them to a small positive epsilon at runtime.
* **Level / SignedLevel**. The compiler does not clamp levels — your patch may overshoot 1.0 or go below -1.0. The audio engine applies whatever saturation each stage requires.

---

## 5. Expressions

### 5.1 Arithmetic

`+ - * /` with conventional precedence: unary minus, then `* /`, then `+ -`. Parentheses override.

```
let detuned = pitch + 7st + 3cent
osc2 { freq = detuned }
filter { cutoff = 200Hz + 800Hz * fltEnv }
```

Division by zero evaluates to `0.0` at runtime — never a NaN.

### 5.2 Unary minus

`-x` negates `x`. Special case: `-N<unit>` where `<unit>` is non‑linear (e.g. `dB`) folds the sign into the literal *before* the unit conversion. `-6dB` is therefore `0.501`, not `-(10^(6/20))`.

### 5.3 Built‑in dynamic inputs

These identifiers refer to per‑sample, per‑voice values supplied by the audio engine:

| Identifier | Range          | Meaning                                              |
|------------|----------------|------------------------------------------------------|
| `pitch`    | semitones (float) | MIDI note plus pitch‑bend                         |
| `note`     | Hz (or st in pitch context) | Hz of the held note; in a `Pitch` parameter the compiler treats it as `pitch` |
| `velocity` | 0..1           | Velocity of the note, normalised                     |
| `gate`     | 0 or 1         | 1 while the key is held, 0 after note‑off            |
| `ampEnv`   | 0..1           | Output of the amp envelope                           |
| `fltEnv`   | 0..1           | Output of the filter envelope                        |
| `lfo1`     | -1..+1         | Output of LFO 1                                      |
| `lfo2`     | -1..+1         | Output of LFO 2                                      |

`ampEnv` and `fltEnv` are valid both as block names *and* as expressions (the latter reads the envelope's current sample). The parser distinguishes by context: a bare `ampEnv` followed by `{` is a block, otherwise it is an input.

The four envelope/LFO inputs and `velocity` are tracked as *modulation sources*. When such an input appears in an expression, the compiler records a *routing* like `lfo1 -> filter.cutoff` for the patch summary.

### 5.4 Constants vs. dynamic expressions

If an expression references no built‑in input, the compiler folds it to a constant at compile time and stores the result in `Expression::constantValue`. Dynamic expressions emit a tiny stack‑machine bytecode (push, load‑input, add/sub/mul/div/neg, midi‑to‑Hz) that the audio engine evaluates per voice per sample.

### 5.5 Enum identifiers

Identifiers used as enum values may *only* appear as the right‑hand side of the assignment to an enum‑kind parameter. Putting `saw`, `lp`, or `on` inside an arithmetic expression is an error:

```
osc1 { wave = saw }            # OK
osc1 { wave = saw + 1 }        # error: waveform name 'saw' not allowed in expression
filter { cutoff = 1kHz * lp }  # error: filter type 'lp' not allowed in expression
```

### 5.6 Qualified names

The grammar supports `identifier.identifier`. In the current language the compiler rejects qualified names inside arbitrary expressions (`osc1.freq` is not a readable value). Qualified names are reserved for future routing extensions.

### 5.7 The expression VM

A compiled expression is a flat array of `Op`s:

```
PushConst i      stack <- constants[i]
LoadInput  k     stack <- inputs[k]
Add Sub Mul Div  binary, popping two and pushing one
Neg              unary
MidiToHz         pop x; push 440 * 2^((x-69)/12)
```

Stack depth is bounded by the AST shape; the runtime stack is fixed at 64 slots. The runtime never allocates and never throws.

---

## 6. Defaults

If a block is omitted, or a parameter is omitted, the following defaults apply:

```
osc1   { wave = saw,    freq = pitch, level = 0.7 }
osc2   { wave = saw,    freq = pitch, level = 0.0 }
osc3   { wave = saw,    freq = pitch, level = 0.0 }
filter { type = lp, cutoff = 2000Hz, res = 0.2, env = 0, keytrack = 0 }
ampEnv { a = 5ms,  d = 200ms, s = 0.7, r = 300ms }
fltEnv { a = 5ms,  d = 200ms, s = 0.7, r = 300ms }
lfo1   { wave = sine, rate = 1Hz, sync = off, retrigger = on, phase = 0 }
lfo2   { wave = sine, rate = 1Hz, sync = off, retrigger = on, phase = 0 }
master { volume = -6dB }
```

A completely empty source is a valid program — it produces a default sawtooth synth at -6 dB.

---

## 7. Diagnostics

The pipeline reports three classes of message:

* **Lex errors** — illegal character (`unexpected character '…'`).
* **Parse errors** — `expected …`, `expected expression`, `expected '{' after block name`, etc.
* **Compile errors** — `unknown identifier`, `unknown parameter`, `invalid unit`, `recursive 'let' binding`, `qualified parameter access not allowed`, `waveform name not allowed in expression`, `sync rate literal only valid as the value of rate=`.

Warnings are non‑fatal. The compiler currently emits one:

* `lfoN is declared but not routed anywhere` — an `lfoN { … }` block exists but no expression in the patch references `lfoN`.

The patch summary also lists every modulation routing the compiler discovered, in source‑order, e.g.:

```
velocity -> osc1.level
lfo1     -> filter.cutoff
fltEnv   -> filter.cutoff
```

---

## 8. Worked example

A complete patch demonstrating most of the language:

```
# A simple "warm pad" patch.

let detune = 7cent
let wobble = lfo1 * 200Hz

# Three sawtooths, slightly detuned, octave‑down sub.
osc1 { wave = saw,    freq = pitch + detune,  level = 0.5 }
osc2 { wave = saw,    freq = pitch - detune,  level = 0.5 }
osc3 { wave = square, freq = pitch - 12st,    level = 0.3 }

# Slow filter that opens with the envelope and gets nudged by LFO1.
filter {
    type     = lp,
    cutoff   = 600Hz + 1500Hz * fltEnv + wobble,
    res      = 0.3,
    env      = 0.8,
    keytrack = 0.4
}

# Slow attack and release for the pad character.
ampEnv { a = 200ms, d = 400ms, s = 0.8,  r = 600ms }
fltEnv { a = 50ms,  d = 800ms, s = 0.4,  r = 600ms }

# A sine LFO modulating cutoff via `wobble`.
lfo1 { wave = sine, rate = 4Hz, retrigger = off }

master { volume = -3dB }
```

Compiled, this produces three oscillators, a 12 dB/octave low‑pass filter, two ADSR envelopes, one LFO, and three routings:

```
fltEnv -> filter.cutoff
lfo1   -> filter.cutoff
```

That is the entire language.
