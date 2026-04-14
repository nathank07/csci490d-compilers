#pragma once
#include <cassert>
#include <memory>
#include <variant>
#include <span>
#include "../lexer/token.hpp"
#include "parseresult.hpp"
#include "asterror.hpp"

struct Expression;

using NodeResult = ParseResult<std::unique_ptr<Expression>, AstError>;

using TermValue = 
    std::variant<
        std::string,
        std::u8string,
        long long,
        double
    >;

struct Negated {
    NodeResult expression;
};

struct Add {
    NodeResult left;
    NodeResult right;
};

struct Sub {
    NodeResult left;
    NodeResult right;
};

struct Mult {
    NodeResult left;
    NodeResult right;
};

struct Div {
    NodeResult left;
    NodeResult right;
};

struct Mod {
    NodeResult left;
    NodeResult right;
};

struct Exp {
    NodeResult base;
    NodeResult exponent;
};

struct Term {
    TermValue v;
    Token tok;
};

struct FunctionCall {
    NodeResult ident;
    NodeResult args;

    uint8_t arg_count();
};

struct FunctionCallArgList {
    NodeResult value;
    NodeResult next;

    uint8_t size();
};

struct Declaration {
    NodeResult type_ident;
    NodeResult declared_ident;
};

struct Assign {
    NodeResult ident;
    NodeResult value;
};

struct StatementBlock {
    NodeResult statements;
};

struct Statements {
    NodeResult value;
    NodeResult next;
};

struct BoolConst {
    NodeResult node;
    bool is_true;
};

struct NumericComparison {
    NodeResult left;
    NodeResult right;
    TokenType token_type;
};

struct Not {
    NodeResult expression;
};

struct And {
    NodeResult left;
    NodeResult right;
};

struct Or {
    NodeResult left;
    NodeResult right;
};

struct If {
    NodeResult logical_expression;
    NodeResult if_statement_block;
    NodeResult else_statement_block;
};

struct Expression {
    std::variant<
        std::monostate, 
        Negated, 
        Add, 
        Sub, 
        Mult, 
        Div, 
        Mod, 
        Exp, 
        Term,
        FunctionCall,
        FunctionCallArgList,
        Declaration,
        Assign,
        StatementBlock,
        Statements,
        BoolConst,
        NumericComparison,
        Not,
        And,
        Or,
        If
    > expression;
};

inline uint8_t FunctionCallArgList::size() {
    if (next) { 
        return 1 + std::get<FunctionCallArgList>((*next)->expression).size(); 
    }

    return 1;
}

inline uint8_t FunctionCall::arg_count() {
    if (args) {
        return std::get<FunctionCallArgList>((*args)->expression).size();
    }
    return 0;
}

inline std::unique_ptr<Expression> take_or_null(NodeResult&& r) {
    if (r.is_just()) return std::move(*r);
    return nullptr;
}

inline NodeResult make_negated(NodeResult inner) {
    if (inner.is_error()) return inner;
    assert(inner.is_just());
    return NodeResult(NodeResult::Just{std::make_unique<Expression>
        (Negated{std::move(inner)})}, inner.consumed, inner.rest);
}

inline NodeResult make_not(NodeResult inner) {
    if (inner.is_error()) return inner;
    assert(inner.is_just());
    return NodeResult(NodeResult::Just{std::make_unique<Expression>
        (Not{std::move(inner)})}, inner.consumed, inner.rest);
}

inline std::unique_ptr<Expression> make_term_expr(const Token& t) {
    switch (t.type) {
        case TokenType::STRING:      return std::make_unique<Expression>(Term{TermValue{std::get<TokenString>(t.data).value}, t});
        case TokenType::IDENTIFIER:  return std::make_unique<Expression>(Term{TermValue{std::get<TokenIdentifier>(t.data).value}, t});
        case TokenType::REAL_NUMBER: return std::make_unique<Expression>(Term{TermValue{std::get<TokenReal>(t.data).value}, t});
        case TokenType::INTEGER:     return std::make_unique<Expression>(Term{TermValue{std::get<TokenInteger>(t.data).value}, t});
        default: __builtin_unreachable();
    }
}

inline NodeResult make_term(NodeResult cont) {
    if (cont.is_error()) return cont;
    return cont.create_expr(make_term_expr(cont.consumed.back()));
}

inline NodeResult make_binary(NodeResult left, NodeResult right) {
    if (left.is_error()) return left;
    if (right.is_error()) return right;

    assert(left.is_just()); assert(right.is_just());
    
    auto consumed = NodeResult::merge_consumed(left.consumed, right.consumed);

    auto make = [&](auto expr) {
        return NodeResult(NodeResult::Just{std::make_unique<Expression>(std::move(expr))}, consumed, right.rest);
    };

    Token t = right.consumed.front();

    auto make_numeric = [&]() {
        return make(NumericComparison{std::move(left), std::move(right), t.type});
    };

    switch (t.type) {
        case TokenType::ADD:      return make(Add{std::move(left), std::move(right)});
        case TokenType::SUB:      return make(Sub{std::move(left), std::move(right)});
        case TokenType::MULTIPLY: return make(Mult{std::move(left), std::move(right)});
        case TokenType::DIVIDE:   return make(Div{std::move(left), std::move(right)});
        case TokenType::EXPONENT: return make(Exp{std::move(left), std::move(right)});
        case TokenType::LESS_THAN:       return make_numeric();
        case TokenType::LESS_THAN_EQ:    return make_numeric();
        case TokenType::GREATER_THAN:    return make_numeric();
        case TokenType::GREATER_THAN_EQ: return make_numeric();
        case TokenType::EQUALS:          return make_numeric();
        case TokenType::NOT_EQUALS:      return make_numeric();
        case TokenType::OR:       return make(Or{std::move(left), std::move(right)});
        case TokenType::AND:      return make(And{std::move(left), std::move(right)});
        case TokenType::IDENTIFIER: {
            if (std::get<TokenIdentifier>(t.data).value == "mod")
                return make(Mod{std::move(left), std::move(right)});
            return NodeResult::nothing(right.rest);
        }
        default: return NodeResult::nothing(right.rest);
    }
}

