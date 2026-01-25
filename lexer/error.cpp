#include "error.hpp"

std::string LexerError::get_type_string() {
    switch (type) {
        case LexerErrorType::FAILED_TO_READ_FILE: return "ERROR_FAILED_TO_READ_FILE";
        case LexerErrorType::UNEXPECTED_TOKEN: return "ERROR_UNEXPECTED_TOKEN";
        case LexerErrorType::UNEXPECTED_EOF: return "ERROR_UNEXPECTED_EOF";
        case LexerErrorType::INVALID_ESCAPE: return "ERROR_INVALID_ESCAPE_CHARACTER";
        case LexerErrorType::UNKNOWN_ERROR: return "ERROR_UNKNOWN";
        case LexerErrorType::MALFORMED_REAL: return "ERROR_MALFORMED_REAL";
        default: return "ERROR_???"; // should be unreachable
    }
}