#pragma once

#include "input/input.hpp"
#include "error.hpp"
#include "token.hpp"
#include <expected>
#include <string>

class Lexer {

public:
    enum class OnTokenError {
        HALT = 0,
        CONTINUE = 1
    };

    static std::expected<Lexer, LexerError> create(const std::string &filename, OnTokenError on_err);
    const std::vector<Token>& get_tokens() { return tokens; };
    const std::vector<LexerError>& get_invalid_tokens() { return invalid_tokens; };

private:

    Input char_buff;
    Lexer(Input char_buff, OnTokenError on_err) : char_buff(char_buff),
                                                continue_on_err(on_err == OnTokenError::CONTINUE) {}
    std::expected<void, LexerError> consume_tokens();
    void push_token(const TokenType &t);
    void push_token_peek(const TokenType &success, const TokenType &fail, unsigned char look_for);
    std::expected<void, LexerError> consume_string();
    std::expected<void, LexerError> consume_ident();
    std::expected<void, LexerError> consume_number();
    std::expected<void, LexerError> consume_float();
    std::expected<void, LexerError> consume_float(std::string& context, std::size_t line_start, std::size_t col_start);
    
    std::vector<Token> tokens;
    
    // Since initilization failures return a LexerError despite
    // OnTokenError being set to CONTINUE, this vector will only
    // contain LexerErrors that are Token related rather than filestream
    // related
    std::vector<LexerError> invalid_tokens;

    bool continue_on_err;

};