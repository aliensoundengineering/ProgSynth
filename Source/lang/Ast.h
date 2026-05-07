#pragma once

#include <memory>
#include <string>
#include <vector>
#include "Units.h"

namespace progsynth {

// ----------- expressions ---------------------------------------------------

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

enum class ExprKind {
    Number,        // numeric literal with unit
    Sync,          // 1/4, 1/8t, etc
    Identifier,    // bareword: variable / mod source / wave/filter/on-off keyword
    Qualified,     // ident.ident
    Binary,
    Unary,
};

struct Expr {
    ExprKind kind;

    // Number
    double number = 0.0;
    Unit   unit   = Unit::None;

    // Sync
    int  syncNum = 0, syncDen = 0;
    bool syncDotted = false, syncTriplet = false;

    // Identifier / Qualified
    std::string name;       // for Identifier or first part of Qualified
    std::string subName;    // for Qualified: second part

    // Binary / Unary
    char op = 0;            // '+', '-', '*', '/'
    ExprPtr lhs;
    ExprPtr rhs;            // unused for Unary

    int line = 1;
    int col = 1;
};

// ----------- top-level -----------------------------------------------------

struct Assignment {
    std::string name;
    ExprPtr value;
    int line = 1, col = 1;
};

struct Block {
    std::string name;       // osc1, filter, etc.
    std::vector<Assignment> assignments;
    int line = 1, col = 1;
};

struct LetStmt {
    std::string name;
    ExprPtr value;
    int line = 1, col = 1;
};

enum class StmtKind { Block, Let };

struct Stmt {
    StmtKind kind;
    Block    block;     // when Block
    LetStmt  let;       // when Let
};

struct Program {
    std::vector<Stmt> stmts;
};

} // namespace progsynth
