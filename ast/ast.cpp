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
    std::vector<NodeResult> errors;

    auto result = parse_global(NodeResult::init(token_span), errors);
    errors.push_back(std::move(result));

    return errors;
}


NodeResult AbstractSyntaxTree::parse_global(NodeResult ctx, std::vector<NodeResult>& errors) {
    return NodeResult::init(ctx.rest)
        .collect_until_tok_recovering(
            expect_statement,
            TokenType::END_OF_FILE,
            make_statements,
            errors
        )
        .then_parse(make_statement_block);
}

NodeResult AbstractSyntaxTree::parse_statement_block(NodeResult ctx) {
    return parse_statement(std::move(ctx))
        .or_try_parse([](auto&& c) {
            return NodeResult::init(c.rest)
                .want_tok(TokenType::LEFT_BRACE)
                .then_collect_until_tok(
                    expect_statement,
                    TokenType::RIGHT_BRACE,
                    make_statements
                )
                .then_parse(make_statement_block);
        });
}

NodeResult AbstractSyntaxTree::expect_statement_block(NodeResult ctx) {
    return parse_statement_block(std::move(ctx))
        .nothing_guard(AstErrorType::EXPECTED_STATEMENT_BLOCK);
}

auto constexpr AbstractSyntaxTree::parse_parened(auto f) {
    return [f](auto&& ctx) {
        return NodeResult::init(ctx.rest)
            .want_tok(TokenType::LEFT_PAREN)
            .then_parse(expect_expressioned(f))
            .then_expect_tok(TokenType::RIGHT_PAREN);
    };
}

auto constexpr AbstractSyntaxTree::parse_semicoloned(auto f) {
    return [f](auto&& ctx) {
        return f(std::move(ctx))
            .then_expect_tok(TokenType::SEMICOLON);
    };
}

auto constexpr AbstractSyntaxTree::expect_expressioned(auto expr_parser) {
    return [expr_parser](auto&& ctx) {
        return expr_parser(std::move(ctx))
            .nothing_guard(AstErrorType::EXPECTED_STATEMENT);
    };
}

NodeResult AbstractSyntaxTree::parse_if_statement(NodeResult ctx) {
    return NodeResult::init(ctx.rest)
        .then_want_ident("if")
        .then_expect_tok(TokenType::LEFT_PAREN)
        .then_parse(expect_expressioned(parse_logical_expression))
        .then_expect_tok(TokenType::RIGHT_PAREN)
        .then_parse_with(expect_statement_block, make_if_block)
        // Normally, the idiomatic usage is then_parse_with; but it's too
        // strict on failure, so we use then_parse and pass the parser to 
        // make_else to support optional else blocks
        .then_parse([](auto&& if_statement) {
            return make_else_block(std::move(if_statement), 
                NodeResult::init(if_statement.rest)
                    .want_ident("else")
                    .then_parse(expect_statement_block)
                );
        });
}

NodeResult AbstractSyntaxTree::parse_statement(NodeResult ctx) {
    return NodeResult::init(ctx.rest)
        .then_parse(parse_assigns)
        .or_try_parse(parse_if_statement)
        .or_try_parse(parse_declaration)
        .or_try_parse(parse_semicoloned(parse_expression));
}

NodeResult AbstractSyntaxTree::expect_statement(NodeResult ctx) {
    return parse_statement(std::move(ctx))
        .nothing_guard(AstErrorType::EXPECTED_STATEMENT);
}

NodeResult AbstractSyntaxTree::parse_expression(NodeResult ctx) {
    return parse_logical_expression(std::move(ctx))
        .or_try_parse(parse_arithmetic);
}

NodeResult AbstractSyntaxTree::parse_logical_expression(NodeResult ctx) {
    return parse_or(std::move(ctx));
}

NodeResult AbstractSyntaxTree::parse_arithmetic(NodeResult ctx) {
    return parse_as(std::move(ctx));
}

NodeResult AbstractSyntaxTree::parse_not(NodeResult ctx) {    

    auto parse_logical_expr = [](auto&& expr) {
        return parse_bool_const(std::move(expr))
            .or_try_parse(parse_numeric_comparison)
            .or_try_parse(parse_parened(parse_logical_expression));
    };

    return NodeResult::init(ctx.rest)
        .want_tok(TokenType::NOT, parse_logical_expr)
        .then_parse(make_not)
        .or_try_parse(parse_logical_expr);
}

