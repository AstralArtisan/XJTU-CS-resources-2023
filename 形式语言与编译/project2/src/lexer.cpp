#include "qcompiler/lexer.h"

#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace qcompiler {

namespace {

bool is_alpha(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool is_digit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

} // namespace

std::string token_type_name(TokenType type) {
    switch (type) {
    case TokenType::End:
        return "END";
    case TokenType::Identifier:
        return "IDENT";
    case TokenType::Number:
        return "NUMBER";
    case TokenType::KwInt:
        return "int";
    case TokenType::KwVoid:
        return "void";
    case TokenType::KwIf:
        return "if";
    case TokenType::KwElse:
        return "else";
    case TokenType::KwWhile:
        return "while";
    case TokenType::KwReturn:
        return "return";
    case TokenType::LBrace:
        return "{";
    case TokenType::RBrace:
        return "}";
    case TokenType::LParen:
        return "(";
    case TokenType::RParen:
        return ")";
    case TokenType::LBracket:
        return "[";
    case TokenType::RBracket:
        return "]";
    case TokenType::Semicolon:
        return ";";
    case TokenType::Comma:
        return ",";
    case TokenType::Plus:
        return "+";
    case TokenType::Minus:
        return "-";
    case TokenType::Star:
        return "*";
    case TokenType::Slash:
        return "/";
    case TokenType::Assign:
        return "=";
    case TokenType::PlusAssign:
        return "+=";
    case TokenType::Less:
        return "<";
    case TokenType::LessEqual:
        return "<=";
    case TokenType::Greater:
        return ">";
    case TokenType::GreaterEqual:
        return ">=";
    case TokenType::EqualEqual:
        return "==";
    case TokenType::NotEqual:
        return "!=";
    case TokenType::Bang:
        return "!";
    case TokenType::AndAnd:
        return "&&";
    case TokenType::OrOr:
        return "||";
    }
    return "?";
}

std::string normalize_source(std::string source) {
    std::string result;
    result.reserve(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(source[i]);
        if (i + 2 < source.size() && c == 0xE2 &&
            static_cast<unsigned char>(source[i + 1]) == 0x80) {
            const unsigned char third = static_cast<unsigned char>(source[i + 2]);
            if (third == 0x93 || third == 0x94 || third == 0x92) {
                result.push_back('-');
                i += 2;
                continue;
            }
        }
        if (i + 1 < source.size() && c == 0xC3 &&
            static_cast<unsigned char>(source[i + 1]) == 0x97) {
            result.push_back('*');
            ++i;
            continue;
        }
        result.push_back(source[i]);
    }
    return result;
}

Lexer::Lexer(std::string source) : source_(normalize_source(std::move(source))) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skip_whitespace_and_comments();
        const std::size_t line = line_;
        const std::size_t column = column_;
        const char c = peek();
        if (c == '\0') {
            tokens.push_back(make_token(TokenType::End, "", line, column));
            return tokens;
        }
        if (is_alpha(c)) {
            tokens.push_back(identifier_or_keyword(line, column));
            continue;
        }
        if (is_digit(c)) {
            tokens.push_back(number(line, column));
            continue;
        }

