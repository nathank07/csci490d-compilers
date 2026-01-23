#pragma once

#include <string>
#include <variant>

enum class TokenType {
    END_OF_FILE,
    ADD,
    SUB,
    MULTIPLY,
    DIVIDE,
    IDENTIFER,
    EXPONENT,
    LESS_THAN,
    GREATER_THAN,
    LESS_THAN_EQ,
    GREATER_THAN_EQ,
    EQUALS,
    NOT_EQUALS,
    NOT,
    ASSIGN,
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACE,
    RIGHT_BRACE,
    LEFT_SQ_BRACKET,
    RIGHT_SQ_BRACKET,
    AND,
    OR,
    DOT,
    AT,
    INTEGER,
    STRING,
    COLON,
    SEMICOLON,
    COMMA,
    REAL_NUMBER
};

struct TokenString {
    std::string value;
};

struct TokenIdentifier {
    std::string value;
};

struct TokenInteger {
    int value;
};

struct TokenReal {
    double value;
};

using TokenValue = 
    std::variant<
        std::monostate,
        TokenString,
        TokenIdentifier,
        TokenInteger,
        TokenReal
    >;

struct Token {
    TokenType type;
    std::size_t line_number;
    std::size_t column_number;
    TokenValue data;
};