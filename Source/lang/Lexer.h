#pragma once

#include <vector>
#include <string>
#include "Token.h"

namespace progsynth {

struct LexError {
    std::string message;
    int line = 1;
    int col = 1;
};

class Lexer {
public:
    explicit Lexer(std::string source);

    // Tokenize the whole source. Returns the list (always ending with Eof).
    // Errors are appended to `errors` and the offending position is skipped.
    std::vector<Token> tokenize(std::vector<LexError>& errors);

private:
    std::string src;
    size_t pos = 0;
    int line = 1;
    int col = 1;

    bool atEnd() const { return pos >= src.size(); }
    char peek(size_t off = 0) const;
    char advance();
    void skipWhitespaceAndComments();

    Token lexNumberOrSync(int startLine, int startCol);
    Token lexIdentifier(int startLine, int startCol);
};

} // namespace progsynth
