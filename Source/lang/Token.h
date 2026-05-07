#pragma once

#include <string>
#include "Units.h"

namespace progsynth {

enum class TokenType {
    // single-char
    LBrace, RBrace, LParen, RParen, Comma, Equals, Dot,
    Plus, Minus, Star, Slash,

    // literals
    Number,        // value + unit (unit may be Unit::None)
    SyncLiteral,   // 1/4, 1/8t, 1/4. etc

    // identifiers and keywords
    Identifier,
    KwLet,

    // end markers
    Eof,
    Error
};

struct Token {
    TokenType type = TokenType::Eof;
    std::string text;        // original lexeme
    double number = 0.0;     // for Number
    Unit unit = Unit::None;  // for Number

    // sync literal fields
    int syncNum = 0;
    int syncDen = 0;
    bool syncDotted = false;
    bool syncTriplet = false;

    int line = 1;
    int col = 1;
};

} // namespace progsynth
