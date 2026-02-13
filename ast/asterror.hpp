#include <span>
#include "../lexer/token.hpp"

enum class AstErrorType {
    MISMATCH_PAREN,
    EXTRA_UNARY
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
};