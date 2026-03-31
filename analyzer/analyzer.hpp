#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "../ast/ast.hpp"
#include "../utils.hpp"

struct Int4;
struct Function;

enum class Type {
    Int4, Void
};

using TypedVar = 
    std::variant<
        Int4,
        Function
    >;

struct Int4 { uint8_t v; };

struct Function { 
    std::vector<Type> args;
    // Once args is exhausted fall back to this specific type
    std::optional<Type> variadic_arg;
    Type return_value;
};

struct Analyzer {

struct SymbolTable {
    std::unordered_map<std::string, TypedVar> table;
    std::vector<NodeResult>::reverse_iterator node_loc;
};

using ExprPtr = std::unique_ptr<Expression>;

private:

    static bool in_table(TermValue term, const SymbolTable& symbol_table) {
        return !std::holds_alternative<std::string>(term) ||
            symbol_table.table.contains(std::get<std::string>(term));
    }

    static void analyze(const NodeResult& just_value, SymbolTable& symbols, std::vector<NodeResult>& results) {
        const auto visitor = overloads
        {
            [&](const std::monostate&) { },
            [&](const Term& t) {
                if (!in_table(t.v, symbols)) {
                    results.push_back(NodeResult::error(AstErrorType::UNRECOGNIZED_IDENT, just_value.consumed, t.tok));
                }
            },
            [&](const Negated& e) {
                analyze(e.expression, symbols, results);
            },
            [&](const Add& v) {
                analyze(v.left, symbols, results);
                analyze(v.right, symbols, results);
            },
            [&](const Sub& v) {
                analyze(v.left, symbols, results);
                analyze(v.right, symbols, results);
            },
            [&](const Mult& v) {
                analyze(v.left, symbols, results);
                analyze(v.right, symbols, results);
            },
            [&](const Div& v) {
                analyze(v.left, symbols, results);
                analyze(v.right, symbols, results);
            },
            [&](const Mod& v) {
                analyze(v.left, symbols, results);
                analyze(v.right, symbols, results);
            },
            [&](const Exp& v) {
                analyze(v.base, symbols, results);
                analyze(v.exponent, symbols, results);
            },
            [&](const FunctionCall& v) {
                analyze(v.ident, symbols, results);
                if (v.args) analyze(v.args, symbols, results);
            },
            [&](const FunctionCallArgList& v) {
                if (v.value) analyze(v.value, symbols, results);
                if (v.next) analyze(v.next, symbols, results);
            },
            [&](const Declaration& v) {
                if (!v.type_ident.is_just() || !v.declared_ident.is_just()) return;
                const auto& type_term = std::get<Term>((*v.type_ident)->expression);
                const auto& name_term = std::get<Term>((*v.declared_ident)->expression);
                if (!std::holds_alternative<std::string>(type_term.v) ||
                    !std::holds_alternative<std::string>(name_term.v)) return;
                const auto& type_name = std::get<std::string>(type_term.v);
                const auto& name = std::get<std::string>(name_term.v);

                if (in_table(name, symbols)) {
                    results.push_back(NodeResult::error(AstErrorType::VAR_DEFINED, just_value.consumed));
                } else if (type_name == "int4") {
                    symbols.table[name] = Int4{};
                } else {
                    results.push_back(NodeResult::error(AstErrorType::UNRECOGNIZED_TYPE, just_value.consumed));
                }
            },
            [&](const Assign& v) {
                analyze(v.ident, symbols, results);
                analyze(v.value, symbols, results);
            },
            [&](const StatementBlock& v) {
                analyze(v.statements, symbols, results);
            },
            [&](const Statements& v) {
                analyze(v.value, symbols, results);
                if (v.next) analyze(v.next, symbols, results);
            }
        };

        std::visit(visitor, (*just_value)->expression);
    }

public:

    static SymbolTable analyze(std::vector<NodeResult>& ast_results) {
        SymbolTable symbol_table = {{
            // TODO: Give print and read real type checking
            {"print", Function{{}, {}, Type::Void}},
            {"read", Function{{}, {}, Type::Void}}
        },
        std::find_if(ast_results.rbegin(), ast_results.rend(), [](const NodeResult& r) {
            return r.is_just() && std::holds_alternative<StatementBlock>(
                std::get<NodeResult::Just>(r.value).value->expression);
        })};
        

        if (symbol_table.node_loc == ast_results.rend()) {
            return symbol_table;
        }

        analyze(*symbol_table.node_loc, symbol_table, ast_results);

        return symbol_table;
    }
};