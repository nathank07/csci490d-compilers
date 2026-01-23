#include "lexer.hpp"
#include "token.hpp"

std::expected<Lexer, LexerError> Lexer::create(const std::string &filename) {
    auto input = Input::create(filename);

    if (input.error() == Input::ErrorCode::FAILED_TO_READ_FILE) {
        return create_error(LexerErrorType::FAILED_TO_READ_FILE, 
            "Input buffer failed to read the provided filepath: " + filename);
    }

    if (!input) {
        return create_error(LexerErrorType::UNKNOWN_ERROR, "Failed to open input buffer for an unknown reason.");
    }

    return Lexer(*input);
}

void Lexer::push_token(const TokenType &t, TokenValue v = {}) {
    tokens.push_back({
        t,
        char_buff.get_line_number(),
        char_buff.get_col_number(),
        v
    });
}

void Lexer::push_token_peek(const TokenType &success, const TokenType &fail, unsigned char look_for) {
    auto c = char_buff.peek();
    if (c == look_for) {
        push_token(success);
        char_buff.consume();
    } else {
        push_token(fail);
    }
}

std::expected<void, LexerError> Lexer::consume_tokens() {
    while (true) {
        auto c = char_buff.consume();
        if (!c) {
            push_token(TokenType::END_OF_FILE);
            return;
        }

        switch (*c) {
            case '+': push_token(TokenType::ADD); break;
            case '-': push_token(TokenType::SUB); break;
            case '*': push_token(TokenType::MULTIPLY); break;
            case '/': push_token(TokenType::DIVIDE); break;
            case '=': push_token(TokenType::EQUALS); break;
            case '(': push_token(TokenType::LEFT_PAREN); break;
            case ')': push_token(TokenType::RIGHT_PAREN); break;
            case '{': push_token(TokenType::LEFT_BRACE); break;
            case '}': push_token(TokenType::RIGHT_BRACE); break;
            case '[': push_token(TokenType::LEFT_SQ_BRACKET); break;
            case ']': push_token(TokenType::RIGHT_SQ_BRACKET); break;
            case '&': push_token(TokenType::AND); break;
            case '|': push_token(TokenType::OR); break;
            case '^': push_token(TokenType::EXPONENT); break;
            case '.': push_token(TokenType::DOT); break;
            case '@': push_token(TokenType::AT); break;
            case ';': push_token(TokenType::SEMICOLON); break;
            case ',': push_token(TokenType::COMMA); break;
            case '!': push_token(TokenType::NOT); break;
            case ':': push_token_peek(TokenType::ASSIGN, TokenType::COLON, '='); break;
            case '<': push_token_peek(TokenType::LESS_THAN_EQ, TokenType::LESS_THAN, '='); break;
            case '>': push_token_peek(TokenType::GREATER_THAN_EQ, TokenType::LESS_THAN, '='); break;
            case '~': {
                auto next = char_buff.peek();
                if (next == '=') {
                    push_token(TokenType::NOT_EQUALS);
                    break;
                } else {
                    return create_error(LexerErrorType::UNEXPECTED_TOKEN, char_buff,
                        "Expected '=' after '~', found " +  
                            (next ? "'" + std::string(1, *next) + "'" : 
                                    std::string("EOF")));
                }
            }
            case '"': {
                auto s = consume_string();
                if (!s) {
                    return std::unexpected(s.error());
                }
                break;
            }
            
        }
    }

    return;
}

std::expected<void, LexerError> Lexer::consume_string() {

    std::string v;

    while (true) {
        auto c = char_buff.consume();

        if (!c) {
            return create_error(LexerErrorType::UNEXPECTED_EOF, char_buff,
                "Found EOF while consuming string");
        }


    }
}
