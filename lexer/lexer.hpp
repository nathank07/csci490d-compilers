#pragma once

#include "input/input.hpp"
#include "error.hpp"
#include "token.hpp"
#include <expected>
#include <string>

class Lexer {

    Input char_buff;
    Lexer(Input char_buff) : char_buff(char_buff) {}
    std::expected<void, LexerError> consume_tokens();
    void push_token(const TokenType &t, TokenValue v);
    void push_token_peek(const TokenType &success, const TokenType &fail, unsigned char look_for);
    std::expected<void, LexerError> consume_string();
    std::expected<void, LexerError> consume_ident();
    std::expected<void, LexerError> consume_number();
    
    std::vector<Token> tokens;

public:
    
    std::expected<Lexer, LexerError> create(const std::string &filename);

};