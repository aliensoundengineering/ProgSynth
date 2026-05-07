#include "Lexer.h"

#include <cctype>
#include <cstdlib>

namespace progsynth {

Lexer::Lexer(std::string source) : src(std::move(source)) {}

char Lexer::peek(size_t off) const {
    return (pos + off < src.size()) ? src[pos + off] : '\0';
}

char Lexer::advance() {
    char c = src[pos++];
    if (c == '\n') { ++line; col = 1; }
    else           { ++col; }
    return c;
}

void Lexer::skipWhitespaceAndComments() {
    while (!atEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '#') {
            while (!atEnd() && peek() != '\n') advance();
        } else {
            break;
        }
    }
}

static bool isIdStart(char c) { return std::isalpha((unsigned char)c) || c == '_'; }
static bool isIdCont (char c) { return std::isalnum((unsigned char)c) || c == '_'; }

Token Lexer::lexNumberOrSync(int startLine, int startCol) {
    Token tok;
    tok.line = startLine;
    tok.col  = startCol;

    size_t startPos = pos;
    while (std::isdigit((unsigned char)peek())) advance();

    // sync literal: integer '/' integer (only at top, no decimal before)
    if (peek() == '/' && std::isdigit((unsigned char)peek(1))) {
        std::string numStr(src.begin() + startPos, src.begin() + pos);
        advance(); // consume '/'
        size_t denStart = pos;
        while (std::isdigit((unsigned char)peek())) advance();
        std::string denStr(src.begin() + denStart, src.begin() + pos);

        tok.type = TokenType::SyncLiteral;
        tok.syncNum = std::atoi(numStr.c_str());
        tok.syncDen = std::atoi(denStr.c_str());

        // optional 't' (triplet) or '.' (dotted)
        if (peek() == 't' && !isIdCont(peek(1))) {
            advance();
            tok.syncTriplet = true;
        } else if (peek() == '.') {
            advance();
            tok.syncDotted = true;
        }

        tok.text.assign(src.begin() + startPos, src.begin() + pos);
        return tok;
    }

    // floating part
    if (peek() == '.' && std::isdigit((unsigned char)peek(1))) {
        advance(); // dot
        while (std::isdigit((unsigned char)peek())) advance();
    }

    std::string numStr(src.begin() + startPos, src.begin() + pos);
    tok.number = std::strtod(numStr.c_str(), nullptr);

    // unit suffix - manual sequence to disambiguate s vs st vs ms etc.
    auto matchUnit = [this](const char* s) -> bool {
        size_t n = 0;
        while (s[n] != '\0') {
            if (peek(n) != s[n]) return false;
            ++n;
        }
        if (isIdCont(peek(n))) return false;
        for (size_t i = 0; i < n; ++i) advance();
        return true;
    };

    Unit u = Unit::None;
    if      (matchUnit("kHz"))  u = Unit::KHz;
    else if (matchUnit("Hz"))   u = Unit::Hz;
    else if (matchUnit("ms"))   u = Unit::Ms;
    else if (matchUnit("st"))   u = Unit::St;
    else if (matchUnit("cent")) u = Unit::Cent;
    else if (matchUnit("dB"))   u = Unit::Db;
    else if (matchUnit("s"))    u = Unit::S;
    else if (peek() == '%')   { advance(); u = Unit::Percent; }

    tok.type = TokenType::Number;
    tok.unit = u;
    tok.text.assign(src.begin() + startPos, src.begin() + pos);
    return tok;
}

Token Lexer::lexIdentifier(int startLine, int startCol) {
    Token tok;
    tok.line = startLine;
    tok.col  = startCol;
    size_t startPos = pos;
    advance();
    while (isIdCont(peek())) advance();
    tok.text.assign(src.begin() + startPos, src.begin() + pos);

    if (tok.text == "let") tok.type = TokenType::KwLet;
    else                   tok.type = TokenType::Identifier;
    return tok;
}

std::vector<Token> Lexer::tokenize(std::vector<LexError>& errors) {
    std::vector<Token> out;

    while (true) {
        skipWhitespaceAndComments();
        if (atEnd()) break;

        int startLine = line;
        int startCol  = col;
        char c = peek();

        if (std::isdigit((unsigned char)c)) {
            out.push_back(lexNumberOrSync(startLine, startCol));
            continue;
        }
        if (isIdStart(c)) {
            out.push_back(lexIdentifier(startLine, startCol));
            continue;
        }

        Token tok;
        tok.line = startLine;
        tok.col  = startCol;
        tok.text = std::string(1, c);

        switch (c) {
            case '{': tok.type = TokenType::LBrace; advance(); break;
            case '}': tok.type = TokenType::RBrace; advance(); break;
            case '(': tok.type = TokenType::LParen; advance(); break;
            case ')': tok.type = TokenType::RParen; advance(); break;
            case ',': tok.type = TokenType::Comma;  advance(); break;
            case '=': tok.type = TokenType::Equals; advance(); break;
            case '.': tok.type = TokenType::Dot;    advance(); break;
            case '+': tok.type = TokenType::Plus;   advance(); break;
            case '-': tok.type = TokenType::Minus;  advance(); break;
            case '*': tok.type = TokenType::Star;   advance(); break;
            case '/': tok.type = TokenType::Slash;  advance(); break;
            default: {
                LexError e;
                e.message = std::string("unexpected character '") + c + "'";
                e.line = startLine;
                e.col  = startCol;
                errors.push_back(e);
                advance();
                continue;
            }
        }
        out.push_back(tok);
    }

    Token eof;
    eof.type = TokenType::Eof;
    eof.line = line;
    eof.col  = col;
    out.push_back(eof);
    return out;
}

} // namespace progsynth
