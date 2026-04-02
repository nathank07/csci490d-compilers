#pragma once

#include "../../ast/ast.hpp"
#include "../../utils.hpp"
#include "x86prog.hpp"
#include "x86Instructions.hpp"
#include "../stackAllocator.hpp"
#include <cmath>
#include <cstdint>
#include <variant>

struct x86Generator : TypeSize<x86Generator> {

    static constexpr uint64_t int4_size = 8;

    using Instruction = x86::Instruction;

    template<typename... Args>
    static Instruction compose(Args&&... args) {
        return x86::compose(std::forward<Args>(args)...);
    }

    static Instruction push_imm(uint64_t v) {
        return x86::compose(
            x86::mov_64(x86::Register::EAX, v),
            x86::push(x86::Register::EAX)
        );
    }

private:

    using Stack = StackAllocator<x86Generator>;

    x86Prog& p;
    Stack& stack;

    x86Generator(x86Prog& p_, Stack& stack_) : p(p_), stack(stack_) {}

    void binary_op(auto fold, auto x86_operation, x86::Register push_me = x86::Register::EAX) {
        auto rhs = stack.pop();
        auto lhs = stack.pop();
        auto l = Stack::maybe_value_u64(lhs);
        auto r = Stack::maybe_value_u64(rhs);
        if (l && r) { 
            stack.push_value(fold(*l, *r)); 
            return; 
        }
        load_reg(x86::Register::EAX, lhs);
        load_reg(x86::Register::ECX, rhs);
        p.append(x86::compose(
            x86_operation(x86::Register::EAX, x86::Register::ECX), 
            x86::push(push_me)
        ));
        stack.push_real_value();
    };

    void load_reg(x86::Register r, Stack::StackUnit v) {
        std::visit(overloads {
            [&](ValueUnit& u)    { p.append(x86::mov_64(r, u.literal)); },
            [&](RealUnit&)       { p.append(x86::pop(r)); },
            [&](PointerUnit&)    { p.append(x86::pop(r)); },
            [&](IdentifierUnit& id) { put_var(r, static_cast<int32_t>(stack.get(id.literal))); }
        }, v);
    }

    std::optional<uint64_t> load_reg(x86::Register r) {
        auto top = stack.pop();
        auto v = Stack::maybe_value_u64(top);
        if (v) return v;

        load_reg(r, top);

        return std::nullopt;
    }

    void put_var(x86::Register r, int32_t symbol_table_bp_offset) {
        p.append(x86::mov_memr_32(r, x86::Register::EBP, symbol_table_bp_offset));
    }

