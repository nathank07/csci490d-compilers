#include "ast.hpp"
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
            size_t consumed = (*exp)->token_span.size();
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

NodeResult AbstractSyntaxTree::parse_md(std::span<const Token> tokens) {
    return parse_binary(tokens, [](const Token& t) -> bool {
        return t.type == TokenType::MULTIPLY || t.type == TokenType::DIVIDE
            || (t.type == TokenType::IDENTIFER && 
                std::get<TokenIdentifier>(t.data).value == std::string("mod"));
    });
}

NodeResult AbstractSyntaxTree::parse_as(std::span<const Token> tokens) {
    return parse_binary(tokens, [](const Token& t) -> bool {
        return t.type == TokenType::ADD || t.type == TokenType::SUB;
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

    return NodeResult::just(make_binary<Exp>(*l_expr, *r_expr));
}

NodeResult AbstractSyntaxTree::parse_binary(std::span<const Token> tokens, std::function<bool (const Token&)> is_op) {
    auto it = std::find_if(tokens.begin(), tokens.end(), is_op);

    if (it == tokens.end()) {
        return NodeResult::nothing();
    }

    auto pos = std::distance(tokens.begin(), it);

    if (pos == 0 || pos == tokens.size()) {
        return NodeResult::nothing();
    }

    auto r_span = tokens.subspan(pos + 1);
    auto l_span = tokens.subspan(0, pos);

    if (r_span.empty() || l_span.empty()) {
        return NodeResult::nothing();
    }

    auto l_expr = parse_expression(l_span);

    if (l_expr && ((*l_expr)->token_span.size()) != l_span.size()) {
        return NodeResult::nothing(); 
    }

    auto r_expr = parse_expression(r_span);

    // r_expr should not error out unless there is literally an error,
    // so just consume the left expression and the existing operand
    // and let top level loop deal with the error later
    if (r_expr.is_error()) {
        return l_expr.and_then([&](auto exp) -> NodeResult {
            exp->token_span = tokens.subspan(0 , pos + 1);
            return NodeResult::just(std::move(exp));
        });
    }

    // could be memoized (?) hilariously expensive solution for
    // left association
    auto next_valid_bin_exists = parse_binary(tokens, [&](const Token& t) {
       return is_op(t) && (&t > &(*it));
    });

    if (l_expr && r_expr && next_valid_bin_exists) {
        return parse_binary(tokens, [&](const Token& t) {
            return is_op(t) && (&t > &(*it));
        });
    }

    if (!l_expr || !r_expr) {
        return NodeResult::nothing();
    }

    switch (it->type) {
        case TokenType::ADD:       return NodeResult::just(make_binary<Add>(*l_expr, *r_expr));
        case TokenType::SUB:       return NodeResult::just(make_binary<Sub>(*l_expr, *r_expr));
        case TokenType::MULTIPLY:  return NodeResult::just(make_binary<Mult>(*l_expr, *r_expr));
        case TokenType::DIVIDE:    return NodeResult::just(make_binary<Div>(*l_expr, *r_expr));
        // since mod is the only ident
        case TokenType::IDENTIFER: return NodeResult::just(make_binary<Mod>(*l_expr, *r_expr));
        default: return NodeResult::nothing();
    }
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
