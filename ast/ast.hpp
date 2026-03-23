#pragma once

#include "../lexer/lexer.hpp"
#include "expression.hpp"
#include <memory>
#include <span>
#include <vector>

class AbstractSyntaxTree {

    NodeResult parse_expression(std::span<const Token> tokens);
    NodeResult parse_expression(NodeResult ctx);
    NodeResult parse_unary(NodeResult ctx);
    NodeResult parse_paren(NodeResult ctx);
    NodeResult parse_as(NodeResult ctx);
    NodeResult parse_md(NodeResult ctx);
    NodeResult parse_exp(NodeResult ctx);
    NodeResult parse_term(NodeResult ctx);

public:

    static std::vector<NodeResult> create(const Lexer& lexer_result);
    static std::vector<std::unique_ptr<Expression>> unwrap_valid_nodes(std::vector<NodeResult>& node_v);
    static void print_tree(std::ostream& o, const std::unique_ptr<Expression>& tree, std::size_t indent = 0);
};