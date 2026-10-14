#pragma once

#include <cstddef>
#include <string>

namespace qcompiler {

enum class TokenType {
    End,
    Identifier,
    Number,
    KwInt,
    KwVoid,
    KwIf,
    KwElse,
    KwWhile,
    KwReturn,
    LBrace,
    RBrace,
    LParen,
    RParen,
    LBracket,
    RBracket,
    Semicolon,
    Comma,
    Plus,
    Minus,
    Star,
    Slash,
    Assign,
    PlusAssign,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    EqualEqual,
    NotEqual,
    Bang,
    AndAnd,
    OrOr
};

struct Token {
    TokenType type = TokenType::End;
    std::string lexeme;
    std::size_t line = 1;
    std::size_t column = 1;
};

std::string token_type_name(TokenType type);

} // namespace qcompiler
