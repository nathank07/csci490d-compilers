#pragma once

#include "ast.hpp"
#include <memory>

MaybeNode AbstractSyntaxTree::create(const Lexer& lexerResult) {
    auto tokens = lexerResult.get_tokens();
    // remove EOF
    std::span<const Token> tokenSpan(tokens.data(), tokens.size() - 1);

    return parse_expression(tokenSpan);
}

MaybeNode AbstractSyntaxTree::parse_expression(std::span<const Token> tokens) {
    return std::move(
        parse_unary(tokens)
        .or_else([&]() { return parse_paren(tokens); })
        .or_else([&]() { return parse_term(tokens);  })
    );
}

MaybeNode AbstractSyntaxTree::parse_unary(std::span<const Token> tokens) {
    auto op = tokens.front().type;
    
    if (op != TokenType::ADD && op != TokenType::SUB) {
        return std::nullopt;
    }

    auto token_subview =
        std::span<const Token>(tokens.begin() + 1, tokens.end());

    return parse_expression(token_subview)
        .and_then([&](auto exp) -> MaybeNode {
            return op == 
                TokenType::SUB ?
                std::make_unique<Expression>(Negated({std::move(exp)})) :
                std::move(exp);
        });
}

std::optional<std::size_t> find_r_paren(std::span<const Token> tokens) {
    std::size_t l_parens = 0;
    
    for (int idx = 0; auto& token : tokens) {

        if (token.type == TokenType::LEFT_PAREN) {
            l_parens++;
        }

        if (token.type == TokenType::RIGHT_PAREN) {
            l_parens--;
        }

        if (l_parens == 0) {
            return idx;
        }

        idx++;
    }

    return std::nullopt;
}

MaybeNode AbstractSyntaxTree::parse_paren(std::span<const Token> tokens) {
    auto paren = tokens.front().type;

    if (paren != TokenType::LEFT_PAREN) {
        return std::nullopt;
    }

    return find_r_paren(tokens)
        .and_then([&](auto idx){
            auto token_subview =
                std::span<const Token>(tokens.begin() + 1, tokens.begin() + idx);
            
            return parse_expression(token_subview);
        });
}

MaybeNode AbstractSyntaxTree::parse_md(std::span<const Token> tokens) {
    
}

template <typename T>
MaybeNode get_expression_node(const Token& token) {
    auto tokenData = std::get<T>(token.data);
    Term termNode { TermValue{tokenData.value} };
    return std::make_unique<Expression>(std::move(termNode));
}

MaybeNode AbstractSyntaxTree::parse_term(std::span<const Token> tokens) {
    if (tokens.size() != 1) {
        return std::nullopt;
    }

    auto token = tokens.front();

    switch (token.type) {
        case TokenType::STRING:      return get_expression_node<TokenString>(token); 
        case TokenType::IDENTIFER:   return get_expression_node<TokenIdentifier>(token); 
        case TokenType::REAL_NUMBER: return get_expression_node<TokenReal>(token);
        case TokenType::INTEGER:     return get_expression_node<TokenInteger>(token);
        default: return std::nullopt;
    }

}
