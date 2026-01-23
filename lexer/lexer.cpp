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

    auto lexer = Lexer(*input);

    auto result = lexer.consume_tokens();

    if (!result) {
        return std::unexpected(result.error());
    }

    return lexer;
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
        auto c = char_buff.peek();
        if (!c) {
            push_token(TokenType::END_OF_FILE);
            return {};
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
            case '>': push_token_peek(TokenType::GREATER_THAN_EQ, TokenType::GREATER_THAN, '='); break;
            case '~': {
                char_buff.next();
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
                char_buff.next();
                auto s = consume_string();
                if (!s) {
                    return std::unexpected(s.error());
                }
                break;
            }
            default: {
                if (std::isspace(*c)) break;

                return create_error(LexerErrorType::UNEXPECTED_TOKEN, char_buff,
                    "Found illegal character: '" + std::string(1, *c) + "'");
            }
        }

        char_buff.next();
    }

    return {};
}

std::expected<void, LexerError> Lexer::consume_string() {

    std::string v;

    while (true) {
        auto c = char_buff.consume();

        if (!c) {
            return create_error(LexerErrorType::UNEXPECTED_EOF, char_buff,
                "Found EOF while consuming string");
        }

        switch (*c) {
            case '"': push_token(TokenType::STRING, TokenString{v}); return {};
            case '\n': break;
            case '\r': break;
            case '\\': {
                auto next = char_buff.peek();
                if (!next) {
                    break; // will continue to unexpected eof error
                }

                switch (*next) {
                    case 'n':  v += '\n'; break;
                    case 't':  v += '\t'; break;
                    case 'r':  v += '\r'; break;
                    case '\\': v += '\\'; break;
                    case 'a':  v += '\a'; break;
                    case 'b':  v += '\b'; break;
                    case '\n': break;
                    case 'u': {
                        // todo implement unicode
                        break;
                    }
                    default: {
                        return create_error(LexerErrorType::INVALID_ESCAPE, char_buff,
                                "Invalid escape sequence found while consuming string.");
                    }
                }
            }
            default: v += *c; break;
        }
    }
}

#include <iostream>

void Lexer::output_tokens() {
    for (auto token : tokens) {
        switch (token.type) {
            case TokenType::ADD: std::cout << "TOKEN_ADD"; break;
            case TokenType::SUB: std::cout << "TOKEN_SUB"; break; 
            case TokenType::MULTIPLY: std::cout << "TOKEN_MULTIPLY"; break;
            case TokenType::DIVIDE: std::cout << "TOKEN_DIVIDE"; break;
            case TokenType::EXPONENT: std::cout << "TOKEN_EXPONENT"; break;
            case TokenType::LESS_THAN: std::cout << "TOKEN_LESS_THAN"; break;
            case TokenType::LESS_THAN_EQ: std::cout << "TOKEN_LESS_THAN_EQ"; break;
            case TokenType::GREATER_THAN: std::cout << "TOKEN_GREATER_THAN"; break;
            case TokenType::GREATER_THAN_EQ: std::cout << "TOKEN_GREATER_THAN_EQ"; break;
            case TokenType::EQUALS: std::cout << "TOKEN_EQUALS"; break;
            case TokenType::NOT_EQUALS: std::cout << "TOKEN_NOT_EQUALS"; break;
            case TokenType::NOT: std::cout << "TOKEN_NOT"; break;
            case TokenType::ASSIGN: std::cout << "TOKEN_ASSIGN"; break;
            case TokenType::LEFT_PAREN: std::cout << "TOKEN_LEFT_PAREN"; break;
            case TokenType::RIGHT_PAREN: std::cout << "TOKEN_RIGHT_PAREN"; break;
            case TokenType::LEFT_BRACE: std::cout << "TOKEN_LEFT_BRACE"; break;
            case TokenType::RIGHT_BRACE: std::cout << "TOKEN_RIGHT_BRACE"; break;
            case TokenType::LEFT_SQ_BRACKET: std::cout << "TOKEN_LEFT_SQ_BRACKET"; break;
            case TokenType::RIGHT_SQ_BRACKET: std::cout << "TOKEN_RIGHT_SQ_BRACKET"; break;
            case TokenType::AND: std::cout << "TOKEN_AND"; break;
            case TokenType::OR: std::cout << "TOKEN_OR"; break;
            case TokenType::DOT: std::cout << "TOKEN_DOT"; break;
            case TokenType::AT: std::cout << "TOKEN_AT"; break;
            case TokenType::COLON: std::cout << "TOKEN_COLON"; break;
            case TokenType::SEMICOLON: std::cout << "TOKEN_SEMICOLON"; break;
            case TokenType::COMMA: std::cout << "TOKEN_COMMA"; break;
            case TokenType::END_OF_FILE: std::cout << "EOF"; break;
            case TokenType::STRING: {
                const auto &str = std::get<TokenString>(token.data);
                std::cout << "TOKEN_STRING: " << str.value;
            }
            case TokenType::IDENTIFER: {
                const auto &str = std::get<TokenIdentifier>(token.data);
                std::cout << "TOKEN_IDENT: " << str.value;
            }
            case TokenType::INTEGER: {
                const auto &i = std::get<TokenInteger>(token.data);
                std::cout << "TOKEN_INTEGER: " << i.value;
            }
            case TokenType::REAL_NUMBER: {
                const auto &f = std::get<TokenReal>(token.data);
                std::cout << "TOKEN_REAL: " << f.value;
            }
        }

        std::cout << " " << token.line_number << ":" << token.column_number << std::endl;
    }
}