template <typename T>
inline NodeResult ensure_ll_node(NodeResult r) {
    if (!r.is_just()) return r;
    if (!std::holds_alternative<T>((*r)->expression)) {
        return NodeResult(NodeResult::Just{std::make_unique<Expression>(
            T{std::move(r), NodeResult::nothing()})}, r.consumed, r.rest);
    }
    return r;
}

template <typename T>
inline NodeResult create_foldr_ll(NodeResult l, NodeResult r) {
    if (l.is_error()) return l;
    if (r.is_error()) return r;
    // allows skipping over errors
    if (l.is_nothing() || l.is_continue()) return r;
    if (r.is_nothing() || r.is_continue()) return l;
    assert(l.is_just());
    auto ensured = ensure_ll_node<T>(std::move(r));
    auto consumed = NodeResult::merge_consumed(l.consumed, ensured.consumed);
    return NodeResult(NodeResult::Just{std::make_unique<Expression>(
        T{std::move(l), std::move(ensured)})}, consumed, ensured.rest);
}

inline NodeResult make_func(NodeResult ident, NodeResult args) {
    if (ident.is_error()) return ident;
    if (args.is_error()) return args;
    assert(ident.is_just());
    auto wrapped = ensure_ll_node<FunctionCallArgList>(std::move(args));
    auto consumed = NodeResult::merge_consumed(ident.consumed, wrapped.consumed);
    return NodeResult(NodeResult::Just{std::make_unique<Expression>(
        FunctionCall{std::move(ident), std::move(wrapped)})}, consumed, wrapped.rest);
}

inline NodeResult make_func_args(NodeResult left, NodeResult right) {
    return create_foldr_ll<FunctionCallArgList>(std::move(left), std::move(right));
}

inline NodeResult make_declaration(NodeResult type, NodeResult name) {
    if (type.is_error()) return type;
    if (name.is_error()) return name;
    assert(type.is_just());
    auto term = make_term(std::move(name));
    auto consumed = NodeResult::merge_consumed(type.consumed, term.consumed);
    return NodeResult(NodeResult::Just{std::make_unique<Expression>(
        Declaration{std::move(type), std::move(term)})}, consumed, term.rest);
}

inline NodeResult make_assign(NodeResult name, NodeResult value) {
    if (name.is_error()) return name;
    if (value.is_error()) return value;
    assert(name.is_just()); assert(value.is_just());
    auto consumed = NodeResult::merge_consumed(name.consumed, value.consumed);
    return NodeResult(NodeResult::Just{std::make_unique<Expression>(
        Assign{std::move(name), std::move(value)})}, consumed, value.rest);
}

inline NodeResult make_statement_block(NodeResult statements) {
    if (statements.is_error()) return statements;
    auto expr = ensure_ll_node<Statements>(std::move(statements));
    assert(expr.is_just());
    return NodeResult(NodeResult::Just{std::make_unique<Expression>(
        StatementBlock{std::move(expr)})}, expr.consumed, expr.rest);
}

inline NodeResult make_statements(NodeResult left, NodeResult right) {
    return create_foldr_ll<Statements>(std::move(left), std::move(right));
}

inline NodeResult make_if_block(NodeResult logical_expression, NodeResult statement_block) {
    if (!logical_expression.is_just()) return logical_expression;
    if (!statement_block.is_just()) return statement_block;
    auto consumed = NodeResult::merge_consumed(logical_expression.consumed, statement_block.consumed);
    return NodeResult(NodeResult::Just{std::make_unique<Expression>(
        If{std::move(logical_expression), std::move(statement_block), NodeResult::nothing()})}, 
            consumed, statement_block.rest);
}

inline NodeResult make_else_block(NodeResult if_statement_block, NodeResult else_block) {
    if (else_block.is_error()) return else_block;
    if (else_block.is_nothing()) return if_statement_block;
    auto consumed = NodeResult::merge_consumed(if_statement_block.consumed, else_block.consumed);
    auto& block = std::get<If>((*if_statement_block)->expression);
    return NodeResult(NodeResult::Just{std::make_unique<Expression>(
        If{std::move(block.logical_expression), std::move(block.if_statement_block), 
            std::move(else_block)})}, consumed, else_block.rest);
}

inline NodeResult make_bool_const(NodeResult node) {
    if (node.is_error()) return node;
    auto node_str = node.consumed.back().get_token_literal();
    return NodeResult(NodeResult::Just{std::make_unique<Expression>(
        BoolConst{ make_term(std::move(node)), node_str == "true"})}, 
            node.consumed, node.rest);
};