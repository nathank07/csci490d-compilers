#pragma once

#include "../../ast/ast.hpp"
#include "../../utils.hpp"
#include "x86prog.hpp"
#include "x86Instructions.hpp"
#include "../stackAllocator.hpp"
#include <cstddef>
#include <variant>

struct x86Generator : TypeSize<x86Generator> {

private:

    struct EvalResult {
        Type type;
        uint64_t string_chunks = 0;
    };

    std::vector<EvalResult> eval(x86Prog& p, std::unique_ptr<Expression> expr, StackAllocator<x86Generator, int32_t>& vars) {
        std::vector<EvalResult> results;

        const auto visitor = overloads {
            [&](std::monostate&) {},
            [&](Term& t) {
                std::visit(overloads {
                    [&](std::string v) {
                        p.push_var(vars.get(v));
                        results = {{Type::Int4}};
                    },
                    [&](std::u8string v) {
                        uint64_t chunks = p.push_string(v);
                        results = {{Type::StringConstant, chunks}};
                    },
                    [&](long long v) {
                        p.push_value(static_cast<uint32_t>(v));
                        results = {{Type::Int4}};
                    },
                    [&](double v) {
                        assert(false && "doubles unsupported");
                    }
                }, t.v);
            },
            [&](Add& e) {
                eval(p, std::move(*e.left), vars);
                eval(p, std::move(*e.right), vars);
                p.add();
                results = {{Type::Int4}};
            },
            [&](Sub& e) {
                eval(p, std::move(*e.left), vars);
                eval(p, std::move(*e.right), vars);
                p.sub();
                results = {{Type::Int4}};
            },
            [&](Mult& e) {
                eval(p, std::move(*e.left), vars);
                eval(p, std::move(*e.right), vars);
                p.mult();
                results = {{Type::Int4}};
            },
            [&](Div& e) {
                eval(p, std::move(*e.left), vars);
                eval(p, std::move(*e.right), vars);
                p.div();
                results = {{Type::Int4}};
            },
            [&](Mod& e) {
                eval(p, std::move(*e.left), vars);
                eval(p, std::move(*e.right), vars);
                p.mod();
                results = {{Type::Int4}};
            },
            [&](Exp& e) {
                eval(p, std::move(*e.base), vars);
                eval(p, std::move(*e.exponent), vars);
                p.exp();
                results = {{Type::Int4}};
            },
            [&](Negated& e) {
                eval(p, std::move(*e.expression), vars);
                p.neg();
                results = {{Type::Int4}};
            },
            [&](FunctionCall& v) {
                auto arg_types = eval(p, std::move(*v.args), vars);

                if (v.get_ident() == "print") {
                    for (auto& res : arg_types) {
                        switch (res.type) {
                            case Type::Int4: p.print_num(); break;
                            case Type::StringConstant: p.print_str(res.string_chunks); break;
                            default: assert(false && "Cannot print type");
                        }
                    }
                }
            },
            [&](FunctionCallArgList& v) {
                std::vector<EvalResult> next_results;
                if (v.next.is_just()) next_results = eval(p, std::move(*v.next), vars);
                auto val_results = eval(p, std::move(*v.value), vars);
                val_results.insert(val_results.end(), next_results.begin(), next_results.end());
                results = std::move(val_results);
            },
            [&](Declaration& v) {},
            [&](Assign& v) {
                eval(p, std::move(*v.value), vars);
                p.load_var(vars.get(v.get_ident()));
            },
            [&](StatementBlock& v) {
                eval(p, std::move(*v.statements), vars);
            },
            [&](Statements& v) {
                eval(p, std::move(*v.value), vars);
                if (v.next.is_just()) eval(p, std::move(*v.next), vars);
            }
        };

        std::visit(visitor, expr->expression);
        return results;
    }

public:

    static constexpr uint64_t int4_size = 8;

    static x86Prog generate(NodeResult expr, const std::unordered_map<std::string, TypedVar>& symbol_table) {
        x86Prog p;
        StackAllocator<x86Generator, int32_t> stack_vars(symbol_table);
        x86Generator generator;
        generator.eval(p, std::move(*expr), stack_vars);
        p.prog_fn = x86::compose(

            // bp: [caller] sp: [top]
            x86::push(x86::Register::EBP),

            // bp: [top] sp: [top]
            x86::mov64(x86::Register::EBP, x86::Register::ESP),

            // bp: [top] sp: [top after vars]
            x86::sub64(x86::Register::ESP, static_cast<int32_t>(stack_vars.size())),

            // bp: [top] sp: [unknown]
            p.prog_fn,

            // bp: [top] sp: [top]
            x86::mov64(x86::Register::ESP, x86::Register::EBP),

            // bp: [caller]
            x86::pop(x86::Register::EBP),
            x86::ret()
        );
        return p;
    }
};
