#include "token.hpp"

std::string Token::get_type_string() {
    switch (type) {
        case TokenType::ADD: return "TOKEN_ADD";
        case TokenType::SUB: return "TOKEN_SUB"; 
        case TokenType::MULTIPLY: return "TOKEN_MULTIPLY";
        case TokenType::DIVIDE: return "TOKEN_DIVIDE";
        case TokenType::EXPONENT: return "TOKEN_EXPONENT";
        case TokenType::LESS_THAN: return "TOKEN_LESS_THAN";
        case TokenType::LESS_THAN_EQ: return "TOKEN_LESS_THAN_EQ";
        case TokenType::GREATER_THAN: return "TOKEN_GREATER_THAN";
        case TokenType::GREATER_THAN_EQ: return "TOKEN_GREATER_THAN_EQ";
        case TokenType::EQUALS: return "TOKEN_EQUALS";
        case TokenType::NOT_EQUALS: return "TOKEN_NOT_EQUALS";
        case TokenType::NOT: return "TOKEN_NOT";
        case TokenType::ASSIGN: return "TOKEN_ASSIGN";
        case TokenType::LEFT_PAREN: return "TOKEN_LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "TOKEN_RIGHT_PAREN";
        case TokenType::LEFT_BRACE: return "TOKEN_LEFT_BRACE";
        case TokenType::RIGHT_BRACE: return "TOKEN_RIGHT_BRACE";
        case TokenType::LEFT_SQ_BRACKET: return "TOKEN_LEFT_SQ_BRACKET";
        case TokenType::RIGHT_SQ_BRACKET: return "TOKEN_RIGHT_SQ_BRACKET";
        case TokenType::AND: return "TOKEN_AND";
        case TokenType::OR: return "TOKEN_OR";
        case TokenType::DOT: return "TOKEN_DOT";
        case TokenType::AT: return "TOKEN_AT";
        case TokenType::COLON: return "TOKEN_COLON";
        case TokenType::SEMICOLON: return "TOKEN_SEMICOLON";
        case TokenType::COMMA: return "TOKEN_COMMA";
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::STRING: {
            const auto &str = std::get<TokenString>(data);
            return "TOKEN_STRING: " + str.value;
        }
        case TokenType::IDENTIFER: {
            const auto &str = std::get<TokenIdentifier>(data);
            return "TOKEN_IDENT: " + str.value;
        }
        case TokenType::INTEGER: {
            const auto &i = std::get<TokenInteger>(data);
            return "TOKEN_INTEGER: " + std::to_string(i.value);
        }
        case TokenType::REAL_NUMBER: {
            const auto &f = std::get<TokenReal>(data);
            return "TOKEN_REAL: " + std::to_string(f.value);
        }
        default: return "TOKEN_UNKNOWN"; // should never return
    }
}