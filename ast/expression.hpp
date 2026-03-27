#pragma once
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
    std::unique_ptr<Expression> expression;
};

struct Add {
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

struct Sub {
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

struct Mult {
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

struct Div {
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

struct Mod {
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

struct Exp {
    std::unique_ptr<Expression> base;
    std::unique_ptr<Expression> exponent;
};

struct Term {
    TermValue v;
};

struct FunctionCall {
    std::unique_ptr<Expression> ident;
    std::unique_ptr<Expression> args;
};

struct FunctionCallArgList {
    std::unique_ptr<Expression> value;
    std::unique_ptr<Expression> next;
};

struct Declaration {
    std::unique_ptr<Expression> type_ident;
    std::unique_ptr<Expression> declared_ident;
};

struct Assign {
    std::unique_ptr<Expression> ident;
    std::unique_ptr<Expression> value;
};

struct StatementBlock {
    std::unique_ptr<Expression> statements;
};

struct Statements {
    std::unique_ptr<Expression> value;
    std::unique_ptr<Expression> next;
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
        Statements
    > expression;
};

inline std::unique_ptr<Expression> take_or_null(NodeResult&& r) {
    if (r.is_just()) return std::move(*r);
    return nullptr;
}

inline NodeResult make_negated(NodeResult inner) {
    if (inner.is_error()) return inner;
    return inner.create_expr(std::make_unique<Expression>(Negated{std::move(*inner)}));
}

inline std::unique_ptr<Expression> make_term_expr(const Token& t) {
    switch (t.type) {
        case TokenType::STRING:      return std::make_unique<Expression>(Term{TermValue{std::get<TokenString>(t.data).value}});
        case TokenType::IDENTIFIER:  return std::make_unique<Expression>(Term{TermValue{std::get<TokenIdentifier>(t.data).value}});
        case TokenType::REAL_NUMBER: return std::make_unique<Expression>(Term{TermValue{std::get<TokenReal>(t.data).value}});
        case TokenType::INTEGER:     return std::make_unique<Expression>(Term{TermValue{std::get<TokenInteger>(t.data).value}});
        default: __builtin_unreachable();
    }
}

inline NodeResult make_term(NodeResult cont) {
    if (cont.is_error()) return cont;
    return cont.create_expr(make_term_expr(cont.consumed.back()));
}

inline NodeResult make_binary(const Token& t, NodeResult left, NodeResult right) {
    if (left.is_error()) return left;
    if (right.is_error()) return right;

    auto make = [&](auto expr) { 
        return right.create_expr(std::make_unique<Expression>(std::move(expr))); };

    switch (t.type) {
        case TokenType::ADD:      return make(Add{std::move(*left), std::move(*right)});
        case TokenType::SUB:      return make(Sub{std::move(*left), std::move(*right)});
        case TokenType::MULTIPLY: return make(Mult{std::move(*left), std::move(*right)});
        case TokenType::DIVIDE:   return make(Div{std::move(*left), std::move(*right)});
        case TokenType::EXPONENT: return make(Exp{std::move(*left), std::move(*right)});
        case TokenType::IDENTIFIER: {

            if (std::get<TokenIdentifier>(t.data).value == "mod")
                return make(Mod{std::move(*left), std::move(*right)});

            return NodeResult::nothing(right.rest);
        }

        default: return NodeResult::nothing(right.rest);
    }

}

template <typename T>
inline std::unique_ptr<Expression> ensure_ll_node(std::unique_ptr<Expression> expr) {
    if (!std::holds_alternative<T>(expr->expression))
        return std::make_unique<Expression>(T{std::move(expr), nullptr});
    return expr;
}

template <typename T>
inline NodeResult create_foldr_ll(NodeResult l, NodeResult r) {
    if (l.is_error()) return l;
    if (r.is_error()) return r;
    auto r_expr = ensure_ll_node<T>(std::move(*r));
    return r.create_expr(std::make_unique<Expression>(
            T{std::move(*l), std::move(r_expr)}));
}

inline NodeResult make_func(NodeResult ident, NodeResult args) {
    if (ident.is_error()) return ident;
    if (args.is_error()) return args;
    auto args_expr = ensure_ll_node<FunctionCallArgList>(std::move(*args));
    return args.create_expr(std::make_unique<Expression>(
        FunctionCall{std::move(*ident), std::move(args_expr)}));
}

inline NodeResult make_func_args(NodeResult left, NodeResult right) {
    return create_foldr_ll<FunctionCallArgList>(std::move(left), std::move(right));
}

inline NodeResult make_declaration(NodeResult type, NodeResult name) {
    if (type.is_error()) return type;
    if (name.is_error()) return name;
    return name.create_expr(std::make_unique<Expression>(
        Declaration{std::move(*type), *make_term(std::move(name))}
    ));
}

inline NodeResult make_assign(NodeResult name, NodeResult value) {
    if (name.is_error()) return name;
    if (value.is_error()) return value;
    return value.create_expr(std::make_unique<Expression>(
        Assign{std::move(*name), std::move(*value)}
    ));
}

inline NodeResult make_statement_block(NodeResult statements) {
    if (statements.is_error()) return statements;
    auto expr = ensure_ll_node<Statements>(std::move(*statements));
    return statements.create_expr(
        std::make_unique<Expression>(StatementBlock{std::move(expr)})
    );
}

inline NodeResult make_statements(NodeResult left, NodeResult right) {
    return create_foldr_ll<Statements>(std::move(left), std::move(right));
}