#include "ast.hpp"
#include "asterror.hpp"
#include "expression.hpp"
#include "../utils.hpp"
#include <algorithm>
#include <optional>
#include <vector>
#include <variant>

std::vector<NodeResult> AbstractSyntaxTree::create(const Lexer& lexer_result) {
    AbstractSyntaxTree factory;
    const auto& tokens = lexer_result.get_tokens();
    std::span<const Token> token_span(tokens.data(), tokens.size());
    std::vector<NodeResult> v;

    while (!token_span.empty() && token_span.front().type != TokenType::END_OF_FILE) {
        auto exp = factory.parse_expression(token_span);
        if (exp) {
            size_t consumed = *exp.get_expr_width();
            v.push_back(std::move(exp));
            token_span = token_span.subspan(consumed);
        } else if (exp.is_error()) {
            token_span = token_span.subspan(exp.error().skip_x_tok);
            auto& err = exp.error();

            // for instance, "1 + 1 + (1" will propogate the same
            // paren mismatch error due to constantly trying to evaluate the rhs
            // and print multiple of the same message. the easiest fix for this is 
            // to just remove duplicates
            bool err_in_vec = std::any_of(v.begin(), v.end(), [&](auto& node) {
                return node.is_error()
                    && node.error().type == err.type
                    && node.error().offending_token.column_number 
                             == err.offending_token.column_number
                    && node.error().offending_token.line_number
                             == err.offending_token.line_number;
            });

            if (!err_in_vec) v.push_back(std::move(exp));
        } 
    }
    
    return v; 
}

std::vector<std::unique_ptr<Expression>> AbstractSyntaxTree::unwrap_valid_nodes(std::vector<NodeResult>& node_v) {
    std::vector<std::unique_ptr<Expression>> expressions;

    for (auto& node : node_v) {
        node.and_then([&](auto&& expr) {
            expressions.push_back(std::move(expr));
            return NodeResult::just(std::move(expr));
        });
    }

    return expressions;
}

void AbstractSyntaxTree::print_tree(std::ostream& o, const std::unique_ptr<Expression>& tree, std::size_t indent) {
    if (!tree) 
        return;

    o << std::string(indent, ' ');
    
    const auto visitor = overloads 
    {
        [&](std::monostate) { o << "empty"; },
        [&](const Term& t) { 
            auto v = overloads {
                [&](std::string s) {
                    o << s;
                },
                [&](std::u8string s) {
                    o << "\"" << std::string(s.begin(), s.end()) << "\"";
                },
                [&](long long l) {
                    o << l;
                },
                [&](double d) {
                    o << d;
                }
            };
            
            std::visit(v, t.v);
            o << "\n";
        },
        [&](const Negated& e) {
            o << "- (neg)\n";
            print_tree(o, e.expression, indent + 2); 
        },
        [&](const Add& v) { 
            o << "+ (add)\n";
            print_tree(o, v.left,  indent + 2);
            print_tree(o, v.right, indent + 2);
        },
        [&](const Sub& v) { 
            o << "- (sub)\n";
            print_tree(o, v.left, indent + 2);
            print_tree(o, v.right, indent + 2);
        },
        [&](const Mult& v) { 
            o << "* (mult)\n";
            print_tree(o, v.left, indent + 2);
            print_tree(o, v.right, indent + 2);
        },
        [&](const Div& v) { 
            o << "/ (div)\n";
            print_tree(o, v.left, indent + 2);
            print_tree(o, v.right, indent + 2);
        },
        [&](const Mod& v) { 
            o << "mod \n";
            print_tree(o, v.left, indent + 2);
            print_tree(o, v.right, indent + 2);
        },
        [&](const Exp& v) { 
            o << "^ (pow)\n";
            print_tree(o, v.base, indent + 2);
            print_tree(o, v.exponent, indent + 2);
        }
    };

    std::visit(visitor, tree->expression);
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
            auto last_tok = tokens[expr_span.size() + 1];

            if (last_tok.type != TokenType::RIGHT_PAREN) {
                return NodeResult::error(AstError::mismatched_bracket(tokens.front(), expr_span.back()));
            }
            // trick the top level parser into thinking parens tokens
            // are apart of the full expression so AST parses correctly
            expr->token_span = tokens.subspan(0, expr_span.size() + 2);
            return NodeResult::just(std::move(expr));
        })
        .map_err([&](auto&& err) {

            // cascade empty parens error so it's only one error
            // instead of however many parens there was, this will cause
            // ((((() to be matched as one empty_parens but it's not the
            // end of the world because things like ((1() will get properly 
            // flagged
            if (err.type == AstErrorType::EMPTY_PARENS) {
                return NodeResult::error(AstError::empty_parens(err));
            }

            if (err.type == AstErrorType::FAILED_TO_PARSE_SYMBOL) {

                if (err.offending_token.type == TokenType::RIGHT_PAREN) {
                    return NodeResult::error(AstError::empty_parens(tokens.front(), err.offending_token));
                }

                if (err.offending_token.type == TokenType::END_OF_FILE) {
                    return NodeResult::error(AstError::mismatched_bracket(tokens.front(), err.offending_token));
                }

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
           || (t.type == TokenType::IDENTIFIER && 
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
                is_op_predicate, get_expr
            );
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
