#pragma once
#include "../lexer/token.hpp"

enum class AstErrorType {
    MISMATCH_PAREN,
    EXTRA_UNARY,
    FAILED_TO_PARSE_SYMBOL
};

struct AstError {
    AstErrorType type;
    std::string message;
    std::size_t skip_x_tok;

    static AstError mismatched_bracket(Token bad_paren) {
        return AstError{AstErrorType::MISMATCH_PAREN, "Mismatched parenthesis", 1};
    }

    static AstError extra_unary(Token bad_unary) {
        return AstError{AstErrorType::EXTRA_UNARY, "Bad unary", 2};
    }

    static AstError bad_symbol(Token bad_symbol) {
        return AstError{AstErrorType::FAILED_TO_PARSE_SYMBOL, "Bad symbol " + bad_symbol.get_type_string(), 1};
    }

};