NodeResult AbstractSyntaxTree::parse_and(NodeResult ctx) {    
    return NodeResult::init(ctx.rest)
        .want_left_sep(
            parse_not,
            [](auto&& rest) {
                return rest
                    .want_tok(TokenType::AND);
            },
            make_binary
        );
}

NodeResult AbstractSyntaxTree::parse_or(NodeResult ctx) {    
    return NodeResult::init(ctx.rest)
        .want_left_sep(
            parse_and,
            [](auto&& rest) {
                return rest
                    .want_tok(TokenType::OR);
            },
            make_binary
        );
}

NodeResult AbstractSyntaxTree::parse_unary(NodeResult ctx) {

    auto parse_expr = [](auto&& rest) {
        return parse_function_call(std::move(rest))
            .or_try_parse(parse_parened(parse_arithmetic))
            .or_try_parse(parse_term);
    };

    auto parse_no_nest = [parse_expr](auto&& rest) {
        return parse_expr(std::move(rest))
            .nothing_guard(AstErrorType::NO_NESTED_UNARIES);
    };
    
    return NodeResult::init(ctx.rest)
        .want_tok(TokenType::SUB)
        .then_parse(parse_no_nest)
        .then_parse(make_negated)
        .or_try_parse([parse_no_nest, parse_expr](auto&& c) {
            return NodeResult::init(c.rest)
                .want_tok(TokenType::ADD)
                .then_parse(parse_no_nest);
        })
        .or_try_parse(parse_expr);
}

NodeResult AbstractSyntaxTree::parse_as(NodeResult ctx) {
    return NodeResult::init(ctx.rest)
        .want_left_sep(
            parse_md,
            [](auto&& rest) {
                return rest
                    .want_tok(TokenType::ADD)
                    .or_want_tok(TokenType::SUB);
            },
            make_binary
        );
}

NodeResult AbstractSyntaxTree::parse_md(NodeResult ctx) {
    return NodeResult::init(ctx.rest)
        .want_left_sep(
            parse_exp,
            [](auto&& rest) {
                return rest
                    .want_tok(TokenType::MULTIPLY)
                    .or_want_tok(TokenType::DIVIDE)
                    .or_want_ident("mod");
            },
            make_binary
        );
}

NodeResult AbstractSyntaxTree::parse_exp(NodeResult ctx) {
    return NodeResult::init(ctx.rest)
        .want_right_sep(
            parse_unary,
            [](auto&& rest) { 
                return rest
                    .want_tok(TokenType::EXPONENT); 
            },
            make_binary
        );
}

NodeResult AbstractSyntaxTree::parse_bool_const(NodeResult ctx) {
    return NodeResult::init(ctx.rest)
        .want_ident("true", make_bool_const)
        .or_want_ident("false", make_bool_const);
}

NodeResult AbstractSyntaxTree::parse_numeric_comparison(NodeResult ctx) {

    auto comparison_op = [](auto&& rest) {
        return NodeResult::init(rest.rest)
            .want_tok(TokenType::EQUALS)
            .or_want_tok(TokenType::NOT_EQUALS)
            .or_want_tok(TokenType::LESS_THAN)
            .or_want_tok(TokenType::LESS_THAN_EQ)
            .or_want_tok(TokenType::GREATER_THAN)
            .or_want_tok(TokenType::GREATER_THAN_EQ);
    };

    return NodeResult::init(ctx.rest)
        .then_parse(parse_arithmetic)
        .then_parse_with([&](auto&& rest) {
            return comparison_op(std::move(rest))
                .then_parse(expect_expressioned(parse_arithmetic));
        }, make_binary);
}

NodeResult AbstractSyntaxTree::parse_term(NodeResult ctx) {
    return NodeResult::init(ctx.rest)
        .want_tok(TokenType::IDENTIFIER, make_term)
        .or_want_tok(TokenType::INTEGER, make_term)
        .or_want_tok(TokenType::STRING, make_term)
        .or_want_tok(TokenType::REAL_NUMBER, make_term);
}

NodeResult AbstractSyntaxTree::parse_function_call(NodeResult ctx) {

    auto sep = [](auto&& rest) { return rest.want_tok(TokenType::COMMA); };

    return NodeResult::init(ctx.rest)
        .want_tok(TokenType::IDENTIFIER, make_term)
        .then_want_tok(TokenType::LEFT_PAREN, [&](auto&& ident, auto&& rest) {
            auto args = rest
                .then_want_right_sep(
                    expect_expressioned(parse_expression),
                    sep,
                    make_func_args
                );
            return make_func(std::move(ident), std::move(args));
        })
        .then_expect_tok(TokenType::RIGHT_PAREN, AstErrorType::COMMA_OR_RPAREN);
}

