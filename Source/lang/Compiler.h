#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include "Ast.h"
#include "CompiledPatch.h"

namespace progsynth {

struct CompileError {
    std::string message;
    int line = 1;
    int col = 1;
};

class Compiler {
public:
    // Compile a parsed program into a CompiledPatch.
    // On failure (errors non-empty), the returned patch may still be partially
    // populated; callers should not use it for audio if errors.size() > 0.
    static CompiledPatch compile(const Program& program,
                                 std::vector<CompileError>& errors);
};

} // namespace progsynth
