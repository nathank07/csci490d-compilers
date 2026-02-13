#include <span>
#include "../lexer/token.hpp"

enum class AstErrorType {
    MISMATCH_PAREN
};

struct AstError {
    AstErrorType type;
    std::string message;

    static AstError mismatched_bracket(std::span<const Token> ctx) {

    }
};