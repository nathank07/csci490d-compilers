#include "lexer.hpp"
#include "error.hpp"
#include "token.hpp"
#include <cctype>
#include <optional>
#include <string>

std::expected<Lexer, LexerError> Lexer::create(const std::string &filename, OnTokenError on_err = OnTokenError::HALT) {
    auto input = Input::create(filename);

    if (!input && input.error() == Input::ErrorCode::FAILED_TO_READ_FILE) {
        return LexerError::create_error(LexerErrorType::FAILED_TO_READ_FILE, 
            "Input buffer failed to read the provided filepath: " + filename);
    }

    if (!input) {
        return LexerError::create_error(LexerErrorType::UNKNOWN_ERROR, "Failed to open input buffer for an unknown reason.");
    }

    auto lexer = Lexer(*input, on_err);

    auto result = lexer.consume_tokens();

    while (!result) {

        if (!lexer.continue_on_err) {
            return std::unexpected(result.error());
        }
        lexer.invalid_tokens.push_back(result.error());

        lexer.char_buff.next();
        
        result = lexer.consume_tokens();
    }

    return lexer;
}

void Lexer::push_token(const TokenType &t) {
    tokens.push_back({
        t,
        char_buff.get_line_number(),
        char_buff.get_col_number(),
        {}
    });
}

void Lexer::push_token_peek(const TokenType &success, const TokenType &fail, unsigned char look_for) {
    char_buff.next();
    auto c = char_buff.peek();
    char_buff.back(); // for position
    if (c == look_for) {
        push_token(success);
        char_buff.next();
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
            case '@': push_token(TokenType::AT); break;
            case ';': push_token(TokenType::SEMICOLON); break;
            case ',': push_token(TokenType::COMMA); break;
            case '!': push_token(TokenType::NOT); break;
            case ':': push_token_peek(TokenType::ASSIGN, TokenType::COLON, '='); break;
            case '>': push_token_peek(TokenType::GREATER_THAN_EQ, TokenType::GREATER_THAN, '='); break;
            case '~': {
                char_buff.next();
                auto next = char_buff.peek();
                if (next == '=') {
                    char_buff.back();
                    push_token(TokenType::NOT_EQUALS);
                    char_buff.next();
                    break;
                } else {
                    return LexerError::create_error(LexerErrorType::UNEXPECTED_TOKEN, char_buff,
                        "Expected '=' after '~', found " +  
                            (next ? "'" + std::string(1, *next) + "'" : 
                                    std::string("EOF")));
                }
            }
            case '#': consume_line_comment(); break;
            case '<': {
                char_buff.next();                
                auto second = char_buff.consume();
                auto third = char_buff.peek();

                if (second == '<' && third == '-') {
                    char_buff.next(); // prevents <<->> from being a valid comment
                    auto e = consume_multi_line_comment();
                    if (!e) {
                        return e;
                    }
                    break;
                }

                // undos character advancement caused by attempting to read comment
                char_buff.back();
                char_buff.back();
                push_token_peek(TokenType::LESS_THAN_EQ, TokenType::LESS_THAN, '=');

                break;
            }
            case '"': {
                auto s = consume_string();
                if (!s) {
                    return s;
                }
                break;
            }
            case '.': {
                auto e = consume_float();
                if (!e) {
                    return e;
                }
                break;
            }
            default: {
                if (std::isspace(*c)) break;
                if (std::isdigit(*c)) {
                    auto n = consume_number();
                    if (!n) {
                        return n;
                    }
                    break;
                }
                if (std::isalnum(*c) || *c == '_') {
                    consume_ident();
                    break; 
                }

                return LexerError::create_error(LexerErrorType::UNEXPECTED_TOKEN, char_buff,
                    "Found illegal character: '" + std::string(1, *c) + "'");
            }
        }

        char_buff.next();
    }

    return {};
}

