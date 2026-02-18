#include "token.hpp"
#include <sstream>

std::string Token::get_type_string() const {
    switch (type) {
        case TokenType::ADD: return "TOKEN_PLUS";
        case TokenType::SUB: return "TOKEN_MINUS"; 
        case TokenType::MULTIPLY: return "TOKEN_MULT";
        case TokenType::DIVIDE: return "TOKEN_DIV";
        case TokenType::EXPONENT: return "TOKEN_EXP";
        case TokenType::LESS_THAN: return "TOKEN_LESS";
        case TokenType::LESS_THAN_EQ: return "TOKEN_LESS_EQ";
        case TokenType::GREATER_THAN: return "TOKEN_GREATER";
        case TokenType::GREATER_THAN_EQ: return "TOKEN_GREATER_EQ";
        case TokenType::EQUALS: return "TOKEN_EQUAL";
        case TokenType::NOT_EQUALS: return "TOKEN_NOT_EQUAL";
        case TokenType::NOT: return "TOKEN_NOT";
        case TokenType::ASSIGN: return "TOKEN_ASSIGN";
        case TokenType::LEFT_PAREN: return "TOKEN_LPAREN";
        case TokenType::RIGHT_PAREN: return "TOKEN_RPAREN";
        case TokenType::LEFT_BRACE: return "TOKEN_LBRACE";
        case TokenType::RIGHT_BRACE: return "TOKEN_RBRACE";
        case TokenType::LEFT_SQ_BRACKET: return "TOKEN_LBRACKET";
        case TokenType::RIGHT_SQ_BRACKET: return "TOKEN_RBRACKET";
        case TokenType::AND: return "TOKEN_AND";
        case TokenType::OR: return "TOKEN_OR";
        case TokenType::DOT: return "TOKEN_DOT";
        case TokenType::AT: return "TOKEN_AT";
        case TokenType::COLON: return "TOKEN_COLON";
        case TokenType::SEMICOLON: return "TOKEN_SEMICOLON";
        case TokenType::COMMA: return "TOKEN_COMMA";
        case TokenType::END_OF_FILE: return "TOKEN_EOF";
        case TokenType::STRING: {
            const auto &str = std::get<TokenString>(data);
            std::string s(str.value.begin(), str.value.end());
            return "TOKEN_STRING: " + s + " ";
        }
        case TokenType::IDENTIFIER: {
            const auto &str = std::get<TokenIdentifier>(data);
            return "TOKEN_IDENT: " + str.value + " ";
        }
        case TokenType::INTEGER: {
            const auto &i = std::get<TokenInteger>(data);
            return "TOKEN_INTEGER: " + std::to_string(i.value) + " ";
        }
        case TokenType::REAL_NUMBER: {
            const auto &f = std::get<TokenReal>(data);
            std::stringstream ss;
            ss <<  f.value;
            return "TOKEN_REAL " + ss.str() + " ";
        }
        default: return "TOKEN_UNKNOWN"; // should never return
    }
}

std::size_t Token::get_token_width() const {
    switch (type) {
        case TokenType::ADD: return 1;
        case TokenType::SUB: return 1; 
        case TokenType::MULTIPLY: return 1;
        case TokenType::DIVIDE: return 1;
        case TokenType::EXPONENT: return 1;
        case TokenType::LESS_THAN: return 1;
        case TokenType::LESS_THAN_EQ: return 2;
        case TokenType::GREATER_THAN: return 1;
        case TokenType::GREATER_THAN_EQ: return 2;
        case TokenType::EQUALS: return 1;
        case TokenType::NOT_EQUALS: return 2;
        case TokenType::NOT: return 1;
        case TokenType::ASSIGN: return 2;
        case TokenType::LEFT_PAREN: return 1;
        case TokenType::RIGHT_PAREN: return 1;
        case TokenType::LEFT_BRACE: return 1;
        case TokenType::RIGHT_BRACE: return 1;
        case TokenType::LEFT_SQ_BRACKET: return 1;
        case TokenType::RIGHT_SQ_BRACKET: return 1;
        case TokenType::AND: return 1;
        case TokenType::OR: return 1;
        case TokenType::DOT: return 1;
        case TokenType::AT: return 1;
        case TokenType::COLON: return 1;
        case TokenType::SEMICOLON: return 1;
        case TokenType::COMMA: return 1;
        case TokenType::END_OF_FILE: return 0;
        case TokenType::STRING: {
            const auto &str = std::get<TokenString>(data);
            return str.value.length();
        }
        case TokenType::IDENTIFIER: {
            const auto &str = std::get<TokenIdentifier>(data);
            return str.value.length();
        }
        case TokenType::INTEGER: {
            const auto &i = std::get<TokenInteger>(data);
            return std::to_string(i.value).length();
        }
        case TokenType::REAL_NUMBER: {
            const auto &f = std::get<TokenReal>(data);
            std::stringstream ss;
            ss << f.value;
            return ss.str().length();
        }
        default: return 0; // should never return
    }
}