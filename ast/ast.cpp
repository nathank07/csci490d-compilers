#pragma once

#include "ast.hpp"
#include <algorithm>
#include <memory>
#include <optional>

MaybeNode AbstractSyntaxTree::create(const Lexer& lexerResult) {
    auto tokens = lexerResult.get_tokens();
    // remove EOF
    std::span<const Token> tokenSpan(tokens.data(), tokens.size() - 1);

    return parse_expression(tokenSpan);
}

MaybeNode AbstractSyntaxTree::parse_expression(std::span<const Token> tokens) {
    return std::move(
        parse_as(tokens)
        .or_else([&]() { return parse_md(tokens); })
        .or_else([&]() { return parse_exp(tokens); })
        .or_else([&]() { return parse_unary(tokens); })
        .or_else([&]() { return parse_paren(tokens); })
        .or_else([&]() { return parse_term(tokens);  })
    );
}

MaybeNode AbstractSyntaxTree::parse_unary(std::span<const Token> tokens) {
    auto op = tokens.front().type;
    
    if (op != TokenType::ADD && op != TokenType::SUB) {
        return std::nullopt;
    }

    auto token_subview = tokens.subspan(1);

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
    return parse_binary(tokens, [](const Token& t) -> bool {
        return t.type == TokenType::MULTIPLY || t.type == TokenType::DIVIDE
            || (t.type == TokenType::IDENTIFER && 
                std::get<TokenIdentifier>(t.data).value == std::string("mod"));
    });
}

MaybeNode AbstractSyntaxTree::parse_as(std::span<const Token> tokens) {
    return parse_binary(tokens, [](const Token& t) -> bool {
        return t.type == TokenType::ADD || t.type == TokenType::SUB;
    });
}

MaybeNode AbstractSyntaxTree::parse_exp(std::span<const Token> tokens) {
    auto it = std::find_if(tokens.begin(), tokens.end(), [&](const Token& c) {
        return c.type == TokenType::EXPONENT;
    });

    if (it == tokens.end()) {
        return std::nullopt;
    }

    auto pos = std::distance(tokens.begin(), it);
    auto l_expr = parse_expression(tokens.subspan(0, pos));
    auto r_expr = parse_expression(tokens.subspan(pos + 1));

    if (!l_expr || !r_expr) {
        return std::nullopt;
    }

    return std::make_unique<Expression>(Exp{ std::move(*l_expr), std::move(*r_expr) });
}

MaybeNode AbstractSyntaxTree::parse_binary(std::span<const Token> tokens, std::function<bool (const Token&)> is_op) {
    auto it = std::find_if(tokens.rbegin(), tokens.rend(), is_op);

    if (it == tokens.rend()) {
        return std::nullopt;
    }

    auto dist = std::distance(tokens.rbegin(), it);
    auto pos = tokens.size() - 1 - dist;

    if (pos == 0 || pos == tokens.size() - 1) {
        return std::nullopt;
    }

    auto l_expr = parse_expression(tokens.subspan(0, pos));
    auto r_expr = parse_expression(tokens.subspan(pos + 1));

    // We may have been looking at a unary operator, so keep
    // searching for a binary operator...
    if (!l_expr) {
        return parse_binary(tokens, [&](const Token& t) {
            return is_op(t) && (&t < &(*it));
        });
    }

    if (!r_expr) {
        return std::nullopt;
    }

    auto create_node = [&]<typename T>() {
        return std::make_unique<Expression>(T{ std::move(*l_expr), std::move(*r_expr) });
    };

    switch (it->type) {
        case TokenType::ADD:       return create_node.operator()<Add>();
        case TokenType::SUB:       return create_node.operator()<Sub>();
        case TokenType::MULTIPLY:  return create_node.operator()<Mult>();
        case TokenType::DIVIDE:    return create_node.operator()<Div>();
        // since mod is the only ident
        case TokenType::IDENTIFER: return create_node.operator()<Mod>();
        default: return std::nullopt;
    }
}


MaybeNode AbstractSyntaxTree::parse_term(std::span<const Token> tokens) {
    if (tokens.size() == 0) {
        return std::nullopt;
    }

    auto token = tokens.front();

    auto get_node = []<typename T>(const Token& t) {
        auto tokenData = std::get<T>(t.data);
        Term termNode { TermValue{tokenData.value} };
        return std::make_unique<Expression>(std::move(termNode));
    };

    switch (token.type) {
        case TokenType::STRING:      return get_node.operator()<TokenString>(token); 
        case TokenType::IDENTIFER:   return get_node.operator()<TokenIdentifier>(token); 
        case TokenType::REAL_NUMBER: return get_node.operator()<TokenReal>(token);
        case TokenType::INTEGER:     return get_node.operator()<TokenInteger>(token);
        default: return std::nullopt;
    }

}
