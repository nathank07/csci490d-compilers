#include "ast.hpp"
#include "asterror.hpp"
#include "expression.hpp"
#include "../utils.hpp"
#include "parseresult.hpp"
#include <algorithm>
#include <functional>
#include <memory>
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
            size_t consumed = exp.size();
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
        if(!node.is_error())
            expressions.push_back(std::move(*node));
            
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
        },
        [&](const FunctionCall& v) {
            o << "call\n";
            print_tree(o, v.ident, indent + 2);
            if (v.args) print_tree(o, v.args, indent + 2);
        },
        [&](const FunctionCallArgList& v) {
            o << "arg\n";
            print_tree(o, v.value, indent + 2);
            if (v.next) print_tree(o, v.next, indent);
        },
        [&](const Declaration& v) {
            o << "decl\n";
            print_tree(o, v.type_ident, indent + 2);
            print_tree(o, v.declared_ident, indent + 2);
        },
        [&](const Assign& v) {
            o << "assigns\n";
            print_tree(o, v.ident, indent + 2);
            print_tree(o, v.value, indent + 2);
        }
    };

    std::visit(visitor, tree->expression);
}

NodeResult AbstractSyntaxTree::parse_expression(std::span<const Token> tokens) {
    return NodeResult::nothing(tokens)
        .or_else([this](auto&& ctx) { return parse_declaration(std::move(ctx)); })
        .or_else([this](auto&& ctx) { return parse_assigns(std::move(ctx)); })
        .or_else([this](auto&& ctx) { return parse_as(std::move(ctx)); });
}

NodeResult AbstractSyntaxTree::parse_expression(NodeResult ctx) {
    return NodeResult::nothing(ctx.rest)
        .or_else([this](auto&& ctx) { return parse_declaration(std::move(ctx)); })
        .or_else([this](auto&& ctx) { return parse_assigns(std::move(ctx)); })
        .or_else([this](auto&& ctx) { return parse_as(std::move(ctx)); });
}

NodeResult AbstractSyntaxTree::parse_paren(NodeResult ctx) {
    return ctx
        .want_tok(TokenType::LEFT_PAREN)
        .and_then([this](auto rest) { return parse_expression(std::move(rest)); })
        .then_expect_tok(TokenType::RIGHT_PAREN);        
}

NodeResult AbstractSyntaxTree::parse_unary(NodeResult ctx) {

    auto parse_expr = [this](auto&& rest) {
        return parse_function(std::move(rest))
            .or_else([this](auto&& c) { return parse_paren(std::move(c)); })
            .or_else([this](auto&& c) { return parse_term(std::move(c)); });
    };
    
    return ctx
        .want_tok(TokenType::SUB)
        .and_then([this](auto&& rest) {
            return make_negated(parse_expression(std::move(rest)));
        })
        .or_else([&](auto&& c) {
            return c
                .want_tok(TokenType::ADD)
                .and_then(parse_expr)
                .or_else(parse_expr);
        });
}

NodeResult AbstractSyntaxTree::parse_as(NodeResult ctx) {

    return ctx
        .want_left_sep(
            [this](auto&& ctx) { return parse_md(std::move(ctx)); },
            [](auto&& rest) {
                return rest
                    .want_tok(TokenType::ADD)
                    .or_want_tok(TokenType::SUB);
            },
            [this](auto&& lhs, auto&& rhs) {
                auto& op_tok = rhs.consumed.front();
                return make_binary(op_tok, std::move(lhs), std::move(rhs));
            }
        );
}

NodeResult AbstractSyntaxTree::parse_md(NodeResult ctx) {

    return ctx
        .want_left_sep(
            [this](auto&& ctx) { return parse_exp(std::move(ctx)); },
            [](auto&& rest) {
                return rest
                    .want_tok(TokenType::MULTIPLY)
                    .or_want_tok(TokenType::DIVIDE)
                    .or_want_ident("mod");
            },
            [this](auto&& lhs, auto&& rhs) {
                auto& op_tok = rhs.consumed.front();
                return make_binary(op_tok, std::move(lhs), std::move(rhs));
            }
        );
}

NodeResult AbstractSyntaxTree::parse_exp(NodeResult ctx) {

    return ctx
        .want_right_sep(
            [this](auto&& ctx) { return parse_unary(std::move(ctx)); },
            [](auto&& rest) { return rest.want_tok(TokenType::EXPONENT); },
            [this](auto&& lhs, auto&& rhs) {
                auto& op_tok = rhs.consumed.front();
                return make_binary(op_tok, std::move(lhs), std::move(rhs));
            });
}

NodeResult AbstractSyntaxTree::parse_term(NodeResult ctx) {

    auto make = [](auto&& cont) { return make_term(std::move(cont)); };

    return ctx
        .want_tok(TokenType::IDENTIFIER, make)
        .or_want_tok(TokenType::INTEGER, make)
        .or_want_tok(TokenType::STRING, make)
        .or_expect_tok(TokenType::REAL_NUMBER, make);
}

NodeResult AbstractSyntaxTree::parse_function(NodeResult ctx) {

    auto make = [](auto&& cont) { return make_term(std::move(cont)); };
    auto parse = [this](auto&& ctx) { return parse_expression(std::move(ctx)); };
    auto sep = [](auto&& rest) { return rest.want_tok(TokenType::COMMA); };
    auto make_args = [](auto&& lhs, auto&& rhs) {
        return make_func_args(std::move(lhs), std::move(rhs));
    };

    return ctx
        .want_tok(TokenType::IDENTIFIER, make)
        .then_want_tok(TokenType::LEFT_PAREN, [&](auto&& ident, auto&& rest) {
            auto args = rest.then_want_right_sep(parse, sep, make_args);
            return make_func(std::move(ident), std::move(args));
        })
        .then_expect_tok(TokenType::RIGHT_PAREN);
}

NodeResult AbstractSyntaxTree::parse_declaration(NodeResult ctx) {

    auto make = [](auto&& cont) { return make_term(std::move(cont)); };

    auto make_declaration = [](auto&& type, auto&& name) { 
        return std::move(make_decl(std::move(type), std::move(name))); };

    return ctx
        .want_tok(TokenType::IDENTIFIER, make)
        .then_want_tok(TokenType::IDENTIFIER, make_declaration)
        .then_expect_tok(TokenType::SEMICOLON);
}

NodeResult AbstractSyntaxTree::parse_assigns(NodeResult ctx) {

    auto make = [](auto&& cont) { return make_term(std::move(cont)); };


    return ctx
        .want_tok(TokenType::IDENTIFIER, make)
        .then_want_tok(TokenType::ASSIGN)
        .and_then([this](auto&& ident, auto&& rest) {
            return make_assign(std::move(ident), parse_expression(std::move(rest)));
        })
        .then_expect_tok(TokenType::SEMICOLON);
}