std::expected<void, LexerError> Lexer::consume_string() {

    std::u8string v;
    auto line_start = char_buff.get_line_number();
    auto col_start = char_buff.get_col_number();

    // skip initial quotation
    char_buff.next();

    // instead of simply returning errors on failures, strings need to keep
    // being consumed so the lexer doesn't keep consuming a string thinking
    // it's consuming something else
    std::optional<LexerError> err;

    while (true) {
        auto c = char_buff.consume();

        if (!c) {
            return LexerError::create_error(LexerErrorType::UNEXPECTED_EOF, char_buff,
                "Found EOF while consuming string");
        }

        if (!continue_on_err && err) {
            return std::unexpected(*err);
        }

        switch (*c) {
            case '"': {
                tokens.push_back({
                    TokenType::STRING,
                    line_start,
                    col_start,
                    TokenString {v}
                });

                if (err) {
                    return std::unexpected(*err);
                }
                
                return {};
            }
            case '\n': v += '\n'; break;
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
                    case '"':  v += '"';  break;
                    case '\n': continue;
                    case 'u': {
                        auto e = consume_unicode_char();
                        if (!e) {
                            err = e.error();
                        }
                        v += *e;
                        break;
                    }
                    default: {
                        err = LexerError::create_error(LexerErrorType::INVALID_ESCAPE, 
                                char_buff, "Invalid escape sequence '\\" + std::string(1, *next) 
                                + "' found while consuming string.").error();
                    }
                }

                char_buff.next();

                break;
            }
            default: v += *c; break;
        }
    }
}


std::expected<void, LexerError> Lexer::consume_number() {
    std::string v;
    auto line_start = char_buff.get_line_number();
    auto col_start = char_buff.get_col_number();


    while (true) {
        auto c = char_buff.peek();
        auto is_int = c && std::isdigit(*c);
        auto is_real = c && (*c == '.' || std::tolower(*c) == 'e');

        if (is_real) {
            return consume_float(v, line_start, col_start);
        }

        if(!is_int) {

            TokenValue token_value = TokenInteger{std::stoll(v)};
            
            tokens.push_back({
                TokenType::INTEGER,
                line_start,
                col_start,
                token_value
            });

            char_buff.back();
            
            return {};
        }

        v += *c;
        char_buff.next();
    }
}

std::expected<void, LexerError> Lexer::consume_float() {
    std::string v = "";

    return consume_float(v, char_buff.get_line_number(), 
                            char_buff.get_col_number());
}


std::expected<void, LexerError> Lexer::consume_float(std::string& context, std::size_t line_start, std::size_t col_start) {
    std::string& v = context; // alias
    
    bool has_decimal = false;
    bool has_e = false;
    bool done = false;
    

    while (true) {

        auto c = char_buff.peek();

        if (done || !c) {
            TokenValue token_value = TokenReal{std::stod(v)};

            tokens.push_back({
                TokenType::REAL_NUMBER,
                line_start,
                col_start,
                token_value
            });

            char_buff.back();

            return {};
        }
        

        switch (std::tolower(*c)) {
            case '.': {
                if (has_decimal || has_e) {
                    return LexerError::create_error(LexerErrorType::MALFORMED_REAL, char_buff,
                        "Read decimal point after exponent or decimal was already read in a real number.");
                }

                has_decimal = true;
                
                char_buff.next();
                auto next = char_buff.peek();

                if (v.empty() && !std::isdigit(*next)) {
                    char_buff.back(); // for line/col pos
                    push_token(TokenType::DOT);
                    return {};
                }
                
                // for instance: "1.e" should really be:
                // <TOKEN_REAL 1.> and <IDENT e> and
                // ".e" is <TOKEN_DOT> and <IDENT e>
                // The same applies with 1.-2 which should be
                // <TOKEN_REAL 1.> <TOKEN_SUB> <TOKEN_INT 2>
                if (!next || (std::tolower(*next) == 'e' || *next == '-' || *next == '+')) {
                    done = true;
                    continue;
                }

                v += *c;
                v += *next;

                break;
            }
            case 'e': {
                
                if (has_e) {
                    done = true;
                    continue;
                }

                char_buff.next();
                has_e = true;
                v += *c;

                auto next = char_buff.peek();

                if (next && (*next == '-' || *next == '+')) {
                    v += *next;
                    char_buff.next();
                    next = char_buff.peek();
                }

                if (next && std::isdigit(*next)) {
                    v += *next;
                    break;
                }

                char_buff.back();

                return LexerError::create_error(LexerErrorType::MALFORMED_REAL, char_buff,
                    "Expected integer after exponent, read " +
                        (next ? "'" + std::string(1, *next) + "'" : std::string("EOF")));
            }
            default: {
                if (!std::isdigit(*c)) {
                    done = true;
                    continue; // don't go to .next(), jump straight to top
                }

                v += *c;
                break;
            }
        }

        char_buff.next();
    }
}

