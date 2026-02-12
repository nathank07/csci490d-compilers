#include "../lexer/lexer.hpp"
#include "expression.hpp"
#include <memory>
#include <span>
#include <variant>

using MaybeNode = std::optional<std::unique_ptr<Expression>>;

// helper type for the visitor
template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };

class AbstractSyntaxTree {

    MaybeNode parseExpression(std::span<const Token> tokens);
    MaybeNode parseUnary(std::span<const Token> tokens);
    MaybeNode parseTerm(std::span<const Token> tokens);

public:
    std::unique_ptr<Expression> create(const Lexer& lexerResult);
    void print_tree(std::ostream& o, std::unique_ptr<Expression> tree) {
        const auto visitor = overloads 
        {
            [&](std::monostate) { o << "empty"; },
            [&](const Term& t) { o << "term"; },
            [&](const Negated& e) { o << "negated"; },
            [&](const Add& v) { o<< "hi"; },
            [&](const Sub& v) { o<< "hi"; },
            [&](const Mult& v) { o<< "hi"; },
            [&](const Div& v) { o<< "hi"; },
            [&](const Exp& v) { o<< "hi"; }
        };


        std::visit(visitor, *tree);
    }
};