        advance();
        switch (c) {
        case '{':
            tokens.push_back(make_token(TokenType::LBrace, "{", line, column));
            break;
        case '}':
            tokens.push_back(make_token(TokenType::RBrace, "}", line, column));
            break;
        case '(':
            tokens.push_back(make_token(TokenType::LParen, "(", line, column));
            break;
        case ')':
            tokens.push_back(make_token(TokenType::RParen, ")", line, column));
            break;
        case '[':
            tokens.push_back(make_token(TokenType::LBracket, "[", line, column));
            break;
        case ']':
            tokens.push_back(make_token(TokenType::RBracket, "]", line, column));
            break;
        case ';':
            tokens.push_back(make_token(TokenType::Semicolon, ";", line, column));
            break;
        case ',':
            tokens.push_back(make_token(TokenType::Comma, ",", line, column));
            break;
        case '+':
            tokens.push_back(match('=') ? make_token(TokenType::PlusAssign, "+=", line, column)
                                        : make_token(TokenType::Plus, "+", line, column));
            break;
        case '-':
            tokens.push_back(make_token(TokenType::Minus, "-", line, column));
            break;
        case '*':
            tokens.push_back(make_token(TokenType::Star, "*", line, column));
            break;
        case '/':
            tokens.push_back(make_token(TokenType::Slash, "/", line, column));
            break;
        case '=':
            tokens.push_back(match('=') ? make_token(TokenType::EqualEqual, "==", line, column)
                                        : make_token(TokenType::Assign, "=", line, column));
            break;
        case '<':
            tokens.push_back(match('=') ? make_token(TokenType::LessEqual, "<=", line, column)
                                        : make_token(TokenType::Less, "<", line, column));
            break;
        case '>':
            tokens.push_back(match('=') ? make_token(TokenType::GreaterEqual, ">=", line, column)
                                        : make_token(TokenType::Greater, ">", line, column));
            break;
        case '!':
            tokens.push_back(match('=') ? make_token(TokenType::NotEqual, "!=", line, column)
                                        : make_token(TokenType::Bang, "!", line, column));
            break;
        case '&':
            if (!match('&')) {
                error("expected '&' after '&'");
            }
            tokens.push_back(make_token(TokenType::AndAnd, "&&", line, column));
            break;
        case '|':
            if (!match('|')) {
                error("expected '|' after '|'");
            }
            tokens.push_back(make_token(TokenType::OrOr, "||", line, column));
            break;
        default:
            error(std::string("unexpected character '") + c + "'");
        }
    }
}

char Lexer::peek(std::size_t offset) const {
    if (pos_ + offset >= source_.size()) {
        return '\0';
    }
    return source_[pos_ + offset];
}

char Lexer::advance() {
    const char c = peek();
    if (c == '\0') {
        return c;
    }
    ++pos_;
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (peek() != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::skip_whitespace_and_comments() {
    while (true) {
        const char c = peek();
        if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
            advance();
            continue;
        }
        if (c == '/' && peek(1) == '/') {
            while (peek() != '\n' && peek() != '\0') {
                advance();
            }
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            advance();
            advance();
            while (!(peek() == '*' && peek(1) == '/') && peek() != '\0') {
                advance();
            }
            if (peek() == '\0') {
                error("unterminated block comment");
            }
            advance();
            advance();
            continue;
        }
        return;
    }
}

Token Lexer::make_token(TokenType type, std::string lexeme, std::size_t line, std::size_t column) const {
    return Token{type, std::move(lexeme), line, column};
}

Token Lexer::identifier_or_keyword(std::size_t line, std::size_t column) {
    const std::size_t start = pos_;
    while (is_alnum(peek())) {
        advance();
    }
    const std::string text = source_.substr(start, pos_ - start);
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"int", TokenType::KwInt},       {"void", TokenType::KwVoid},
        {"if", TokenType::KwIf},         {"else", TokenType::KwElse},
        {"while", TokenType::KwWhile},   {"return", TokenType::KwReturn},
    };
    const auto it = keywords.find(text);
    if (it != keywords.end()) {
        return make_token(it->second, text, line, column);
    }
    return make_token(TokenType::Identifier, text, line, column);
}

Token Lexer::number(std::size_t line, std::size_t column) {
    const std::size_t start = pos_;
    while (is_digit(peek())) {
        advance();
    }
    return make_token(TokenType::Number, source_.substr(start, pos_ - start), line, column);
}

void Lexer::error(const std::string &message) const {
    throw std::runtime_error("lex error at " + std::to_string(line_) + ":" + std::to_string(column_) + ": " + message);
}

} // namespace qcompiler
