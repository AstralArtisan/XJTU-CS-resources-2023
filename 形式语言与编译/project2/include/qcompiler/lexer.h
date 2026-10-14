#pragma once

#include "qcompiler/token.h"

#include <string>
#include <vector>

namespace qcompiler {

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize();

private:
    char peek(std::size_t offset = 0) const;
    char advance();
    bool match(char expected);
    void skip_whitespace_and_comments();
    Token make_token(TokenType type, std::string lexeme, std::size_t line, std::size_t column) const;
    Token identifier_or_keyword(std::size_t line, std::size_t column);
    Token number(std::size_t line, std::size_t column);
    [[noreturn]] void error(const std::string &message) const;

    std::string source_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

std::string normalize_source(std::string source);

} // namespace qcompiler
