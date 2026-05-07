#pragma once

#include <vector>
#include <string>
#include "Ast.h"
#include "Token.h"

namespace progsynth {

struct ParseError {
    std::string message;
    int line = 1;
    int col = 1;
};

class Parser {
public:
    Parser(std::vector<Token> tokens, std::vector<ParseError>& errors);

    Program parseProgram();

private:
    std::vector<Token> toks;
    std::vector<ParseError>& errs;
    size_t pos = 0;

    const Token& peek(size_t off = 0) const;
    const Token& advance();
    bool match(TokenType t);
    bool check(TokenType t) const;
    const Token& expect(TokenType t, const char* what);

    void error(const std::string& msg, const Token& at);
    void synchronize();   // skip to next plausible statement boundary on error

    // statements
    bool parseStmt(Stmt& out);
    bool parseLet  (LetStmt& out);
    bool parseBlock(Block&   out);

    // expressions (precedence climbing)
    ExprPtr parseExpression();
    ExprPtr parseAdd();
    ExprPtr parseMul();
    ExprPtr parseUnary();
    ExprPtr parsePrimary();
};

} // namespace progsynth
