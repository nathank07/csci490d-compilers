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
        Assign
    > expression;
};

inline NodeResult make_negated(NodeResult inner) {
    return inner.create_expr(std::make_unique<Expression>(Negated{std::move(*inner)}));
}

inline std::unique_ptr<Expression> make_term(Term term) {
    return std::make_unique<Expression>(std::move(term));
}

inline std::unique_ptr<Expression> make_term(const Token& t) {
    switch (t.type) {
        case TokenType::STRING:      return make_term(Term{TermValue{std::get<TokenString>(t.data).value}});
        case TokenType::IDENTIFIER:  return make_term(Term{TermValue{std::get<TokenIdentifier>(t.data).value}});
        case TokenType::REAL_NUMBER: return make_term(Term{TermValue{std::get<TokenReal>(t.data).value}});
        case TokenType::INTEGER:     return make_term(Term{TermValue{std::get<TokenInteger>(t.data).value}});
        default: __builtin_unreachable();
    }
}

inline NodeResult make_term(NodeResult cont) {
    return cont.create_expr(make_term(cont.consumed.back()));
}

inline NodeResult make_binary(const Token& t, NodeResult left, NodeResult right) {

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

inline NodeResult make_func_args(NodeResult left, NodeResult right) {
    auto next = std::move(*right);
    if (!std::holds_alternative<FunctionCallArgList>(next->expression)) {
        next = std::make_unique<Expression>(
            FunctionCallArgList{std::move(next), nullptr});
    }
    return right.create_expr(std::make_unique<Expression>(
        FunctionCallArgList{std::move(*left), std::move(next)}));
}

inline NodeResult make_func(std::unique_ptr<Expression> ident, NodeResult args) {
    auto val = std::move(*args);
    std::unique_ptr<Expression> arg_list;
    if (val && std::holds_alternative<FunctionCallArgList>(val->expression)) {
        arg_list = std::move(val);
    } else if (val) {
        arg_list = std::make_unique<Expression>(
            FunctionCallArgList{std::move(val), nullptr});
    }
    return args.create_expr(std::make_unique<Expression>(
        FunctionCall{std::move(ident), std::move(arg_list)}));
}

inline NodeResult make_decl(std::unique_ptr<Expression> type, NodeResult name) {
    return name.create_expr(std::make_unique<Expression>(
        Declaration{std::move(type), make_term(name.consumed.back())}
    ));
}

inline NodeResult make_assign(std::unique_ptr<Expression> name, NodeResult value) {
    return value.create_expr(std::make_unique<Expression>(
        Assign{std::move(name), make_term(value.consumed.back())}
    ));
}