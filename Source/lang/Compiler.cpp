#include "Compiler.h"

#include <cmath>
#include <sstream>

namespace progsynth {

namespace {

struct Ctx {
    std::vector<CompileError>* errors = nullptr;
    std::map<std::string, const Expr*> lets;
    std::set<std::string> expanding;
    CompiledPatch* patch = nullptr;
};

void err(Ctx& ctx, const std::string& msg, int line, int col) {
    if (!ctx.errors) return;
    CompileError e; e.message = msg; e.line = line; e.col = col;
    ctx.errors->push_back(e);
}

const char* inputNameOf(int idx) {
    switch (idx) {
        case ExprInputs::Pitch:    return "pitch";
        case ExprInputs::Velocity: return "velocity";
        case ExprInputs::Gate:     return "gate";
        case ExprInputs::AmpEnv:   return "ampEnv";
        case ExprInputs::FltEnv:   return "fltEnv";
        case ExprInputs::Lfo1:     return "lfo1";
        case ExprInputs::Lfo2:     return "lfo2";
        case ExprInputs::NoteHz:   return "note";
    }
    return "?";
}

bool isModulationSource(int idx) {
    return idx == ExprInputs::Velocity
        || idx == ExprInputs::AmpEnv
        || idx == ExprInputs::FltEnv
        || idx == ExprInputs::Lfo1
        || idx == ExprInputs::Lfo2;
}

// Convert a numeric literal to canonical units of the destination kind.
// Returns false on incompatible unit (and emits error).
bool convertLiteral(double v, Unit u, ParamKind kind,
                    Ctx& ctx, const Expr& at, double& out) {
    switch (kind) {
        case ParamKind::Frequency:
            switch (u) {
                case Unit::None: case Unit::Hz: out = v; return true;
                case Unit::KHz:                 out = v * 1000.0; return true;
                default: break;
            }
            err(ctx, std::string("invalid unit '") + unitName(u) +
                     "' for frequency parameter (expected Hz or kHz)",
                at.line, at.col);
            out = v; return false;

        case ParamKind::Pitch:
            switch (u) {
                case Unit::None: case Unit::St: out = v; return true;
                case Unit::Cent:                out = v / 100.0; return true;
                case Unit::Hz:
                    if (v > 0) { out = 12.0 * std::log2(v / 440.0) + 69.0; return true; }
                    out = 0; return true;
                case Unit::KHz:
                    if (v > 0) { out = 12.0 * std::log2(v * 1000.0 / 440.0) + 69.0; return true; }
                    out = 0; return true;
                default: break;
            }
            err(ctx, std::string("invalid unit '") + unitName(u) +
                     "' for pitch parameter (expected st, cent, Hz, kHz)",
                at.line, at.col);
            out = v; return false;

        case ParamKind::Time:
            switch (u) {
                case Unit::None: case Unit::S: out = v; return true;
                case Unit::Ms:                 out = v * 0.001; return true;
                default: break;
            }
            err(ctx, std::string("invalid unit '") + unitName(u) +
                     "' for time parameter (expected s or ms)",
                at.line, at.col);
            out = v; return false;

        case ParamKind::Level:
        case ParamKind::SignedLevel:
            switch (u) {
                case Unit::None:    out = v; return true;
                case Unit::Percent: out = v / 100.0; return true;
                default: break;
            }
            err(ctx, std::string("invalid unit '") + unitName(u) +
                     "' for level parameter (expected unitless or %)",
                at.line, at.col);
            out = v; return false;

        case ParamKind::Volume:
            switch (u) {
                case Unit::None:    out = v; return true;
                case Unit::Percent: out = v / 100.0; return true;
                case Unit::Db:      out = std::pow(10.0, v / 20.0); return true;
                default: break;
            }
            err(ctx, std::string("invalid unit '") + unitName(u) +
                     "' for volume parameter (expected unitless, %, or dB)",
                at.line, at.col);
            out = v; return false;
    }
    out = v;
    return true;
}

void emit(const Expr& e, Expression& out, ParamKind kind, Ctx& ctx);

// Resolve an identifier inside an expression and emit corresponding ops.
// Returns true on success.
bool emitIdentifier(const Expr& e, Expression& out, ParamKind kind, Ctx& ctx) {
    const std::string& n = e.name;

    // Wave names, filter type, on/off are NOT valid inside an arithmetic expression.
    if (n == "sine" || n == "tri" || n == "saw" || n == "square") {
        err(ctx, "waveform name '" + n + "' is not allowed in an expression "
                  "(use it as the value of `wave=`)", e.line, e.col);
        return false;
    }
    if (n == "lp" || n == "hp") {
        err(ctx, "filter type '" + n + "' is not allowed in an expression",
            e.line, e.col);
        return false;
    }
    if (n == "on" || n == "off") {
        err(ctx, "'" + n + "' is not allowed in an expression",
            e.line, e.col);
        return false;
    }

    // Built-in dynamic identifiers.
    int inputIdx = -1;
    if      (n == "pitch")    inputIdx = ExprInputs::Pitch;
    else if (n == "velocity") inputIdx = ExprInputs::Velocity;
    else if (n == "gate")     inputIdx = ExprInputs::Gate;
    else if (n == "ampEnv")   inputIdx = ExprInputs::AmpEnv;
    else if (n == "fltEnv")   inputIdx = ExprInputs::FltEnv;
    else if (n == "lfo1")     inputIdx = ExprInputs::Lfo1;
    else if (n == "lfo2")     inputIdx = ExprInputs::Lfo2;
    else if (n == "note") {
        // In pitch context, 'note' is promoted to semitones (alias of pitch).
        // Otherwise it's the note frequency in Hz.
        inputIdx = (kind == ParamKind::Pitch) ? ExprInputs::Pitch
                                              : ExprInputs::NoteHz;
    }

    if (inputIdx >= 0) {
        Op op; op.code = Op::Code::LoadInput; op.arg = inputIdx;
        out.ops.push_back(op);
        out.inputMask |= (1u << inputIdx);
        return true;
    }

    // let-binding lookup
    auto it = ctx.lets.find(n);
    if (it != ctx.lets.end()) {
        if (ctx.expanding.count(n)) {
            err(ctx, "recursive 'let' binding '" + n + "'", e.line, e.col);
            return false;
        }
        ctx.expanding.insert(n);
        emit(*it->second, out, kind, ctx);
        ctx.expanding.erase(n);
        return true;
    }

    err(ctx, "unknown identifier '" + n + "'", e.line, e.col);
    return false;
}

void emit(const Expr& e, Expression& out, ParamKind kind, Ctx& ctx) {
    switch (e.kind) {
        case ExprKind::Number: {
            double v = 0.0;
            convertLiteral(e.number, e.unit, kind, ctx, e, v);
            out.constants.push_back(v);
            Op op; op.code = Op::Code::PushConst;
            op.arg = (int)out.constants.size() - 1;
            out.ops.push_back(op);
            break;
        }
        case ExprKind::Sync: {
            err(ctx, "sync rate literal is only valid as the value of `rate=` "
                     "in an LFO block (or `time=` in a delay block)", e.line, e.col);
            // emit 0 so VM stack stays consistent
            out.constants.push_back(0.0);
            Op op; op.code = Op::Code::PushConst;
            op.arg = (int)out.constants.size() - 1;
            out.ops.push_back(op);
            break;
        }
        case ExprKind::Identifier: {
            if (!emitIdentifier(e, out, kind, ctx)) {
                out.constants.push_back(0.0);
                Op op; op.code = Op::Code::PushConst;
                op.arg = (int)out.constants.size() - 1;
                out.ops.push_back(op);
            }
            break;
        }
        case ExprKind::Qualified: {
            err(ctx, "qualified parameter access (" + e.name + "." + e.subName +
                     ") is not allowed inside expressions",
                e.line, e.col);
            out.constants.push_back(0.0);
            Op op; op.code = Op::Code::PushConst;
            op.arg = (int)out.constants.size() - 1;
            out.ops.push_back(op);
            break;
        }
        case ExprKind::Unary: {
            // Special case: a unary minus directly in front of a numeric literal
            // must fold its sign into the value BEFORE unit conversion, otherwise
            // non-linear units (dB) give wrong results: -6 dB should mean
            // 10^(-6/20) = 0.501, not -(10^(6/20)) = -1.995.
            if (e.op == '-' && e.lhs && e.lhs->kind == ExprKind::Number) {
                double v = 0.0;
                convertLiteral(-(e.lhs->number), e.lhs->unit, kind, ctx, *e.lhs, v);
                out.constants.push_back(v);
                Op op; op.code = Op::Code::PushConst;
                op.arg = (int)out.constants.size() - 1;
                out.ops.push_back(op);
                break;
            }
            if (e.lhs) emit(*e.lhs, out, kind, ctx);
            Op op; op.code = Op::Code::Neg; out.ops.push_back(op);
            break;
        }
        case ExprKind::Binary: {
            if (e.lhs) emit(*e.lhs, out, kind, ctx);
            if (e.rhs) emit(*e.rhs, out, kind, ctx);
            Op op;
            switch (e.op) {
                case '+': op.code = Op::Code::Add; break;
                case '-': op.code = Op::Code::Sub; break;
                case '*': op.code = Op::Code::Mul; break;
                case '/': op.code = Op::Code::Div; break;
                default:  op.code = Op::Code::Add; break;
            }
            out.ops.push_back(op);
            break;
        }
    }
}

// Compile an expression for a given destination kind. For pitch, append a
// final MidiToHz op so the runtime value is in Hz.
Expression compileExpr(const Expr& e, ParamKind kind, Ctx& ctx) {
    Expression out;
    emit(e, out, kind, ctx);
    if (kind == ParamKind::Pitch) {
        Op op; op.code = Op::Code::MidiToHz;
        out.ops.push_back(op);
    }
    // try to fold to a constant if no inputs were referenced
    if (out.inputMask == 0) {
        // Evaluate WHILE isConstant is still false, so the VM actually runs
        // the bytecode rather than returning the default constantValue.
        ExprInputs zero{};
        const double folded = out.evaluate(zero);
        out.constantValue = folded;
        out.isConstant    = true;
    } else {
        out.isConstant = false;
    }
    return out;
}

// Helpers for "static" assignments (wave, filter type, on/off, sync literal).
bool readWave(const Expr& e, WaveKind& out, Ctx& ctx) {
    if (e.kind != ExprKind::Identifier) {
        err(ctx, "expected waveform name (sine, tri, saw, square)", e.line, e.col);
        return false;
    }
    if      (e.name == "sine")   out = WaveKind::Sine;
    else if (e.name == "tri")    out = WaveKind::Tri;
    else if (e.name == "saw")    out = WaveKind::Saw;
    else if (e.name == "square") out = WaveKind::Square;
    else if (e.name == "sub")    { out = WaveKind::Square; /* alias for sub-osc */ }
    else {
        err(ctx, "expected waveform name, got '" + e.name + "'", e.line, e.col);
        return false;
    }
    return true;
}

bool readFilterType(const Expr& e, FilterKind& out, Ctx& ctx) {
    if (e.kind != ExprKind::Identifier) {
        err(ctx, "expected filter type (lp, hp)", e.line, e.col);
        return false;
    }
    if      (e.name == "lp") out = FilterKind::LP;
    else if (e.name == "hp") out = FilterKind::HP;
    else {
        err(ctx, "expected filter type, got '" + e.name + "'", e.line, e.col);
        return false;
    }
    return true;
}

bool readOnOff(const Expr& e, bool& out, Ctx& ctx) {
    if (e.kind != ExprKind::Identifier) {
        err(ctx, "expected 'on' or 'off'", e.line, e.col);
        return false;
    }
    if      (e.name == "on")  out = true;
    else if (e.name == "off") out = false;
    else {
        err(ctx, "expected 'on' or 'off', got '" + e.name + "'", e.line, e.col);
        return false;
    }
    return true;
}

void recordRoutings(const Expression& expr, const std::string& slotPath,
                    CompiledPatch& patch) {
    if (expr.isConstant) return;
    for (int i = 0; i < ExprInputs::Count; ++i) {
        if ((expr.inputMask & (1u << i)) && isModulationSource(i)) {
            std::string r = std::string(inputNameOf(i)) + " -> " + slotPath;
            patch.routings.push_back(r);
            ++patch.activeRoutings;
        }
    }
}

void compileOscBlock(const Block& b, OscPatch& osc, const std::string& name,
                     Ctx& ctx, CompiledPatch& patch) {
    for (const auto& a : b.assignments) {
        if (!a.value) continue;
        if (a.name == "wave") {
            readWave(*a.value, osc.wave, ctx);
        } else if (a.name == "freq") {
            osc.freq = compileExpr(*a.value, ParamKind::Pitch, ctx);
            recordRoutings(osc.freq, name + ".freq", patch);
        } else if (a.name == "level") {
            osc.level = compileExpr(*a.value, ParamKind::Level, ctx);
            recordRoutings(osc.level, name + ".level", patch);
        } else {
            err(ctx, "unknown parameter '" + a.name + "' in " + name,
                a.line, a.col);
        }
    }
}

void compileFilterBlock(const Block& b, FilterPatch& f, Ctx& ctx,
                        CompiledPatch& patch) {
    for (const auto& a : b.assignments) {
        if (!a.value) continue;
        if (a.name == "type") {
            readFilterType(*a.value, f.type, ctx);
        } else if (a.name == "cutoff") {
            f.cutoff = compileExpr(*a.value, ParamKind::Frequency, ctx);
            recordRoutings(f.cutoff, "filter.cutoff", patch);
        } else if (a.name == "res") {
            f.res = compileExpr(*a.value, ParamKind::Level, ctx);
            recordRoutings(f.res, "filter.res", patch);
        } else if (a.name == "env") {
            f.env = compileExpr(*a.value, ParamKind::SignedLevel, ctx);
            recordRoutings(f.env, "filter.env", patch);
        } else if (a.name == "keytrack") {
            f.keytrack = compileExpr(*a.value, ParamKind::Level, ctx);
            recordRoutings(f.keytrack, "filter.keytrack", patch);
        } else {
            err(ctx, "unknown parameter '" + a.name + "' in filter",
                a.line, a.col);
        }
    }
}

void compileEnvBlock(const Block& b, EnvPatch& env, const std::string& name,
                     Ctx& ctx, CompiledPatch& patch) {
    for (const auto& a : b.assignments) {
        if (!a.value) continue;
        if (a.name == "a") {
            env.a = compileExpr(*a.value, ParamKind::Time, ctx);
            recordRoutings(env.a, name + ".a", patch);
        } else if (a.name == "d") {
            env.d = compileExpr(*a.value, ParamKind::Time, ctx);
            recordRoutings(env.d, name + ".d", patch);
        } else if (a.name == "s") {
            env.s = compileExpr(*a.value, ParamKind::Level, ctx);
            recordRoutings(env.s, name + ".s", patch);
        } else if (a.name == "r") {
            env.r = compileExpr(*a.value, ParamKind::Time, ctx);
            recordRoutings(env.r, name + ".r", patch);
        } else {
            err(ctx, "unknown parameter '" + a.name + "' in " + name,
                a.line, a.col);
        }
    }
}

void compileLfoBlock(const Block& b, LfoPatch& lfo, const std::string& name,
                     Ctx& ctx, CompiledPatch& patch) {
    for (const auto& a : b.assignments) {
        if (!a.value) continue;
        if (a.name == "wave") {
            readWave(*a.value, lfo.wave, ctx);
        } else if (a.name == "rate") {
            if (a.value->kind == ExprKind::Sync) {
                lfo.syncRate.num     = a.value->syncNum;
                lfo.syncRate.den     = a.value->syncDen;
                lfo.syncRate.dotted  = a.value->syncDotted;
                lfo.syncRate.triplet = a.value->syncTriplet;
                lfo.syncRate.valid   = true;
                // a placeholder constant rate; real Hz will come from sync
                lfo.rate.isConstant = true;
                lfo.rate.constantValue = 1.0;
            } else {
                lfo.rate = compileExpr(*a.value, ParamKind::Frequency, ctx);
                recordRoutings(lfo.rate, name + ".rate", patch);
            }
        } else if (a.name == "sync") {
            readOnOff(*a.value, lfo.sync, ctx);
        } else if (a.name == "retrigger") {
            readOnOff(*a.value, lfo.retrigger, ctx);
        } else if (a.name == "phase") {
            lfo.phase = compileExpr(*a.value, ParamKind::Level, ctx);
            recordRoutings(lfo.phase, name + ".phase", patch);
        } else {
            err(ctx, "unknown parameter '" + a.name + "' in " + name,
                a.line, a.col);
        }
    }
}

bool readDistortionShape(const Expr& e, DistortionShape& out, Ctx& ctx) {
    if (e.kind != ExprKind::Identifier) {
        err(ctx, "expected distortion shape ('soft' or 'hard')", e.line, e.col);
        return false;
    }
    if      (e.name == "soft") out = DistortionShape::Soft;
    else if (e.name == "hard") out = DistortionShape::Hard;
    else {
        err(ctx, "expected distortion shape ('soft' or 'hard'), got '" +
                 e.name + "'", e.line, e.col);
        return false;
    }
    return true;
}

void compileReverbBlock(const Block& b, CompiledPatch& patch, Ctx& ctx) {
    auto& fx = patch.reverb;
    fx.enabled = true;
    for (const auto& a : b.assignments) {
        if (!a.value) continue;
        if      (a.name == "mix")     { fx.mix     = compileExpr(*a.value, ParamKind::Level, ctx);
                                        recordRoutings(fx.mix,     "reverb.mix",     patch); }
        else if (a.name == "size")    { fx.size    = compileExpr(*a.value, ParamKind::Level, ctx);
                                        recordRoutings(fx.size,    "reverb.size",    patch); }
        else if (a.name == "damping") { fx.damping = compileExpr(*a.value, ParamKind::Level, ctx);
                                        recordRoutings(fx.damping, "reverb.damping", patch); }
        else if (a.name == "width")   { fx.width   = compileExpr(*a.value, ParamKind::Level, ctx);
                                        recordRoutings(fx.width,   "reverb.width",   patch); }
        else err(ctx, "unknown parameter '" + a.name + "' in reverb", a.line, a.col);
    }
}

void compileDelayBlock(const Block& b, CompiledPatch& patch, Ctx& ctx) {
    auto& fx = patch.delay;
    fx.enabled = true;
    for (const auto& a : b.assignments) {
        if (!a.value) continue;
        if (a.name == "time") {
            if (a.value->kind == ExprKind::Sync) {
                fx.syncRate.num     = a.value->syncNum;
                fx.syncRate.den     = a.value->syncDen;
                fx.syncRate.dotted  = a.value->syncDotted;
                fx.syncRate.triplet = a.value->syncTriplet;
                fx.syncRate.valid   = true;
                fx.time.isConstant    = true;
                fx.time.constantValue = 0.25;   // safe fallback (s)
            } else {
                fx.time = compileExpr(*a.value, ParamKind::Time, ctx);
                recordRoutings(fx.time, "delay.time", patch);
            }
        }
        else if (a.name == "sync")     readOnOff(*a.value, fx.sync, ctx);
        else if (a.name == "feedback") { fx.feedback = compileExpr(*a.value, ParamKind::Level, ctx);
                                         recordRoutings(fx.feedback, "delay.feedback", patch); }
        else if (a.name == "mix")      { fx.mix      = compileExpr(*a.value, ParamKind::Level, ctx);
                                         recordRoutings(fx.mix,      "delay.mix",      patch); }
        else err(ctx, "unknown parameter '" + a.name + "' in delay", a.line, a.col);
    }
}

void compileChorusBlock(const Block& b, CompiledPatch& patch, Ctx& ctx) {
    auto& fx = patch.chorus;
    fx.enabled = true;
    for (const auto& a : b.assignments) {
        if (!a.value) continue;
        if      (a.name == "rate")        { fx.rate        = compileExpr(*a.value, ParamKind::Frequency,   ctx);
                                            recordRoutings(fx.rate,        "chorus.rate",        patch); }
        else if (a.name == "depth")       { fx.depth       = compileExpr(*a.value, ParamKind::Level,       ctx);
                                            recordRoutings(fx.depth,       "chorus.depth",       patch); }
        else if (a.name == "centreDelay") { fx.centreDelay = compileExpr(*a.value, ParamKind::Time,        ctx);
                                            recordRoutings(fx.centreDelay, "chorus.centreDelay", patch); }
        else if (a.name == "feedback")    { fx.feedback    = compileExpr(*a.value, ParamKind::SignedLevel, ctx);
                                            recordRoutings(fx.feedback,    "chorus.feedback",    patch); }
        else if (a.name == "mix")         { fx.mix         = compileExpr(*a.value, ParamKind::Level,       ctx);
                                            recordRoutings(fx.mix,         "chorus.mix",         patch); }
        else err(ctx, "unknown parameter '" + a.name + "' in chorus", a.line, a.col);
    }
}

void compileFlangerBlock(const Block& b, CompiledPatch& patch, Ctx& ctx) {
    auto& fx = patch.flanger;
    fx.enabled = true;
    for (const auto& a : b.assignments) {
        if (!a.value) continue;
        if      (a.name == "rate")        { fx.rate        = compileExpr(*a.value, ParamKind::Frequency,   ctx);
                                            recordRoutings(fx.rate,        "flanger.rate",        patch); }
        else if (a.name == "depth")       { fx.depth       = compileExpr(*a.value, ParamKind::Level,       ctx);
                                            recordRoutings(fx.depth,       "flanger.depth",       patch); }
        else if (a.name == "centreDelay") { fx.centreDelay = compileExpr(*a.value, ParamKind::Time,        ctx);
                                            recordRoutings(fx.centreDelay, "flanger.centreDelay", patch); }
        else if (a.name == "feedback")    { fx.feedback    = compileExpr(*a.value, ParamKind::SignedLevel, ctx);
                                            recordRoutings(fx.feedback,    "flanger.feedback",    patch); }
        else if (a.name == "mix")         { fx.mix         = compileExpr(*a.value, ParamKind::Level,       ctx);
                                            recordRoutings(fx.mix,         "flanger.mix",         patch); }
        else err(ctx, "unknown parameter '" + a.name + "' in flanger", a.line, a.col);
    }
}

void compileCompressorBlock(const Block& b, CompiledPatch& patch, Ctx& ctx) {
    auto& fx = patch.compressor;
    fx.enabled = true;
    for (const auto& a : b.assignments) {
        if (!a.value) continue;
        if      (a.name == "threshold") { fx.threshold = compileExpr(*a.value, ParamKind::Volume, ctx);
                                          recordRoutings(fx.threshold, "compressor.threshold", patch); }
        else if (a.name == "ratio")     { fx.ratio     = compileExpr(*a.value, ParamKind::Level,  ctx);
                                          recordRoutings(fx.ratio,     "compressor.ratio",     patch); }
        else if (a.name == "attack")    { fx.attack    = compileExpr(*a.value, ParamKind::Time,   ctx);
                                          recordRoutings(fx.attack,    "compressor.attack",    patch); }
        else if (a.name == "release")   { fx.release   = compileExpr(*a.value, ParamKind::Time,   ctx);
                                          recordRoutings(fx.release,   "compressor.release",   patch); }
        else if (a.name == "makeup")    { fx.makeup    = compileExpr(*a.value, ParamKind::Volume, ctx);
                                          recordRoutings(fx.makeup,    "compressor.makeup",    patch); }
        else err(ctx, "unknown parameter '" + a.name + "' in compressor", a.line, a.col);
    }
}

void compileDistortionBlock(const Block& b, CompiledPatch& patch, Ctx& ctx) {
    auto& fx = patch.distortion;
    fx.enabled = true;
    for (const auto& a : b.assignments) {
        if (!a.value) continue;
        if      (a.name == "shape") readDistortionShape(*a.value, fx.shape, ctx);
        else if (a.name == "drive") { fx.drive = compileExpr(*a.value, ParamKind::Volume, ctx);
                                      recordRoutings(fx.drive, "distortion.drive", patch); }
        else if (a.name == "mix")   { fx.mix   = compileExpr(*a.value, ParamKind::Level,  ctx);
                                      recordRoutings(fx.mix,   "distortion.mix",   patch); }
        else err(ctx, "unknown parameter '" + a.name + "' in distortion", a.line, a.col);
    }
}

void compileEqBlock(const Block& b, CompiledPatch& patch, Ctx& ctx) {
    auto& fx = patch.eq;
    fx.enabled = true;
    for (const auto& a : b.assignments) {
        if (!a.value) continue;
        if      (a.name == "lowFreq")  { fx.lowFreq  = compileExpr(*a.value, ParamKind::Frequency, ctx);
                                         recordRoutings(fx.lowFreq,  "eq.lowFreq",  patch); }
        else if (a.name == "lowGain")  { fx.lowGain  = compileExpr(*a.value, ParamKind::Volume,    ctx);
                                         recordRoutings(fx.lowGain,  "eq.lowGain",  patch); }
        else if (a.name == "midFreq")  { fx.midFreq  = compileExpr(*a.value, ParamKind::Frequency, ctx);
                                         recordRoutings(fx.midFreq,  "eq.midFreq",  patch); }
        else if (a.name == "midQ")     { fx.midQ     = compileExpr(*a.value, ParamKind::Level,     ctx);
                                         recordRoutings(fx.midQ,     "eq.midQ",     patch); }
        else if (a.name == "midGain")  { fx.midGain  = compileExpr(*a.value, ParamKind::Volume,    ctx);
                                         recordRoutings(fx.midGain,  "eq.midGain",  patch); }
        else if (a.name == "highFreq") { fx.highFreq = compileExpr(*a.value, ParamKind::Frequency, ctx);
                                         recordRoutings(fx.highFreq, "eq.highFreq", patch); }
        else if (a.name == "highGain") { fx.highGain = compileExpr(*a.value, ParamKind::Volume,    ctx);
                                         recordRoutings(fx.highGain, "eq.highGain", patch); }
        else err(ctx, "unknown parameter '" + a.name + "' in eq", a.line, a.col);
    }
}

void compileMasterBlock(const Block& b, CompiledPatch& patch, Ctx& ctx) {
    for (const auto& a : b.assignments) {
        if (!a.value) continue;
        if (a.name == "volume") {
            patch.masterVolume = compileExpr(*a.value, ParamKind::Volume, ctx);
            recordRoutings(patch.masterVolume, "master.volume", patch);
        } else {
            err(ctx, "unknown parameter '" + a.name + "' in master",
                a.line, a.col);
        }
    }
}

// Sensible defaults so an empty patch still produces sound.
void initDefaults(CompiledPatch& p) {
    auto constE = [](double v) {
        Expression e; e.isConstant = true; e.constantValue = v; return e;
    };
    auto pitchE = []() {
        // freq = midiToHz(pitch)
        Expression e;
        Op load; load.code = Op::Code::LoadInput; load.arg = ExprInputs::Pitch;
        e.ops.push_back(load);
        Op m2h; m2h.code = Op::Code::MidiToHz; e.ops.push_back(m2h);
        e.inputMask = (1u << ExprInputs::Pitch);
        e.isConstant = false;
        return e;
    };

    p.osc1.wave = WaveKind::Saw; p.osc1.freq = pitchE(); p.osc1.level = constE(0.7);
    p.osc2.wave = WaveKind::Saw; p.osc2.freq = pitchE(); p.osc2.level = constE(0.0);
    p.osc3.wave = WaveKind::Saw; p.osc3.freq = pitchE(); p.osc3.level = constE(0.0);

    p.filter.type     = FilterKind::LP;
    p.filter.cutoff   = constE(2000.0);
    p.filter.res      = constE(0.2);
    p.filter.env      = constE(0.0);
    p.filter.keytrack = constE(0.0);

    p.ampEnv.a = constE(0.005); p.ampEnv.d = constE(0.2);
    p.ampEnv.s = constE(0.7);   p.ampEnv.r = constE(0.3);
    p.fltEnv.a = constE(0.005); p.fltEnv.d = constE(0.2);
    p.fltEnv.s = constE(0.7);   p.fltEnv.r = constE(0.3);

    p.lfo1.wave = WaveKind::Sine; p.lfo1.rate = constE(1.0);
    p.lfo1.phase = constE(0.0);
    p.lfo2.wave = WaveKind::Sine; p.lfo2.rate = constE(1.0);
    p.lfo2.phase = constE(0.0);

    p.masterVolume = constE(std::pow(10.0, -6.0/20.0));  // -6 dB

    // Effect-block defaults. `enabled = false` keeps each FX bypassed unless
    // the user actually declares the block; the values below are only used
    // once enabled, to fill in any parameters the user omitted.
    p.reverb.mix     = constE(0.3);
    p.reverb.size    = constE(0.5);
    p.reverb.damping = constE(0.5);
    p.reverb.width   = constE(1.0);

    p.delay.time     = constE(0.25);
    p.delay.feedback = constE(0.4);
    p.delay.mix      = constE(0.3);

    p.chorus.rate        = constE(1.0);
    p.chorus.depth       = constE(0.25);
    p.chorus.centreDelay = constE(0.007);   // 7 ms
    p.chorus.feedback    = constE(0.0);
    p.chorus.mix         = constE(0.5);

    p.flanger.rate        = constE(0.5);
    p.flanger.depth       = constE(0.6);
    p.flanger.centreDelay = constE(0.002);  // 2 ms
    p.flanger.feedback    = constE(0.5);
    p.flanger.mix         = constE(0.5);

    p.compressor.threshold = constE(std::pow(10.0, -12.0/20.0));   // -12 dB
    p.compressor.ratio     = constE(4.0);
    p.compressor.attack    = constE(0.005);
    p.compressor.release   = constE(0.1);
    p.compressor.makeup    = constE(1.0);

    p.distortion.drive = constE(1.0);   // 0 dB
    p.distortion.mix   = constE(1.0);

    p.eq.lowFreq  = constE(200.0);
    p.eq.lowGain  = constE(1.0);
    p.eq.midFreq  = constE(1000.0);
    p.eq.midQ     = constE(0.7);
    p.eq.midGain  = constE(1.0);
    p.eq.highFreq = constE(4000.0);
    p.eq.highGain = constE(1.0);
}

} // anonymous namespace

CompiledPatch Compiler::compile(const Program& program,
                                std::vector<CompileError>& errors) {
    CompiledPatch patch;
    initDefaults(patch);

    Ctx ctx;
    ctx.errors = &errors;
    ctx.patch  = &patch;

    // First pass: collect let bindings (so order doesn't matter).
    for (const auto& s : program.stmts) {
        if (s.kind == StmtKind::Let) {
            if (s.let.value)
                ctx.lets[s.let.name] = s.let.value.get();
        }
    }

    // Second pass: compile blocks.
    for (const auto& s : program.stmts) {
        if (s.kind != StmtKind::Block) continue;
        const Block& b = s.block;
        if      (b.name == "osc1")   compileOscBlock(b, patch.osc1, "osc1", ctx, patch);
        else if (b.name == "osc2")   compileOscBlock(b, patch.osc2, "osc2", ctx, patch);
        else if (b.name == "osc3")   compileOscBlock(b, patch.osc3, "osc3", ctx, patch);
        else if (b.name == "filter") compileFilterBlock(b, patch.filter, ctx, patch);
        else if (b.name == "ampEnv") compileEnvBlock(b, patch.ampEnv, "ampEnv", ctx, patch);
        else if (b.name == "fltEnv") compileEnvBlock(b, patch.fltEnv, "fltEnv", ctx, patch);
        else if (b.name == "lfo1")   compileLfoBlock(b, patch.lfo1, "lfo1", ctx, patch);
        else if (b.name == "lfo2")   compileLfoBlock(b, patch.lfo2, "lfo2", ctx, patch);
        else if (b.name == "master")     compileMasterBlock(b, patch, ctx);
        else if (b.name == "reverb")     compileReverbBlock(b, patch, ctx);
        else if (b.name == "delay")      compileDelayBlock(b, patch, ctx);
        else if (b.name == "chorus")     compileChorusBlock(b, patch, ctx);
        else if (b.name == "flanger")    compileFlangerBlock(b, patch, ctx);
        else if (b.name == "compressor") compileCompressorBlock(b, patch, ctx);
        else if (b.name == "distortion") compileDistortionBlock(b, patch, ctx);
        else if (b.name == "eq")         compileEqBlock(b, patch, ctx);
        else err(ctx, "unknown block '" + b.name + "'", b.line, b.col);
    }

    // Warnings: declared but unused LFOs.
    auto usesLfo = [&](int idx) {
        auto check = [idx](const Expression& e) { return (e.inputMask & (1u<<idx)) != 0; };
        return check(patch.osc1.freq) || check(patch.osc1.level)
            || check(patch.osc2.freq) || check(patch.osc2.level)
            || check(patch.osc3.freq) || check(patch.osc3.level)
            || check(patch.filter.cutoff) || check(patch.filter.res)
            || check(patch.filter.env) || check(patch.filter.keytrack)
            || check(patch.ampEnv.a) || check(patch.ampEnv.d)
            || check(patch.ampEnv.s) || check(patch.ampEnv.r)
            || check(patch.fltEnv.a) || check(patch.fltEnv.d)
            || check(patch.fltEnv.s) || check(patch.fltEnv.r)
            || check(patch.masterVolume)
            || check(patch.reverb.mix) || check(patch.reverb.size)
            || check(patch.reverb.damping) || check(patch.reverb.width)
            || check(patch.delay.time) || check(patch.delay.feedback)
            || check(patch.delay.mix)
            || check(patch.chorus.rate) || check(patch.chorus.depth)
            || check(patch.chorus.centreDelay) || check(patch.chorus.feedback)
            || check(patch.chorus.mix)
            || check(patch.flanger.rate) || check(patch.flanger.depth)
            || check(patch.flanger.centreDelay) || check(patch.flanger.feedback)
            || check(patch.flanger.mix)
            || check(patch.compressor.threshold) || check(patch.compressor.ratio)
            || check(patch.compressor.attack) || check(patch.compressor.release)
            || check(patch.compressor.makeup)
            || check(patch.distortion.drive) || check(patch.distortion.mix)
            || check(patch.eq.lowFreq) || check(patch.eq.lowGain)
            || check(patch.eq.midFreq) || check(patch.eq.midQ)
            || check(patch.eq.midGain)
            || check(patch.eq.highFreq) || check(patch.eq.highGain);
    };
    bool lfo1Declared = false, lfo2Declared = false;
    for (const auto& s : program.stmts) {
        if (s.kind == StmtKind::Block) {
            if (s.block.name == "lfo1") lfo1Declared = true;
            if (s.block.name == "lfo2") lfo2Declared = true;
        }
    }
    if (lfo1Declared && !usesLfo(ExprInputs::Lfo1))
        patch.warnings.push_back("lfo1 is declared but not routed anywhere");
    if (lfo2Declared && !usesLfo(ExprInputs::Lfo2))
        patch.warnings.push_back("lfo2 is declared but not routed anywhere");

    return patch;
}

} // namespace progsynth
