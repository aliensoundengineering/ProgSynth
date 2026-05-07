#pragma once

#include <vector>
#include <cstdint>
#include <cmath>

namespace progsynth {

// Per-voice inputs available to expressions.
// Index order matches the LoadInput opcode argument.
struct ExprInputs {
    enum : int {
        Pitch    = 0,   // semitones (MIDI float)
        Velocity = 1,   // 0..1
        Gate     = 2,   // 0/1
        AmpEnv   = 3,   // 0..1
        FltEnv   = 4,   // 0..1
        Lfo1     = 5,   // -1..+1
        Lfo2     = 6,   // -1..+1
        NoteHz   = 7,   // semitone -> Hz of the held note
        Count    = 8
    };
    double v[Count] = {0,0,0,0,0,0,0,0};
};

struct Op {
    enum class Code : uint8_t {
        PushConst,
        LoadInput,
        Add, Sub, Mul, Div, Neg,
        MidiToHz   // pop x; push 440 * 2^((x-69)/12)
    };
    Code code = Code::PushConst;
    int  arg  = 0;   // index into constants[] for PushConst, input idx for LoadInput
};

class Expression {
public:
    // Default false: until the compiler proves an expression is constant by
    // evaluating its bytecode, evaluate() must run the ops, not short-circuit
    // on the (uninitialised) constantValue.
    bool                isConstant = false;
    double              constantValue = 0.0;
    std::vector<Op>     ops;
    std::vector<double> constants;

    // referenced inputs (for routing reporting); bitmask of (1 << ExprInputs::*).
    uint32_t inputMask = 0;

    // Evaluate. Audio thread; no allocation, no exceptions.
    double evaluate(const ExprInputs& in) const noexcept {
        if (isConstant) return constantValue;
        // small fixed-size stack; depth is bounded by AST shape,
        // but our compiler emits depth <= ops.size().
        constexpr int MAX = 64;
        double stack[MAX];
        int sp = 0;

        for (const auto& o : ops) {
            switch (o.code) {
                case Op::Code::PushConst:
                    if (sp < MAX) stack[sp++] = constants[(size_t)o.arg];
                    break;
                case Op::Code::LoadInput:
                    if (sp < MAX) stack[sp++] = in.v[o.arg];
                    break;
                case Op::Code::Add: if (sp>=2) { stack[sp-2] += stack[sp-1]; --sp; } break;
                case Op::Code::Sub: if (sp>=2) { stack[sp-2] -= stack[sp-1]; --sp; } break;
                case Op::Code::Mul: if (sp>=2) { stack[sp-2] *= stack[sp-1]; --sp; } break;
                case Op::Code::Div: if (sp>=2) {
                    double d = stack[sp-1];
                    stack[sp-2] = (d == 0.0) ? 0.0 : (stack[sp-2] / d);
                    --sp;
                } break;
                case Op::Code::Neg: if (sp>=1) stack[sp-1] = -stack[sp-1]; break;
                case Op::Code::MidiToHz: if (sp>=1) {
                    double midi = stack[sp-1];
                    // 440 * 2^((midi-69)/12)
                    stack[sp-1] = 440.0 * std::pow(2.0, (midi - 69.0) / 12.0);
                } break;
            }
        }
        return sp > 0 ? stack[sp - 1] : 0.0;
    }
};

} // namespace progsynth
