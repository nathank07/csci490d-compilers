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
        Term
    > expression;

    std::span<const Token> token_span;
};

inline std::span<const Token> span_covering(const Expression& left, const Expression& right) {
    auto start_ptr = left.token_span.data();
    auto end_ptr = right.token_span.data() + right.token_span.size();
    return {start_ptr, static_cast<std::size_t>(end_ptr - start_ptr)};
}

inline NodeResult make_binary(const Token& t, NodeResult left, NodeResult right) {    
    auto span = span_covering(**left, **right);

    switch (t.type) {
        case TokenType::ADD: return NodeResult::just(std::make_unique<Expression>(Add{std::move(*left), std::move(*right)}, span));
        case TokenType::SUB: return NodeResult::just(std::make_unique<Expression>(Sub{std::move(*left), std::move(*right)}, span));
        case TokenType::MULTIPLY: return NodeResult::just(std::make_unique<Expression>(Mult{std::move(*left), std::move(*right)}, span));
        case TokenType::DIVIDE: return NodeResult::just(std::make_unique<Expression>(Div{std::move(*left), std::move(*right)}, span));
        case TokenType::IDENTIFER: return NodeResult::just(std::make_unique<Expression>(Mod{std::move(*left), std::move(*right)}, span));
        case TokenType::EXPONENT: return NodeResult::just(std::make_unique<Expression>(Exp{std::move(*left), std::move(*right)}, span));
        default: return NodeResult::error(AstError::bad_symbol(t));
    }

}

inline NodeResult make_negated(std::unique_ptr<Expression> inner, std::span<const Token> tokens) {
    return NodeResult::just(std::make_unique<Expression>(Negated{std::move(inner)}, tokens));
}

inline NodeResult make_term(Term term, std::span<const Token> tokens) {
    return NodeResult::just(std::make_unique<Expression>(std::move(term), tokens));
}

inline NodeResult make_term(const Token& t, std::span<const Token> tokens) {
    switch (t.type) {
        case TokenType::STRING:      return make_term(Term{TermValue{std::get<TokenString>(t.data).value}}, tokens);
        case TokenType::IDENTIFER:   return make_term(Term{TermValue{std::get<TokenIdentifier>(t.data).value}}, tokens);
        case TokenType::REAL_NUMBER: return make_term(Term{TermValue{std::get<TokenReal>(t.data).value}}, tokens);
        case TokenType::INTEGER:     return make_term(Term{TermValue{std::get<TokenInteger>(t.data).value}}, tokens);
        default: return NodeResult::nothing();
    }
}


inline NodeResult make_term(std::unique_ptr<Expression> term, std::span<const Token> tokens) {
    return NodeResult::just(std::make_unique<Expression>(std::move(term->expression), tokens));
}
