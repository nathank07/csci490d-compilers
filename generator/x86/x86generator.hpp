#pragma once

#include "../../ast/ast.hpp"
#include "../../utils.hpp"
#include "x86prog.hpp"
#include "x86Instructions.hpp"
#include "../stackAllocator.hpp"
#include <variant>

struct x86Generator : TypeSize<x86Generator> {

private:

    static void eval(x86Prog& p, std::unique_ptr<Expression> expr, StackAllocator<x86Generator, int32_t>& vars) {

        const auto visitor = overloads {
            [&](std::monostate&) {},
            [&](Term& t) {
                p.push_value(static_cast<uint32_t>(std::get<long long>(t.v)));
            },
            [&](Add& e) {
                eval(p, std::move(*e.left), vars);
                eval(p, std::move(*e.right), vars);
                p.add();
            },
            [&](Sub& e) {
                eval(p, std::move(*e.left), vars);
                eval(p, std::move(*e.right), vars);
                p.sub();
            },
            [&](Mult& e) {
                eval(p, std::move(*e.left), vars);
                eval(p, std::move(*e.right), vars);
                p.mult();
            },
            [&](Div& e) {
                eval(p, std::move(*e.left), vars);
                eval(p, std::move(*e.right), vars);
                p.div();
            },
            [&](Mod& e) {
                eval(p, std::move(*e.left), vars);
                eval(p, std::move(*e.right), vars);
                p.mod();
            },
            [&](Exp& e) {
                eval(p, std::move(*e.base), vars);
                eval(p, std::move(*e.exponent), vars);
                p.exp();
            },
            [&](Negated& e) {
                eval(p, std::move(*e.expression), vars);
                p.neg();
            },
            [&](FunctionCall& v) {
                
            },
            [&](FunctionCallArgList& v) {
                eval(p, std::move(*v.value), vars);
                if (v.next.is_just()) eval(p, std::move(*v.next), vars);
            },
            [&](Declaration& v) {},
            [&](Assign& v) {
                eval(p, std::move(*v.value), vars);
                p.pop_last_into_bp(vars.get(v.get_ident()));
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
    }

public:

    static constexpr uint64_t int4_size = 8;

    static x86Prog generate(NodeResult expr, const std::unordered_map<std::string, TypedVar>& symbol_table) {
        x86Prog p;
        StackAllocator<x86Generator, int32_t> stack_vars(symbol_table);
        eval(p, std::move(*expr), stack_vars);
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