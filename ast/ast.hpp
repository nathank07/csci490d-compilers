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

    MaybeNode parse_expression(std::span<const Token> tokens);
    MaybeNode parse_unary(std::span<const Token> tokens);
    MaybeNode parse_paren(std::span<const Token> tokens);
    MaybeNode parse_md(std::span<const Token> tokens);
    MaybeNode parse_as(std::span<const Token> tokens);
    MaybeNode parse_term(std::span<const Token> tokens);

public:
    MaybeNode create(const Lexer& lexerResult);
    void print_tree(std::ostream& o, const std::unique_ptr<Expression>& tree) {
        const auto visitor = overloads 
        {
            [&](std::monostate) { o << "empty"; },
            [&](const Term& t) { 
                auto v = overloads {
                    [&](std::string s) {
                        o << s << "\n";
                    },
                    [&](std::u8string s) {
                        o << std::string(s.begin(), s.end()) << "\n";
                    },
                    [&](long long l) {
                        o << l << "\n";
                    },
                    [&](double d) {
                        o << d << "\n";
                    }
                };
                
                o << "term: ";
                std::visit(v, t.v); 
            },
            [&](const Negated& e) { o << "negated "; print_tree(o, e.expression); },
            [&](const Add& v) { o<< "hi"; },
            [&](const Sub& v) { o<< "hi"; },
            [&](const Mult& v) { o<< "hi"; },
            [&](const Div& v) { o<< "hi"; },
            [&](const Exp& v) { o<< "hi"; }
        };


        std::visit(visitor, *tree);
    }
};