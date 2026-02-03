#pragma once

#include "input/input.hpp"
#include "error.hpp"
#include "token.hpp"
#include <expected>
#include <string>

/**
 * @brief Tokenizes source code into a stream of Token objects.
 * 
 * Starts lexing on creation using the Lexer::create factory - 
 * this is to prevent having an invalid state where you create
 * the Lexer and attempt to utilize the vectors with invalid
 * uninitialized data. Setting OnTokenError::HALT will cause it
 * to return a LexerError value. As an api user you must still
 * check this value even with OnTokenError::CONTINUE as the Input
 * class may fail to open the file or it may fail in some other
 * unrecoverable way.
 *
 * Lexical Grammar:
 * - Identifiers: `[a-Z_][a-zA-Z0-9_]*`
 * - Numerics: Integers (`123`), Reals (`1.2`, `.5`, `1e-10`)
 * - Strings: Double-quoted, supports 6-digit Unicode `\uXXXXXX`, other escapes
 * - Comments: Single-line (`#`), Multi-line (`<<- ... ->>`)
 * - Operators: `+ - * / = ~= < <= > >= := : ; . , ( ) [ ] { } & | ^ @ !`
 */
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
    Lexer(Input p_char_buff, OnTokenError on_err) : char_buff(p_char_buff),
                                                  continue_on_err(on_err == OnTokenError::CONTINUE) {}

    // gets immediately called on creation. Consumes tokens and will propagate 
    // the error value to create() to be either pushed to invalid_tokens or propagated
    std::expected<void, LexerError> consume_tokens();
    
    // helper function that initializes a token and pushes it. Uses current char_buff
    // position to determine the Token position, so char_buff.back() may be required
    // if reading multi line tokens.
    void push_token(const TokenType &t);
    
    // Calls push_token(success) and advances char_buff if the next character is look_for,
    // otherwise pushes push_token(fail) and undos char_buff advancement
    void push_token_peek(const TokenType &success, const TokenType &fail, unsigned char look_for);
    
    // expects char_buff to be on character '"' and whole string to be "my \a <- alert string"
    std::expected<void, LexerError> consume_string();

    // expects char_buff to be pointing at some number
    std::expected<void, LexerError> consume_number();

    // use this if consuming a "." and you don't know if it will be a number 
    std::expected<void, LexerError> consume_float();

    // use this when consuming a number and it doesn't turn out to be a float
    std::expected<void, LexerError> consume_float(std::string& context, std::size_t line_start, std::size_t col_start);
    
    
    // expects "<<-" already to be read, and for the char_buff to be pointing
    // at the next character over (for example, the 'a' in "<<-apple->>")
    // Will return a LexerError::UNEXPECTED_EOF if it reads and does not find '->>'.
    std::expected<void, LexerError> consume_multi_line_comment();
    
    // Helper function for consume_string. Returns a string to be pushed
    // to a string rather than automatically pushing a token like the
    // other functions due to the unicode char needing to append to the string
    std::expected<std::u8string, LexerError> consume_unicode_char();
    
    // expects char_buff to be pointing at the first character of an identifier
    void consume_ident();
    
    // Skips until finding a new line.
    void consume_line_comment();

    // Tokens get automatically pushed here as consume_* functions are used
    std::vector<Token> tokens;
    
    // Since initilization failures return a LexerError despite
    // OnTokenError being set to CONTINUE, this vector will only
    // contain LexerErrors that are Token related rather than filestream
    // related
    std::vector<LexerError> invalid_tokens;

    bool continue_on_err;

};