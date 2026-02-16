#include "ast.hpp"
#include "asterror.hpp"
#include "expression.hpp"
#include <algorithm>
#include <optional>
#include <vector>

std::vector<NodeResult> AbstractSyntaxTree::create(const Lexer& lexer_result) {
    const auto& tokens = lexer_result.get_tokens();
    std::span<const Token> token_span(tokens.data(), tokens.size());
    std::vector<NodeResult> v;

    while (!token_span.empty() && token_span.front().type != TokenType::END_OF_FILE) {
        auto exp = parse_expression(token_span);
        if (exp) {
            size_t consumed = *exp.get_expr_width();
            v.push_back(std::move(exp));
            token_span = token_span.subspan(consumed);
        } else if (exp.is_error()) {
            token_span = token_span.subspan(exp.error().skip_x_tok);
            v.push_back(std::move(exp));
        } 
    }
    
    return v; 
}

NodeResult AbstractSyntaxTree::parse_expression(std::span<const Token> tokens) {
    return parse_as(tokens)
        .or_else([&]() { 
            return NodeResult::error(AstError::bad_symbol(tokens.front())); 
        });
}

NodeResult AbstractSyntaxTree::parse_unary(std::span<const Token> tokens) {
    auto op = tokens.front().type;
    auto has_op = op == TokenType::ADD || op == TokenType::SUB;
    auto next_span = !has_op ? tokens : tokens.subspan(1);

    return parse_paren(next_span)
        .or_else([&]() { return parse_term(next_span); })
        .and_then_span([&](auto&& expr, auto&& expr_span) -> NodeResult {
            
            if (!has_op) {
                return NodeResult::just(std::move(expr));
            }

            return op == TokenType::SUB ?
                make_negated(std::move(expr), tokens.subspan(0, expr_span.size() + 1)) :
                make_term(std::move(expr), tokens.subspan(0, expr_span.size() + 1));
        });
}

NodeResult AbstractSyntaxTree::parse_paren(std::span<const Token> tokens) {
    
    if (tokens.front().type != TokenType::LEFT_PAREN) {
        return NodeResult::nothing();
    }

    return parse_expression(tokens.subspan(1))
        .and_then_span([&](auto&& expr, auto&& expr_span) {
            // this is safe, because even if it's the last character, it will be EOF
            auto last_tok = tokens.subspan(expr_span.size() + 1)[0];

            if (last_tok.type != TokenType::RIGHT_PAREN) {
                return NodeResult::error(AstError::mismatched_bracket(tokens.front()));
            }
            // trick the top level parser into thinking parens tokens
            // are apart of the full expression so AST parses correctly
            expr->token_span = tokens.subspan(0, expr_span.size() + 2);
            return NodeResult::just(std::move(expr));
        })
        .map_err([&](auto&& err) {

            // cascade empty parens error so it's only one error
            // instead of however many parens there was
            if (err.type == AstErrorType::EMPTY_PARENS) {
                return NodeResult::error(AstError::empty_parens(err));
            }

            if (err.type == AstErrorType::FAILED_TO_PARSE_SYMBOL 
                && err.offending_token->type == TokenType::RIGHT_PAREN) {
                return NodeResult::error(AstError::empty_parens(tokens));
            }

            return NodeResult::error(std::move(err));
        });
}


NodeResult AbstractSyntaxTree::parse_exp(std::span<const Token> tokens) {
    return parse_unary(tokens)
        .and_then_span([&](auto&& l_expr, auto&& l_expr_span) {

            auto rest = tokens.subspan(l_expr_span.size());
            auto op = rest.front();

            if (op.type != TokenType::EXPONENT) {
                return NodeResult::just(std::move(l_expr));
            }

            return parse_exp(rest.subspan(1))
                .and_then([&](auto&& r_expr) {
                return make_binary(op, 
                        NodeResult::just(std::move(l_expr)),
                        NodeResult::just(std::move(r_expr)));
                })
                .or_else([&]() {
                    // rhs failed to parse so just give the built lhs
                    // and let the expression parser deal with stray op
                    return NodeResult::just(std::move(l_expr));
                });
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
        .and_then_span([&](auto&& l_expr, auto&& l_expr_span) {
            return parse_binary_rest(
                    NodeResult::just(std::move(l_expr)),
                    tokens.subspan(l_expr_span.size()),
                    is_op_predicate, get_expr);
        });
}

NodeResult AbstractSyntaxTree::parse_binary_rest(NodeResult base, std::span<const Token> tokens, auto is_op_predicate, auto get_expr) {

    auto op = tokens.front();

    if (!is_op_predicate(op)) {
        return base;
    }

    return get_expr(tokens.subspan(1))
        .and_then_span([&](auto&& l_expr, auto&& l_expr_span) {
            auto r_subspan = tokens.subspan(l_expr_span.size() + 1);
            auto new_base = make_binary(op, std::move(base), 
                            std::move(NodeResult::just(std::move(l_expr))));
            return parse_binary_rest(std::move(new_base), r_subspan, is_op_predicate, get_expr);
        })
        .or_else([&]() {
            // rhs failed to parse so just give the built lhs
            // and let the expression parser deal with stray op
            return std::move(base);
        });
}


NodeResult AbstractSyntaxTree::parse_term(std::span<const Token> tokens) {
    return make_term(tokens.front(), tokens.subspan(0, 1));
}
