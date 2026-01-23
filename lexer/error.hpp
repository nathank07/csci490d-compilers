#pragma once

#include <string>

enum class LexerErrorType {
    FAILED_TO_READ_FILE,
    UNEXPECTED_TOKEN,
    UNEXPECTED_EOF,
    INVALID_ESCAPE,
    UNKNOWN_ERROR
};

struct LexerError {
    LexerErrorType type;
    std::size_t line_number;
    std::size_t column_number;
    std::string line;
    std::string message;
};

inline std::unexpected<LexerError> create_error(LexerErrorType e, const Input& buff, const std::string& message) {
    auto line_num = buff.get_col_number();
    return std::unexpected<LexerError>(LexerError{
        e,
        line_num,
        buff.get_col_number(),
        buff.get_line(line_num).value_or(""),
        message
    });
}

inline std::unexpected<LexerError> create_error(LexerErrorType e, const std::string& message) {
    return std::unexpected<LexerError>(LexerError{
        e,
        0,
        0,
        "N/A",
        message
    });
}