#include "ast.hpp"
#include "asterror.hpp"
#include "expression.hpp"
#include <algorithm>
#include <optional>
#include <vector>

#include <iostream>

std::vector<NodeResult> AbstractSyntaxTree::create(const Lexer& lexerResult) {
    const auto& tokens = lexerResult.get_tokens();
    std::span<const Token> tokenSpan(tokens.data(), tokens.size() - 1);
    std::vector<NodeResult> v;

    while (!tokenSpan.empty()) {
        auto exp = parse_expression(tokenSpan);
        if (exp) {
            size_t consumed = *exp.get_expr_width();
            v.push_back(std::move(exp));
            tokenSpan = tokenSpan.subspan(consumed);
        } else if (exp.is_error()) {
            std::cout << exp.error().message << "\n";
            tokenSpan = tokenSpan.subspan(exp.error().skip_x_tok);
        } else {
            std::cout << "unreachable point reached\n";
            tokenSpan = tokenSpan.subspan(1);
        }
    }
    
    return v; 
}


NodeResult AbstractSyntaxTree::parse_expression(std::span<const Token> tokens) {
    std::cout << "parse_expr called\n";
    return std::move(
        parse_as(tokens)
        .or_else([&]() { return parse_md(tokens); })
        .or_else([&]() { return parse_exp(tokens); })
        .or_else([&]() { return parse_unary(tokens); })
        .or_else([&]() { return parse_paren(tokens); })
        .or_else([&]() { return parse_term(tokens);  })
        .or_else([&]() { 
            return NodeResult::error(AstError::bad_symbol(tokens.front())); 
        })
    );
}

NodeResult AbstractSyntaxTree::parse_unary(std::span<const Token> tokens) {
    if (tokens.empty()) {
        return NodeResult::nothing();
    }
    
    auto op = tokens.front();
    
    if (op.type != TokenType::ADD && op.type != TokenType::SUB) {
        return NodeResult::nothing();
    }

    auto token_subview = tokens.subspan(1);

    return parse_paren(token_subview)
           .or_else([&]() { return parse_term(token_subview); })
           .and_then([&](auto exp) -> NodeResult {
                return op.type ==
                    TokenType::SUB ?
                    NodeResult::just(make_negated(std::move(exp), tokens)) :
                    NodeResult::just(std::move(exp));
            })
            .or_else([&]() { 
                return parse_unary(token_subview)
                .and_then([&](auto&&) { return NodeResult::error(AstError::extra_unary(op)); });
            });
}

std::optional<std::size_t> find_r_paren(std::span<const Token> tokens) {
    std::size_t l_parens = 0, idx = 0;
    
    for (auto& token : tokens) {

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

NodeResult AbstractSyntaxTree::parse_paren(std::span<const Token> tokens) {
    if (tokens.empty()) {
        return NodeResult::nothing();
    }

    auto paren = tokens.front();

    if (paren.type != TokenType::LEFT_PAREN) {
        return NodeResult::nothing();
    }

    auto idx = find_r_paren(tokens);

    if (!idx) {
        return NodeResult::error(AstError::mismatched_bracket(paren));
    }
    
    return parse_expression(tokens.subspan(1, *idx - 1))
            .and_then([&](auto exp) -> NodeResult {
                // trick the top level parser into thinking parens tokens
                // are apart of the full expression so AST parses correctly
                exp->token_span = tokens.subspan(0, *idx + 1);
                return NodeResult::just(std::move(exp));
            });
}

NodeResult AbstractSyntaxTree::parse_exp(std::span<const Token> tokens) {
    auto it = std::find_if(tokens.begin(), tokens.end(), [&](const Token& c) {
        return c.type == TokenType::EXPONENT;
    });

    if (it == tokens.end()) {
        return NodeResult::nothing();
    }

    auto pos = std::distance(tokens.begin(), it);
    auto l_expr = parse_expression(tokens.subspan(0, pos));
    auto r_expr = parse_expression(tokens.subspan(pos + 1));

    if (!l_expr || !r_expr) {
        return NodeResult::nothing();
    }

    return make_binary(*it, std::move(l_expr), std::move(r_expr));
}

NodeResult AbstractSyntaxTree::parse_md(std::span<const Token> tokens) {
    return parse_binary_base(tokens, [](const Token& t) -> bool {
        return t.type == TokenType::MULTIPLY || t.type == TokenType::DIVIDE
           || (t.type == TokenType::IDENTIFER && 
               std::get<TokenIdentifier>(t.data).value == std::string("mod"));
    }, [&](auto&& span) -> NodeResult {
        return parse_unary(span)
        .or_else([&]() { return parse_paren(span); })
        .or_else([&]() { return parse_term(span);  });
    });
}

NodeResult AbstractSyntaxTree::parse_as(std::span<const Token> tokens) {
    return parse_binary_base(tokens, [](const Token& t) -> bool {
        return t.type == TokenType::ADD || t.type == TokenType::SUB;
    }, [&](auto&& span) -> NodeResult {
        return parse_md(span);
    });
}

NodeResult AbstractSyntaxTree::parse_binary_base(std::span<const Token> tokens, auto is_op_predicate, auto get_expr) {
    auto l_expr = get_expr(tokens);

    if (!l_expr) {
        return NodeResult::nothing();
    }

    return parse_binary_rest(std::move(l_expr), tokens.subspan(*l_expr.get_expr_width()), is_op_predicate, get_expr);
}

NodeResult AbstractSyntaxTree::parse_binary_rest(NodeResult base, std::span<const Token> tokens, auto is_op_predicate, auto get_expr) {

    if (tokens.empty()) {
        return base;
    }

    auto op = tokens.front();

    if (!is_op_predicate(op)) {
        return base;
    }
    
    auto l_expr = get_expr(tokens.subspan(1));

    if (!l_expr) {
        return NodeResult::error(AstError::bad_symbol(op));
    }

    auto r_subspan = tokens.subspan(*l_expr.get_expr_width() + 1);
    auto new_base = make_binary(op, std::move(base), std::move(l_expr));

    return parse_binary_rest(std::move(new_base), r_subspan, is_op_predicate, get_expr);
}


NodeResult AbstractSyntaxTree::parse_term(std::span<const Token> tokens) {
    if (tokens.empty()) {
        return NodeResult::nothing();
    }

    auto token = tokens.front();

    auto get_node = [&]<typename T>(const Token& t) {
        auto tokenData = std::get<T>(t.data);
        return NodeResult::just(make_term(Term{TermValue{tokenData.value}}, tokens.subspan(0, 1)));
    };

    switch (token.type) {
        case TokenType::STRING:      return get_node.operator()<TokenString>(token); 
        case TokenType::IDENTIFER:   return get_node.operator()<TokenIdentifier>(token); 
        case TokenType::REAL_NUMBER: return get_node.operator()<TokenReal>(token);
        case TokenType::INTEGER:     return get_node.operator()<TokenInteger>(token);
        default: return NodeResult::nothing();
    }

}