NodeResult AbstractSyntaxTree::parse_declaration(NodeResult ctx) {
    return NodeResult::init(ctx.rest)
        .want_tok(TokenType::IDENTIFIER, make_term)
        .then_want_tok(TokenType::IDENTIFIER, make_declaration)
        .then_expect_tok(TokenType::SEMICOLON);
}

NodeResult AbstractSyntaxTree::parse_assigns(NodeResult ctx) {
    return NodeResult::init(ctx.rest)
        .want_tok(TokenType::IDENTIFIER, make_term)
        .then_want_tok(TokenType::ASSIGN)
        .then_parse_with(expect_expressioned(parse_expression), make_assign)
        .then_expect_tok(TokenType::SEMICOLON);
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
            print_tree(o, *e.expression, indent + 2);
        },
        [&](const Add& v) {
            o << "+ (add)\n";
            print_tree(o, *v.left,  indent + 2);
            print_tree(o, *v.right, indent + 2);
        },
        [&](const Sub& v) {
            o << "- (sub)\n";
            print_tree(o, *v.left, indent + 2);
            print_tree(o, *v.right, indent + 2);
        },
        [&](const Mult& v) {
            o << "* (mult)\n";
            print_tree(o, *v.left, indent + 2);
            print_tree(o, *v.right, indent + 2);
        },
        [&](const Div& v) {
            o << "/ (div)\n";
            print_tree(o, *v.left, indent + 2);
            print_tree(o, *v.right, indent + 2);
        },
        [&](const Mod& v) {
            o << "mod \n";
            print_tree(o, *v.left, indent + 2);
            print_tree(o, *v.right, indent + 2);
        },
        [&](const Exp& v) {
            o << "^ (pow)\n";
            print_tree(o, *v.base, indent + 2);
            print_tree(o, *v.exponent, indent + 2);
        },
        [&](const FunctionCall& v) {
            o << "call\n";
            print_tree(o, *v.ident, indent + 2);
            if (v.args) print_tree(o, *v.args, indent + 2);
        },
        [&](const FunctionCallArgList& v) {
            o << "arg\n";
            print_tree(o, *v.value, indent + 2);
            if (v.next) print_tree(o, *v.next, indent);
        },
        [&](const Declaration& v) {
            o << "decl\n";
            print_tree(o, *v.type_ident, indent + 2);
            print_tree(o, *v.declared_ident, indent + 2);
        },
        [&](const Assign& v) {
            o << "assigns\n";
            print_tree(o, *v.ident, indent + 2);
            print_tree(o, *v.value, indent + 2);
        },
        [&](const StatementBlock& v) {
            o << "statement block\n";
            print_tree(o, *v.statements, indent + 2);
        },
        [&](const Statements& v) {
            o << "statement\n";
            print_tree(o, *v.value, indent + 2);
            if (v.next) print_tree(o, *v.next, indent);
        },
        [&](const BoolConst& v) {
            o << "const bool\n";
            o << std::string(indent + 2, ' ') << (v.is_true ? "true\n" : "false\n");
        },
        [&](const NumericComparison& v) {
            o << Token::get_token_literal(v.token_type) << "\n";
            print_tree(o, *v.left, indent + 2);
            print_tree(o, *v.right, indent + 2);
        },
        [&](const Not& v) {
            o << "not\n";
            print_tree(o, *v.expression, indent + 2);
        },
        [&](const And& v) {
            o << "and\n";
            print_tree(o, *v.left, indent + 2);
            print_tree(o, *v.right, indent + 2);
        },
        [&](const Or& v) {
            o << "or\n";
            print_tree(o, *v.left, indent + 2);
            print_tree(o, *v.right, indent + 2);
        },
        [&](const If& v) {
            o << "if\n";
            print_tree(o, *v.logical_expression, indent + 2);
            print_tree(o, *v.if_statement_block, indent + 2);

            if (v.else_statement_block) {
                o << std::string(indent, ' ') << "else\n";
                print_tree(o, *v.else_statement_block, indent + 2);
            }
        }
    };

    std::visit(visitor, tree->expression);
}