#pragma once

#include "ast.hpp"
#include <memory>


std::unique_ptr<Expression> AbstractSyntaxTree::create(const Lexer& lexerResult) {
    auto tokens = lexerResult.get_tokens();
    // remove EOF
    std::span<const Token> tokenSpan(tokens.data(), tokens.size() - 1);

    return std::move(*parseExpression(tokenSpan));
}

MaybeNode AbstractSyntaxTree::parseExpression(std::span<const Token> tokens) {
    return std::move(
        parseUnary(tokens)
        .or_else([&]() { return parseTerm(tokens); })
    );
}

MaybeNode AbstractSyntaxTree::parseUnary(std::span<const Token> tokens) {
    auto op = tokens.front().type;
    
    if (op != TokenType::ADD && op != TokenType::SUB) {
        return std::nullopt;
    }

    auto token_subview =
        std::span<const Token>(tokens.begin() + 1, tokens.end());

    return parseExpression(token_subview)
        .and_then([&](auto exp) -> MaybeNode {
            return op == 
                TokenType::SUB ?
                std::make_unique<Expression>(Negated({std::move(exp)})) :
                std::move(exp);

        });
}

template <typename T>
MaybeNode getExpressionNode(const Token& token) {
    auto tokenData = std::get<T>(token.data);
    Term termNode { TermValue{tokenData.value} };
    return std::make_unique<Expression>(std::move(termNode));
}

MaybeNode AbstractSyntaxTree::parseTerm(std::span<const Token> tokens) {
    if (tokens.size() != 1) {
        return std::nullopt;
    }

    auto token = tokens.front();

    switch (token.type) {
        case TokenType::STRING:      return getExpressionNode<TokenString>(token); 
        case TokenType::IDENTIFER:   return getExpressionNode<TokenIdentifier>(token); 
        case TokenType::REAL_NUMBER: return getExpressionNode<TokenReal>(token);
        case TokenType::INTEGER:     return getExpressionNode<TokenInteger>(token);
        default: return std::nullopt;
    }

}