    void eval(std::unique_ptr<Expression> expr) {

        const auto visitor = overloads {
            [&](std::monostate&) {},
            [&](Term& t) {
                std::visit(overloads {
                    [&](std::string v) {
                        stack.push_identifier(v);
                    },
                    [&](std::u8string v) {
                        p.append(stack.push_value(v));
                    },
                    [&](long long v) {
                        stack.push_value(static_cast<uint64_t>(v));
                    },
                    [&](double) {
                        assert(false && "doubles unsupported");
                    }
                }, t.v);
            },
            [&](Add& e) {
                eval(std::move(*e.left));
                eval(std::move(*e.right));
                binary_op([](auto a, auto b){ return a + b; },
                          [](auto r1, auto r2){ return x86::add(r1, r2); });

            },
            [&](Sub& e) {
                eval(std::move(*e.left));
                eval(std::move(*e.right));
                binary_op([](auto a, auto b){ return a - b; },
                          [](auto r1, auto r2){ return x86::sub(r1, r2); });
            },
            [&](Mult& e) {
                eval(std::move(*e.left));
                eval(std::move(*e.right));
                binary_op([](auto a, auto b){ return a * b; },
                          [](auto r1, auto r2){ return x86::imul(r1, r2); });
            },
            [&](Div& e) {
                eval(std::move(*e.left));
                eval(std::move(*e.right));
                binary_op([](auto a, auto b){ return a / b; },
                          [](auto, auto r2){ return x86::compose(
                            x86::cdq(), 
                            x86::idiv(r2)); 
                        });
            },
            [&](Mod& e) {
                eval(std::move(*e.left));
                eval(std::move(*e.right));
                binary_op([](auto a, auto b){ return a % b; },
                          [](auto, auto r2){ return x86::compose(
                            x86::cdq(), 
                            x86::idiv(r2)); 
                        }, x86::Register::EDX);
            },
            [&](Exp& e) {
                eval(std::move(*e.base));
                eval(std::move(*e.exponent));
                binary_op([](auto a, auto b){ 
                    return static_cast<uint64_t>(std::pow(a, b)); }, x86::exp);
            },
            [&](Negated& e) {
                eval(std::move(*e.expression));
                auto v = load_reg(x86::Register::EAX);
                if (v) {
                    stack.push_value(std::bit_cast<uint64_t>(-(*v)));
                    return;
                }
                p.append(x86::compose(
                    x86::neg(x86::Register::EAX),
                    x86::push(x86::Register::EAX)
                ));
                stack.push_real_value();
            },
            [&](FunctionCall& v) {
                auto n = v.arg_count();
                if (v.args.is_just()) eval(std::move(*v.args));

                std::stack<Stack::StackUnit> args;
                for (uint8_t i = 0; i < n; i++) args.push(stack.pop());

                eval(std::move(*v.ident));
                auto fn = Stack::assert_ident(stack.pop());
                if (fn == "print") {
                    uint32_t string_chunks = 0;
                    while (!args.empty()) {
                        auto arg = std::move(args.top()); args.pop();
                        std::visit(overloads {
                            [&](ValueUnit& u) {
                                p.append(x86::compose(
                                    x86::mov_64(x86::Register::EAX, u.literal),
                                    x86::print_num_literal(x86::Register::EAX))
                                );
                            },
                            [&](RealUnit&) {
                                p.append(x86::compose(
                                    x86::pop(x86::Register::EAX),
                                    x86::print_num_literal(x86::Register::EAX))
                                );
                            },
                            [&](IdentifierUnit& u) {
                                put_var(x86::Register::EAX, stack.get(u.literal));
                                p.append(x86::print_num_literal(x86::Register::EAX));
                            },
                            [&](PointerUnit& u) {
                                p.append(x86::mov(x86::Register::EAX, x86::Register::EBP));
                                p.append(x86::add(x86::Register::EAX, static_cast<int32_t>(u.bp_offset)));
                                p.append(x86::print_char_addr(x86::Register::EAX));
                                string_chunks += u.size;
                            }
                        }, arg);
                    }

                    if (string_chunks > 0)
                        p.append(x86::add(x86::Register::ESP, 
                                 static_cast<int32_t>(string_chunks * sizeof(uint64_t))));

                } else if (fn == "read") {
                    assert(n == 1 && "read takes exactly one argument");
                    auto arg = std::move(args.top()); args.pop();
                    auto id = stack.assert_ident(arg);
                    p.append(x86::read_int4(x86::Register::EAX));
                    p.append(x86::mov_rmem_64(x86::Register::EBP, x86::Register::EAX, stack.get(id)));
                } else {
                    assert(false && "unknown function");
                }
            },
            [&](FunctionCallArgList& v) {
                eval(std::move(*v.value));
                if (v.next.is_just()) eval(std::move(*v.next));
            },
            [&](Declaration&) {}, // nothing needed because its initalized by StackAllocator
            [&](Assign& v) {
                eval(std::move(*v.ident));
                auto ident_unit = stack.pop();
                auto offset = static_cast<int32_t>(stack.get(stack.assert_ident(ident_unit)));
                eval(std::move(*v.value));
                auto top = stack.pop();
                std::visit(overloads {
                    [&](ValueUnit& u)    { p.append(x86::mov_mem_32(x86::Register::EBP, static_cast<int32_t>(u.literal), offset)); },
                    // pop here and move to memory - you want pop here because
                    // at the moment it's on the evaluation stack, and we're moving it to
                    // the initialized stack with all the other variables
                    [&](RealUnit&)       { p.append(x86::pop(x86::Register::EBP, offset)); },
                    [&](PointerUnit&)    { assert(false && "Variable strings not supported"); },
                    [&](IdentifierUnit& id) {
                        p.append(x86::mov_memr_32(x86::Register::EAX, x86::Register::EBP, stack.get(id.literal)));
                        p.append(x86::mov_rmem_64(x86::Register::EBP, x86::Register::EAX, offset));
                    }
                }, top);
            },
            [&](StatementBlock& v) {
                eval(std::move(*v.statements));
            },
            [&](Statements& v) {
                eval(std::move(*v.value));
                if (v.next.is_just()) eval(std::move(*v.next));
            }
        };

        std::visit(visitor, expr->expression);
    }

public:

    static x86Prog generate(NodeResult expr, const std::unordered_map<std::string, TypedVar>& symbol_table) {
        x86Prog prog;
        Stack stack_vars(symbol_table);
        x86Generator(prog, stack_vars).eval(std::move(*expr));
        prog.prog_fn = x86::compose(

            // bp: [caller] sp: [top]
            x86::push(x86::Register::EBP),

            // bp: [top] sp: [top]
            x86::mov(x86::Register::EBP, x86::Register::ESP),

            // bp: [top] sp: [top after vars]
            x86::sub(x86::Register::ESP, static_cast<int32_t>(stack_vars.symbols_size())),

            // bp: [top] sp: [unknown]
            prog.prog_fn,

            // bp: [top] sp: [top]
            x86::mov(x86::Register::ESP, x86::Register::EBP),

            // bp: [caller]
            x86::pop(x86::Register::EBP),
            x86::ret()
        );
        return prog;
    }
};
