#pragma once

#include "../lexer/lexer.hpp"
#include "expression.hpp"
#include <memory>
#include <span>
#include <vector>

class AbstractSyntaxTree {

    NodeResult parse_expression(std::span<const Token> tokens);
    NodeResult parse_function(std::span<const Token> tokens);
    NodeResult parse_declaration(std::span<const Token> tokens);
    NodeResult expect_token(std::span<const Token> tokens, const TokenType expect_tok);
    NodeResult expect_ident(std::span<const Token> tokens);
    NodeResult expect_ident(std::span<const Token> tokens, std::string expect_str);
    NodeResult parse_unary(std::span<const Token> tokens);
    NodeResult parse_paren(std::span<const Token> tokens);
    NodeResult parse_md(std::span<const Token> tokens);
    NodeResult parse_as(std::span<const Token> tokens);
    NodeResult parse_exp(std::span<const Token> tokens);
    NodeResult parse_binary_base(std::span<const Token> tokens, auto is_op_predicate, auto get_expr);
    NodeResult parse_binary_rest(NodeResult base, std::span<const Token> tokens, auto is_op_predicate, auto get_expr);
    NodeResult parse_term(std::span<const Token> tokens);

public:

    static std::vector<NodeResult> create(const Lexer& lexer_result);
    static std::vector<std::unique_ptr<Expression>> unwrap_valid_nodes(std::vector<NodeResult>& node_v);
    static void print_tree(std::ostream& o, const std::unique_ptr<Expression>& tree, std::size_t indent = 0);
};