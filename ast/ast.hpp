#include "../lexer/lexer.hpp"
#include "expression.hpp"
#include "parseresult.hpp"
#include "asterror.hpp"
#include <memory>
#include <span>
#include <variant>
#include <functional>

using NodeResult = ParseResult<std::unique_ptr<Expression>, AstError>;

// helper type for the visitor
template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };

class AbstractSyntaxTree {

    NodeResult parse_expression(std::span<const Token> tokens);
    NodeResult parse_unary(std::span<const Token> tokens);
    NodeResult parse_paren(std::span<const Token> tokens);
    NodeResult parse_md(std::span<const Token> tokens);
    NodeResult parse_as(std::span<const Token> tokens);
    NodeResult parse_exp(std::span<const Token> tokens);
    NodeResult parse_binary(std::span<const Token> tokens, std::function<bool (const Token&)> is_op);
    NodeResult parse_term(std::span<const Token> tokens);

public:
    NodeResult create(const Lexer& lexerResult);
    void print_tree(std::ostream& o, const std::unique_ptr<Expression>& tree, std::size_t indent = 0) {
        if (!tree || tree->token_span.empty()) 
            return;

        o << std::string(indent, ' ');
        
        const auto& start_tok = tree->token_span.front();
        const auto& end_tok = tree->token_span.back();

        const auto visitor = overloads 
        {
            [&](std::monostate) { o << "empty"; },
            [&](const Term& t) { 
                auto v = overloads {
                    [&](std::string s) {
                        o << s;
                    },
                    [&](std::u8string s) {
                        o << std::string(s.begin(), s.end());
                    },
                    [&](long long l) {
                        o << l;
                    },
                    [&](double d) {
                        o << d;
                    }
                };
                
                o << "term: ";
                std::visit(v, t.v);
                o << "[" << start_tok.line_number << ":" << start_tok.column_number
                << "-" << end_tok.column_number << "] \n";  
            },
            [&](const Negated& e) {
                o << "neg ";
                o << "[" << start_tok.line_number << ":" << start_tok.column_number
                << "-" << end_tok.column_number << "] \n";
                print_tree(o, e.expression, indent + 2); 
            },
            [&](const Add& v) { 
                o << "+ ";
                o << "[" << start_tok.line_number << ":" << start_tok.column_number
                << "-" << end_tok.column_number << "] \n"; 
                print_tree(o, v.left,  indent + 2);
                print_tree(o, v.right, indent + 2);
            },
            [&](const Sub& v) { 
                o << "- ";
                o << "[" << start_tok.line_number << ":" << start_tok.column_number
                << "-" << end_tok.column_number << "] \n";
                print_tree(o, v.left, indent + 2);
                print_tree(o, v.right, indent + 2);
            },
            [&](const Mult& v) { 
                o << "* ";
                o << "[" << start_tok.line_number << ":" << start_tok.column_number
                << "-" << end_tok.column_number << "] \n";
                print_tree(o, v.left, indent + 2);
                print_tree(o, v.right, indent + 2);
            },
            [&](const Div& v) { 
                o << "/ ";
                o << "[" << start_tok.line_number << ":" << start_tok.column_number
                << "-" << end_tok.column_number << "] \n";
                print_tree(o, v.left, indent + 2);
                print_tree(o, v.right, indent + 2);
            },
            [&](const Mod& v) { 
                o << "mod ";
                o << "[" << start_tok.line_number << ":" << start_tok.column_number
                << "-" << end_tok.column_number << "] \n";
                print_tree(o, v.left, indent + 2);
                print_tree(o, v.right, indent + 2);
            },
            [&](const Exp& v) { 
                o << "^ ";
                o << "[" << start_tok.line_number << ":" << start_tok.column_number
                << "-" << end_tok.column_number << "] \n";
                print_tree(o, v.base, indent + 2);
                print_tree(o, v.exponent, indent + 2);
             }
        };

        std::visit(visitor, tree->expression);
    }
};