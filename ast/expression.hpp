#include <memory>
#include <variant>
#include <span>
#include "../lexer/token.hpp"

struct Expression;

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

template<typename T>
std::unique_ptr<Expression> make_binary(std::unique_ptr<Expression>& left, std::unique_ptr<Expression>& right) {
    auto span = span_covering(*left, *right);
    return std::make_unique<Expression>(T{std::move(left), std::move(right)}, span);
}

inline std::unique_ptr<Expression> make_negated(std::unique_ptr<Expression> inner, std::span<const Token> tokens) {
    return std::make_unique<Expression>(Negated{std::move(inner)}, tokens);
}

inline std::unique_ptr<Expression> make_term(Term term, std::span<const Token> tokens) {
    return std::make_unique<Expression>(std::move(term), tokens.subspan(0, 1));
}
