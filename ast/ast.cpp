#include "ast.hpp"
#include "asterror.hpp"
#include "expression.hpp"
#include <algorithm>
#include <optional>
#include <vector>

std::vector<NodeResult> AbstractSyntaxTree::create(const Lexer& lexerResult) {
    const auto& tokens = lexerResult.get_tokens();
    std::span<const Token> tokenSpan(tokens.data(), tokens.size());
    std::vector<NodeResult> v;

    while (!tokenSpan.empty() && tokenSpan.front().type != TokenType::END_OF_FILE) {
        auto exp = parse_expression(tokenSpan);
        if (exp) {
            size_t consumed = *exp.get_expr_width();
            v.push_back(std::move(exp));
            tokenSpan = tokenSpan.subspan(consumed);
        } else if (exp.is_error()) {
            v.push_back(std::move(exp));
            tokenSpan = tokenSpan.subspan(exp.error().skip_x_tok);
        } 
    }
    
    return v; 
}

NodeResult AbstractSyntaxTree::parse_expression(std::span<const Token> tokens) {
    return std::move(
        parse_as(tokens)
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
        // nested unaries are illegal
        return parse_paren(tokens)
                .or_else([&]() { return parse_term(tokens); });
    }

    auto token_subview = tokens.subspan(1);

    return parse_paren(token_subview)
           .or_else([&]() { return parse_term(token_subview); })
           .and_then([&](auto exp) -> NodeResult {
                auto width = 1 + exp->token_span.size();
                auto expr_span = tokens.subspan(0, width);

                return op.type ==
                    TokenType::SUB ?
                    make_negated(std::move(exp), expr_span) :
                    make_term(std::move(exp), expr_span);
            });
}

NodeResult AbstractSyntaxTree::parse_paren(std::span<const Token> tokens) {
    
    if (tokens.empty() || tokens.front().type != TokenType::LEFT_PAREN) {
        return NodeResult::nothing();
    }

    if (tokens.size() > 1 && tokens[1].type == TokenType::RIGHT_PAREN) {
        return NodeResult::error(AstError::empty_parens(tokens.subspan(0, 2)));
    }

    return parse_expression(tokens.subspan(1))
            .and_then([&](auto&& expr) {
                size_t inner_width = expr->token_span.size();
                size_t r_paren_idx = 1 + inner_width;

                if (r_paren_idx >= tokens.size() || 
                    tokens[r_paren_idx].type != TokenType::RIGHT_PAREN) {
                    return NodeResult::error(AstError::mismatched_bracket(tokens.front()));
                }

                expr->token_span = tokens.subspan(0, r_paren_idx + 1);
                return NodeResult::just(std::move(expr));
            })
            .map_err([&](auto&& e) {
                e.skip_x_tok += 1;

                if (e.skip_x_tok < tokens.size() &&
                    tokens[e.skip_x_tok].type == TokenType::RIGHT_PAREN) {
                    e.skip_x_tok += 1;
                    return NodeResult::error(std::move(e));
                }

                return NodeResult::error(AstError::mismatched_bracket(tokens.front()));
            });
}


NodeResult AbstractSyntaxTree::parse_exp(std::span<const Token> tokens) {
    
    auto l_expr = parse_unary(tokens);

    if (!l_expr) {
        return l_expr;
    }
    
    auto rest = tokens.subspan(*l_expr.get_expr_width());

    if (rest.empty()) {
        return l_expr;
    }

    auto op = rest.front();

    if (op.type != TokenType::EXPONENT) {
        return l_expr;
    }

    return parse_exp(rest.subspan(1))
           .and_then([&](auto&& r_expr) {
               return make_binary(op, std::move(l_expr), 
                                    std::move(NodeResult::just(std::move(r_expr))));
           })
           .map_err([&](auto&& e) {
               return NodeResult::error(AstError::malformed_rhs(rest.subspan(1), std::move(e)));
           });
}

NodeResult AbstractSyntaxTree::parse_md(std::span<const Token> tokens) {
    return parse_binary_base(tokens, [](const Token& t) -> bool {
        return t.type == TokenType::MULTIPLY || t.type == TokenType::DIVIDE
           || (t.type == TokenType::IDENTIFER && 
               std::get<TokenIdentifier>(t.data).value == "mod");
    }, [&](auto&& span) -> NodeResult {
        return parse_exp(span);
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
    return get_expr(tokens)
            .and_then([&](auto&& l_expr) {
                return parse_binary_rest(NodeResult::just(std::move(l_expr)),
                        tokens.subspan(l_expr->token_span.size()),
                        is_op_predicate, get_expr);
            });
}

NodeResult AbstractSyntaxTree::parse_binary_rest(NodeResult base, std::span<const Token> tokens, auto is_op_predicate, auto get_expr) {

    if (tokens.empty()) {
        return base;
    }

    auto op = tokens.front();

    if (!is_op_predicate(op)) {
        return base;
    }

    return get_expr(tokens.subspan(1))
            .and_then([&](auto&& l_expr) {
                auto r_subspan = tokens.subspan(l_expr->token_span.size() + 1);
                auto new_base = make_binary(op, std::move(base), 
                                std::move(NodeResult::just(std::move(l_expr))));
                return parse_binary_rest(std::move(new_base), r_subspan, is_op_predicate, get_expr);
            })
            .or_else([&]() {
                return NodeResult::error(AstError::malformed_rhs(tokens.subspan(1), AstError::bad_symbol(op)));
            })
            .map_err([&](auto&& e) {
                return NodeResult::error(AstError::malformed_rhs(tokens.subspan(1), std::move(e)));
            });
}


NodeResult AbstractSyntaxTree::parse_term(std::span<const Token> tokens) {
    if (tokens.empty()) {
        return NodeResult::nothing();
    }

    return make_term(tokens.front(), tokens.subspan(0, 1));
}
