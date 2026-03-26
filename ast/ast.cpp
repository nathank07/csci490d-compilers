#include "ast.hpp"
#include "asterror.hpp"
#include "expression.hpp"
#include "../utils.hpp"
#include "parseresult.hpp"
#include <algorithm>
#include <memory>
#include <vector>
#include <variant>

std::vector<NodeResult> AbstractSyntaxTree::create(const Lexer& lexer_result) {
    const auto& tokens = lexer_result.get_tokens();
    std::span<const Token> token_span(tokens.data(), tokens.size());
    std::vector<NodeResult> v;

    while (!token_span.empty() && token_span.front().type != TokenType::END_OF_FILE) {
        auto exp = parse_global(NodeResult::nothing(token_span));
        if (exp.is_error()) {
            token_span = token_span.subspan(std::max(exp.size(), std::size_t{1}));
        } else { token_span = token_span.subspan(exp.size()); }
        v.push_back(std::move(exp));
    }
    
    return v; 
}


NodeResult AbstractSyntaxTree::parse_global(NodeResult ctx) {
    return ctx
        .collect_until_tok(
            expect_statement,
            TokenType::END_OF_FILE,
            make_statements
        )
        .then_parse_rest(make_statement_block);
}

auto constexpr AbstractSyntaxTree::parse_semicoloned(auto&& f) {
    return [&](auto&& ctx) {
        return f(std::move(ctx))
            .then_expect_tok(TokenType::SEMICOLON);
    };
}

NodeResult AbstractSyntaxTree::parse_statement(NodeResult ctx) {
    return NodeResult::nothing(ctx.rest)
        .or_try(parse_assigns)
        .or_try(parse_declaration)
        .or_try(parse_semicoloned(parse_function_call))
        .or_try(parse_semicoloned(parse_expression));
}

NodeResult AbstractSyntaxTree::expect_statement(NodeResult ctx) {
    return parse_statement(std::move(ctx))
        .nothing_guard(AstErrorType::EXPECTED_STATEMENT);
}

NodeResult AbstractSyntaxTree::parse_expression(NodeResult ctx) {
    return parse_as(NodeResult::nothing(ctx.rest));
}

NodeResult AbstractSyntaxTree::expect_expression(NodeResult ctx) {
    return parse_as(NodeResult::nothing(ctx.rest))
        .nothing_guard(AstErrorType::EXPECTED_EXPRESSION);
}

NodeResult AbstractSyntaxTree::parse_paren(NodeResult ctx) {
    return ctx
        .want_tok(TokenType::LEFT_PAREN)
        .then_parse_rest(parse_expression)
        .then_expect_tok(TokenType::RIGHT_PAREN);        
}

NodeResult AbstractSyntaxTree::parse_unary(NodeResult ctx) {

    auto parse_expr = [](auto&& rest) {
        return parse_function_call(std::move(rest))
            .or_try(parse_paren)
            .or_try(parse_term);
    };
    
    return ctx
        .want_tok(TokenType::SUB)
        .then_parse_rest([parse_expr](auto&& rest) {
            return make_negated(parse_expr(std::move(rest))
                .nothing_guard(AstErrorType::NO_NESTED_UNARIES));
        })
        .or_try([&](auto&& c) {
            return c
                .want_tok(TokenType::ADD)
                .then_parse_rest(parse_expr);
        })
        .or_try(parse_expr);
}

NodeResult AbstractSyntaxTree::parse_as(NodeResult ctx) {
    return ctx
        .want_left_sep(
            parse_md,
            [](auto&& rest) {
                return rest
                    .want_tok(TokenType::ADD)
                    .or_want_tok(TokenType::SUB);
            },
            parse_binary
        );
}

NodeResult AbstractSyntaxTree::parse_md(NodeResult ctx) {

    return ctx
        .want_left_sep(
            parse_exp,
            [](auto&& rest) {
                return rest
                    .want_tok(TokenType::MULTIPLY)
                    .or_want_tok(TokenType::DIVIDE)
                    .or_want_ident("mod");
            },
            parse_binary
        );
}

NodeResult AbstractSyntaxTree::parse_exp(NodeResult ctx) {
    return ctx
        .want_right_sep(
            parse_unary,
            [](auto&& rest) { 
                return rest
                    .want_tok(TokenType::EXPONENT); 
            },
            parse_binary
        );
}

NodeResult AbstractSyntaxTree::parse_term(NodeResult ctx) {
    return ctx
        .want_tok(TokenType::IDENTIFIER, make_term)
        .or_want_tok(TokenType::INTEGER, make_term)
        .or_want_tok(TokenType::STRING, make_term)
        .or_want_tok(TokenType::REAL_NUMBER, make_term);
}

NodeResult AbstractSyntaxTree::parse_function_call(NodeResult ctx) {

    auto sep = [](auto&& rest) { return rest.want_tok(TokenType::COMMA); };

    return ctx
        .want_tok(TokenType::IDENTIFIER, make_term)
        .then_want_tok(TokenType::LEFT_PAREN, [&](auto&& ident, auto&& rest) {
            auto args = rest
                .then_want_right_sep(
                    expect_expression,
                    sep,
                    make_func_args
                );
            return make_func(std::move(ident), std::move(args));
        })
        .then_expect_tok(TokenType::RIGHT_PAREN, AstErrorType::COMMA_OR_RPAREN);
}

NodeResult AbstractSyntaxTree::parse_declaration(NodeResult ctx) {
    return ctx
        .want_tok(TokenType::IDENTIFIER, make_term)
        .then_want_tok(TokenType::IDENTIFIER, make_declaration)
        .then_expect_tok(TokenType::SEMICOLON);
}

NodeResult AbstractSyntaxTree::parse_assigns(NodeResult ctx) {
    return ctx
        .want_tok(TokenType::IDENTIFIER, make_term)
        .then_want_tok(TokenType::ASSIGN)
        .then_parse_rest_with([](auto&&, auto&& rest) {
            return expect_expression(std::move(rest))
                .then_parse_rest_with(make_assign);
        })
        .then_expect_tok(TokenType::SEMICOLON);
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
        },
        [&](const StatementBlock& v) {
            o << "statement block\n";
            print_tree(o, v.statements, indent + 2);
        },
        [&](const Statements& v) {
            o << "statement\n";
            print_tree(o, v.value, indent + 2);
            if (v.next) print_tree(o, v.next, indent);
        }
    };

    std::visit(visitor, tree->expression);
}