void Lexer::consume_line_comment() {
    char_buff.next();

    while (auto c = char_buff.peek()) {
        if (!c || *c == '\n') {
            return;
        }
        char_buff.next();
    }
}

std::expected<void, LexerError> Lexer::consume_multi_line_comment() {
    while (true) {
        auto c = char_buff.consume();
        
        if (!c) {
            break;
        }

        if (*c == '-') {
            if (char_buff.peek() == '>') {
                char_buff.next();

                if (char_buff.peek() == '>') {
                    char_buff.next();
                    return {};
                }
            }
        }
    }

    return LexerError::create_error(LexerErrorType::UNEXPECTED_EOF, char_buff,
                "Expected to see block comment end with '->>', got EOF");
}

void Lexer::consume_ident() {
    std::string v;
    auto line_start = char_buff.get_line_number();
    auto col_start = char_buff.get_col_number();

    while (true) {
        auto c = char_buff.peek();
        if (!c || (!std::isalnum(*c) && *c != '_')) {

            tokens.push_back({
                TokenType::IDENTIFER,
                line_start,
                col_start,
                TokenIdentifier {v}
            });

            char_buff.back();

            return;
        }

        v += *c;

        char_buff.next();
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wconversion"

//    Char. number range  |        UTF-8 octet sequence
//       (hexadecimal)    |              (binary)
//    --------------------+---------------------------------------------
//                  00-7F | 0xxxxxxx
//                080-7FF | 110xxxxx 10xxxxxx
//              0800-FFFF | 1110xxxx 10xxxxxx 10xxxxxx
//        01 0000-10 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
std::expected<std::u8string, LexerError> Lexer::consume_unicode_char() {

    // ignore 'u' in \u
    char_buff.next();
    std::string hex;
    
    for (int i = 0; i < 6; i++) {
        auto c = char_buff.consume();

        if (!c || !std::isxdigit(*c)) {
            return LexerError::create_error(LexerErrorType::MALFORMED_UNICODE, char_buff,
                    "Found unexpected character while reading unicode character: " + 
                    (c ? "EOF" : "'" + std::string(1, *c) + "'"));
        }

        hex.push_back(*c);
    }

    char_buff.back();

    uint32_t hex_v = std::stoi(hex, nullptr, 16);
    std::u8string v;

    uint32_t bits_6 = 0x3f;
    uint32_t bits_5 = 0x1f;
    uint32_t bits_4 = 0xf;
    uint32_t bits_3 = 0x7;

    if (hex_v <= 0x7f) {
        v.push_back(hex_v);
        return v;
    }

    if (hex_v <= 0x7ff) {
        // 110xxxxx
        uint8_t first  = 0xC0;
        // 10xxxxxx
        uint8_t other = 0x80;
        
        v.push_back(first | (hex_v >> 6) & bits_5);
        v.push_back(other | hex_v & bits_6);

        return v;
    }

    if (hex_v <= 0xffff) {
        // 1110xxxx
        uint8_t first = 0xE0;
        // 10xxxxxx
        uint8_t other = 0x80;

        v.push_back(first  | (hex_v >> 12) & bits_4);
        v.push_back(other  | (hex_v >> 6)  & bits_6);
        v.push_back(other  | (hex_v)       & bits_6);
        return v;
    }

    if (hex_v <= 0x10ffff) {
        // 11110xxx
        uint8_t first = 0xF0;
        // 10xxxxxx
        uint8_t other = 0x80;

        v.push_back(first | (hex_v >> 18) & bits_3);
        v.push_back(other | (hex_v >> 12) & bits_6);
        v.push_back(other | (hex_v >> 6)  & bits_6);
        v.push_back(other | hex_v & bits_6);
        return v;
    }

    return LexerError::create_error(LexerErrorType::MALFORMED_UNICODE, char_buff,
              "Unicode encoding is >0x10ffff.");
}

#pragma GCC diagnostic pop