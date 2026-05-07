#include "Parser.h"

namespace progsynth {

Parser::Parser(std::vector<Token> tokens, std::vector<ParseError>& errors)
    : toks(std::move(tokens)), errs(errors) {}

const Token& Parser::peek(size_t off) const {
    return toks[std::min(pos + off, toks.size() - 1)];
}

const Token& Parser::advance() {
    if (pos < toks.size() - 1) ++pos;
    return toks[pos - 1];
}

bool Parser::check(TokenType t) const { return peek().type == t; }

bool Parser::match(TokenType t) {
    if (check(t)) { advance(); return true; }
    return false;
}

const Token& Parser::expect(TokenType t, const char* what) {
    if (check(t)) return advance();
    error(std::string("expected ") + what, peek());
    return peek();
}

void Parser::error(const std::string& msg, const Token& at) {
    ParseError e;
    e.message = msg;
    e.line = at.line;
    e.col  = at.col;
    errs.push_back(e);
}

// On error, skip until we hit a newline-equivalent boundary.  Since our lexer
// strips newlines, we use the next block keyword / let / RBrace / Eof.
void Parser::synchronize() {
    while (!check(TokenType::Eof)) {
        const Token& t = peek();
        if (t.type == TokenType::KwLet) return;
        if (t.type == TokenType::Identifier) {
            const auto& n = t.text;
            if (n == "osc1" || n == "osc2" || n == "osc3" ||
                n == "filter" || n == "ampEnv" || n == "fltEnv" ||
                n == "lfo1" || n == "lfo2" || n == "master")
                return;
        }
        if (t.type == TokenType::RBrace) { advance(); return; }
        advance();
    }
}

Program Parser::parseProgram() {
    Program p;
    while (!check(TokenType::Eof)) {
        Stmt s;
        size_t savedPos = pos;
        if (parseStmt(s)) {
            p.stmts.push_back(std::move(s));
        } else {
            // didn't make progress? force advance to avoid infinite loop
            if (pos == savedPos) advance();
            synchronize();
        }
    }
    return p;
}

bool Parser::parseStmt(Stmt& out) {
    if (check(TokenType::KwLet)) {
        out.kind = StmtKind::Let;
        return parseLet(out.let);
    }
    if (check(TokenType::Identifier)) {
        out.kind = StmtKind::Block;
        return parseBlock(out.block);
    }
    error("expected block name or 'let'", peek());
    return false;
}

bool Parser::parseLet(LetStmt& out) {
    const Token& letTok = expect(TokenType::KwLet, "'let'");
    out.line = letTok.line; out.col = letTok.col;
    if (!check(TokenType::Identifier)) {
        error("expected identifier after 'let'", peek());
        return false;
    }
    out.name = advance().text;
    expect(TokenType::Equals, "'='");
    out.value = parseExpression();
    return out.value != nullptr;
}

bool Parser::parseBlock(Block& out) {
    const Token& nameTok = advance();   // identifier
    out.name = nameTok.text;
    out.line = nameTok.line;
    out.col  = nameTok.col;

    if (!match(TokenType::LBrace)) {
        error("expected '{' after block name", peek());
        return false;
    }

    if (!check(TokenType::RBrace)) {
        while (true) {
            if (!check(TokenType::Identifier)) {
                error("expected parameter name", peek());
                break;
            }
            const Token& nm = advance();
            Assignment a;
            a.name = nm.text;
            a.line = nm.line;
            a.col  = nm.col;
            expect(TokenType::Equals, "'='");
            a.value = parseExpression();
            if (a.value) out.assignments.push_back(std::move(a));

            if (!match(TokenType::Comma)) break;
        }
    }

    expect(TokenType::RBrace, "'}'");
    return true;
}

ExprPtr Parser::parseExpression() { return parseAdd(); }

ExprPtr Parser::parseAdd() {
    ExprPtr left = parseMul();
    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        char op = advance().text[0];
        ExprPtr right = parseMul();
        auto bin = std::make_unique<Expr>();
        bin->kind = ExprKind::Binary;
        bin->op = op;
        bin->line = left ? left->line : 0;
        bin->col  = left ? left->col  : 0;
        bin->lhs = std::move(left);
        bin->rhs = std::move(right);
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parseMul() {
    ExprPtr left = parseUnary();
    while (check(TokenType::Star) || check(TokenType::Slash)) {
        char op = advance().text[0];
        ExprPtr right = parseUnary();
        auto bin = std::make_unique<Expr>();
        bin->kind = ExprKind::Binary;
        bin->op = op;
        bin->line = left ? left->line : 0;
        bin->col  = left ? left->col  : 0;
        bin->lhs = std::move(left);
        bin->rhs = std::move(right);
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (check(TokenType::Minus)) {
        const Token& m = advance();
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Unary;
        e->op = '-';
        e->line = m.line; e->col = m.col;
        e->lhs = parseUnary();
        return e;
    }
    return parsePrimary();
}

ExprPtr Parser::parsePrimary() {
    const Token& t = peek();
    if (t.type == TokenType::Number) {
        advance();
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Number;
        e->number = t.number;
        e->unit   = t.unit;
        e->line = t.line; e->col = t.col;
        return e;
    }
    if (t.type == TokenType::SyncLiteral) {
        advance();
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Sync;
        e->syncNum = t.syncNum;
        e->syncDen = t.syncDen;
        e->syncDotted = t.syncDotted;
        e->syncTriplet = t.syncTriplet;
        e->line = t.line; e->col = t.col;
        return e;
    }
    if (t.type == TokenType::Identifier) {
        advance();
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Identifier;
        e->name = t.text;
        e->line = t.line; e->col = t.col;
        if (match(TokenType::Dot)) {
            if (!check(TokenType::Identifier)) {
                error("expected identifier after '.'", peek());
                return nullptr;
            }
            const Token& sub = advance();
            e->kind = ExprKind::Qualified;
            e->subName = sub.text;
        }
        return e;
    }
    if (t.type == TokenType::LParen) {
        advance();
        ExprPtr inner = parseExpression();
        expect(TokenType::RParen, "')'");
        return inner;
    }
    error("expected expression", t);
    return nullptr;
}

} // namespace progsynth
