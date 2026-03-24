#pragma once

#include "../lexer/lexer.hpp"
#include "expression.hpp"
#include <memory>
#include <span>
#include <vector>

class AbstractSyntaxTree {

    static NodeResult parse_expression(std::span<const Token> tokens);
    static NodeResult parse_statement(NodeResult ctx);
    static NodeResult parse_expression(NodeResult ctx);
    static NodeResult parse_unary(NodeResult ctx);
    static NodeResult parse_paren(NodeResult ctx);
    static NodeResult parse_as(NodeResult ctx);
    static NodeResult parse_md(NodeResult ctx);
    static NodeResult parse_exp(NodeResult ctx);
    static NodeResult parse_term(NodeResult ctx);
    static NodeResult parse_function(NodeResult ctx);
    static NodeResult parse_declaration(NodeResult ctx);
    static NodeResult parse_assigns(NodeResult ctx);

public:

    static std::vector<NodeResult> create(const Lexer& lexer_result);
    static std::vector<std::unique_ptr<Expression>> unwrap_valid_nodes(std::vector<NodeResult>& node_v);
    static void print_tree(std::ostream& o, const std::unique_ptr<Expression>& tree, std::size_t indent = 0);
};