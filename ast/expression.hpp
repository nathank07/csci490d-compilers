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
    std::unique_ptr<Expression> next_arg;
};

struct Declaration {
    std::unique_ptr<Expression> type_ident;
    std::unique_ptr<Expression> declared_ident;
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
        Declaration
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