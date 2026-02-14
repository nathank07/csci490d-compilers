#pragma once
#include "../lexer/token.hpp"
#include <span>

enum class AstErrorType {
    MISMATCH_PAREN,
    EXTRA_UNARY,
    FAILED_TO_PARSE_SYMBOL,
    EMPTY_PARENS,
    MALFORMED_RHS,
};

struct AstError {
    AstErrorType type;
    std::string message;
    std::size_t skip_x_tok;
    std::optional<Token> offending_token;

    static AstError mismatched_bracket(Token bad_paren) {
        return AstError{AstErrorType::MISMATCH_PAREN, "Mismatched parenthesis", 1, bad_paren};
    }

    static AstError extra_unary(Token bad_unary) {
        return AstError{AstErrorType::EXTRA_UNARY, "Bad unary", 2, bad_unary};
    }

    static AstError bad_symbol(Token bad_symbol) {
        return AstError{AstErrorType::FAILED_TO_PARSE_SYMBOL, "Bad symbol " + bad_symbol.get_type_string(), 1, bad_symbol};
    }

    static AstError malformed_rhs(std::span <const Token> context, AstError e) {
        return AstError{AstErrorType::MALFORMED_RHS, "Invalid RHS while parsing expression.", context.size() + 1, e.offending_token};
    }

    static AstError empty_parens(std::span <const Token> parens) {
        return AstError{AstErrorType::EMPTY_PARENS, "Empty parens", parens.size()};
    }

};