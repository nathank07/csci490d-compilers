#pragma once

#include "../lexer/lexer.hpp"
#include "expression.hpp"
#include <memory>
#include <vector>

class AbstractSyntaxTree {

    
    static NodeResult parse_global(NodeResult ctx, std::vector<NodeResult>& errors);
    
    static NodeResult parse_statement(NodeResult ctx);
    static NodeResult parse_function_call(NodeResult ctx);
    static NodeResult parse_declaration(NodeResult ctx);
    static NodeResult parse_assigns(NodeResult ctx);

    static NodeResult parse_expression(NodeResult ctx);
    static NodeResult parse_unary(NodeResult ctx);
    static NodeResult parse_paren(NodeResult ctx);
    static NodeResult parse_as(NodeResult ctx);
    static NodeResult parse_md(NodeResult ctx);
    static NodeResult parse_exp(NodeResult ctx);
    static NodeResult parse_term(NodeResult ctx);
    static NodeResult parse_bool_const(NodeResult ctx);

    static NodeResult expect_statement(NodeResult ctx);
    static NodeResult expect_expression(NodeResult ctx);

    static constexpr auto parse_semicoloned(auto&& f);

    static constexpr auto parse_binary = [](auto&& lhs, auto&& rhs) {
        auto& op_tok = rhs.consumed.front();
        return make_binary(op_tok, std::move(lhs), std::move(rhs));
    };

public:

    static std::vector<NodeResult> create(const Lexer& lexer_result);
    static void print_tree(std::ostream& o, const std::unique_ptr<Expression>& tree, std::size_t indent = 0);
};